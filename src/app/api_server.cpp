#include "api_server.h"

#include <httplib.h>

#include <atomic>
#include <charconv>
#include <cmath>
#include <mutex>
#include <sstream>
#include <thread>

#include "batch.h"
#include "geometry_io.h"
#include "hardware.h"
#include "job_registry.h"
#include "report_writers.h"
#include "system_info.h"

namespace uew {
namespace {

constexpr const char* kJson = "application/json";

void sendJson(httplib::Response& res, const std::string& body, int status = 200) {
    res.status = status;
    res.set_content(body, kJson);
}

void sendError(httplib::Response& res, int status, const std::string& message) {
    JsonWriter json;
    json.beginObject().field("ok", false).field("error", message).endObject();
    sendJson(res, json.str(), status);
}

// Maps exception types to HTTP status codes: bad input 400, conflicting
// state 409, anything else 500. The message is always shown to the user.
template <typename Fn>
void guarded(httplib::Response& res, Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument& e) {
        sendError(res, 400, e.what());
    } catch (const ParseError& e) {
        sendError(res, 400, e.what());
    } catch (const std::logic_error& e) {
        sendError(res, 409, e.what());
    } catch (const std::exception& e) {
        sendError(res, 500, std::string("Unexpected error: ") + e.what());
    }
}

double numberParam(const httplib::Request& req, const char* name, double fallback) {
    if (!req.has_param(name)) return fallback;
    const std::string text = req.get_param_value(name);
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || ec != std::errc() || ptr != text.data() + text.size() || !std::isfinite(value))
        throw std::invalid_argument(std::string("Parameter '") + name + "' must be a finite number, got '" + text + "'.");
    return value;
}

int intParam(const httplib::Request& req, const char* name, int fallback) {
    const double v = numberParam(req, name, fallback);
    if (v != std::floor(v) || std::abs(v) > 2e9)
        throw std::invalid_argument(std::string("Parameter '") + name + "' must be a whole number.");
    return static_cast<int>(v);
}

RodProblem rodFromRequest(const httplib::Request& req) {
    RodProblem p;
    p.length_m = numberParam(req, "L", p.length_m);
    p.area_m2 = numberParam(req, "A", p.area_m2);
    p.conductivity_W_per_mK = numberParam(req, "k", p.conductivity_W_per_mK);
    p.heat_generation_W_per_m3 = numberParam(req, "q", p.heat_generation_W_per_m3);
    p.temp_left_C = numberParam(req, "T0", p.temp_left_C);
    p.temp_right_C = numberParam(req, "TL", p.temp_right_C);
    validate(p);  // reports the parameter by name before any mesh is solved
    return p;
}

std::vector<int> meshesFromRequest(const httplib::Request& req) {
    std::vector<int> meshes;
    std::stringstream list(req.has_param("N") ? req.get_param_value("N") : "10,20,40,80");
    for (std::string item; std::getline(list, item, ',');) {
        int n = 0;
        const auto [ptr, ec] = std::from_chars(item.data(), item.data() + item.size(), n);
        if (ec != std::errc() || ptr != item.data() + item.size() || n < 1 || n > 100'000)
            throw std::invalid_argument("Each mesh size N must be a whole number from 1 to 100000; got '" + item + "'.");
        meshes.push_back(n);
    }
    if (meshes.empty() || meshes.size() > 8) throw std::invalid_argument("Give between 1 and 8 mesh sizes.");
    return meshes;
}

void writeDevice(JsonWriter& json, const DeviceAdapter& d) {
    const DeviceDescriptor desc = d.descriptor();
    json.beginObject()
        .field("id", desc.id)
        .field("kind", toString(desc.kind))
        .field("model", desc.model)
        .field("host_link", desc.host_link)
        .field("io_voltage", desc.io_voltage)
        .field("state", toString(d.state()))
        .field("simulated", true)
        .key("capabilities").beginArray();
    for (const auto& c : d.capabilities())
        json.beginObject().field("name", c.name).field("detail", c.detail).endObject();
    json.endArray().endObject();
}

}  // namespace

struct ApiServer::Impl {
    std::string webRoot, sampleRoot;
    Logger log;
    Clock sinceLaunch;
    httplib::Server http;
    std::thread thread;
    int port = 0;
    double serverReadyMs = -1.0;
    std::atomic<double> uiReadyMs{-1.0};
    JobRegistry jobs;
    std::mutex hardwareMutex;
    DeviceManager hardware;

    void routes();
    void thermalRoutes();
    void geometryRoutes();
    void benchmarkRoutes();
    void hardwareRoutes();
};

void ApiServer::Impl::routes() {
    http.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        sendJson(res, R"({"ok":true,"app":"Unified Engineering Workbench","version":"1.0.0"})");
    });
    http.Post("/api/ui-ready", [this](const httplib::Request&, httplib::Response& res) {
        double expected = -1.0;
        uiReadyMs.compare_exchange_strong(expected, sinceLaunch());  // first page load only
        sendJson(res, R"({"ok":true})");
    });
    http.Get("/api/system", [this](const httplib::Request&, httplib::Response& res) {
        guarded(res, [&] {
            JsonWriter json;
            json.beginObject().field("ok", true).key("system");
            writeSystemInfo(json, querySystemInfo());
            json.field("launch_to_server_ready_ms", serverReadyMs)
                .field("launch_to_ui_ready_ms", uiReadyMs.load())
                .endObject();
            sendJson(res, json.str());
        });
    });
    http.Get("/api/backprop", [](const httplib::Request&, httplib::Response& res) {
        guarded(res, [&] {
            JsonWriter json;
            writeBackprop(json, runBackpropStudy());
            sendJson(res, json.str());
        });
    });
    thermalRoutes();
    geometryRoutes();
    benchmarkRoutes();
    hardwareRoutes();
}

void ApiServer::Impl::thermalRoutes() {
    http.Get("/api/thermal/study", [](const httplib::Request& req, httplib::Response& res) {
        guarded(res, [&] {
            JsonWriter json;
            writeThermalStudy(json, runThermalStudy(rodFromRequest(req), meshesFromRequest(req)));
            sendJson(res, json.str());
        });
    });
    http.Get("/api/thermal/export", [](const httplib::Request& req, httplib::Response& res) {
        guarded(res, [&] {
            const auto study = runThermalStudy(rodFromRequest(req), meshesFromRequest(req));
            const std::string which = req.has_param("file") ? req.get_param_value("file") : "nodes";
            std::string csv;
            if (which == "nodes") csv = thermalNodesCsv(study);
            else if (which == "midpoints") csv = thermalMidpointsCsv(study);
            else if (which == "convergence") csv = convergenceCsv(study);
            else throw std::invalid_argument("file must be nodes, midpoints or convergence.");
            res.set_header("Content-Disposition", "attachment; filename=\"thermal_" + which + ".csv\"");
            res.set_content(csv, "text/csv");
        });
    });
}

void ApiServer::Impl::geometryRoutes() {
    http.Post("/api/geometry/stl", [](const httplib::Request& req, httplib::Response& res) {
        guarded(res, [&] {
            JsonWriter json;
            writeMesh(json, parseAsciiStl(req.body));
            sendJson(res, json.str());
        });
    });
    http.Post("/api/results/csv", [](const httplib::Request& req, httplib::Response& res) {
        guarded(res, [&] {
            JsonWriter json;
            writeNodalResults(json, parseNodalCsv(req.body));
            sendJson(res, json.str());
        });
    });
}

void ApiServer::Impl::benchmarkRoutes() {
    http.Post("/api/benchmark", [this](const httplib::Request& req, httplib::Response& res) {
        guarded(res, [&] {
            BenchmarkConfig cfg;
            cfg.batch.cases = intParam(req, "cases", cfg.batch.cases);
            cfg.batch.elements = intParam(req, "mesh", cfg.batch.elements);
            cfg.batch.seed = static_cast<std::uint64_t>(intParam(req, "seed", static_cast<int>(cfg.batch.seed)));
            cfg.max_workers = intParam(req, "workers", cfg.max_workers);
            if (cfg.max_workers < 1 || cfg.max_workers > 4) throw std::invalid_argument("Workers must be 1 to 4.");
            validate(cfg.batch);
            const int pools = std::min({cfg.max_workers, 4, static_cast<int>(std::max(1u, std::thread::hardware_concurrency()))});
            const std::size_t solvesPerPass = static_cast<std::size_t>(cfg.batch.cases);
            const std::size_t total = solvesPerPass * static_cast<std::size_t>(2 + cfg.repetitions * (1 + pools));

            const std::string id = jobs.start("benchmark", total, [cfg](RunControl& control, const JobRegistry::StageFn& stage) {
                const BenchmarkReport report = runBenchmark(cfg, &control, stage);
                JsonWriter json;
                json.beginObject().key("system");
                writeSystemInfo(json, querySystemInfo());
                json.key("benchmark");
                writeBenchmark(json, report);
                json.endObject();
                return JobOutput{json.str(), {{"benchmark_summary.csv", benchmarkSummaryCsv(report)},
                                              {"benchmark_samples.csv", benchmarkSamplesCsv(report)},
                                              {"benchmark.json", json.str()}}};
            });
            sendJson(res, R"({"ok":true,"job_id":")" + id + "\"}", 202);
        });
    });
    http.Get(R"(/api/jobs/(\w+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto job = jobs.get(req.matches[1]);
        if (!job) return sendError(res, 404, "Unknown job id.");
        JsonWriter json;
        json.beginObject().field("id", job->id).field("kind", job->kind).field("state", job->state)
            .field("stage", job->stage).field("completed", job->completed).field("total", job->total)
            .field("elapsed_ms", job->elapsed_ms).field("error", job->error);
        if (job->output) json.key("result").raw(job->output->json);
        json.endObject();
        sendJson(res, json.str());
    });
    http.Post(R"(/api/jobs/(\w+)/cancel)", [this](const httplib::Request& req, httplib::Response& res) {
        if (!jobs.cancel(req.matches[1])) return sendError(res, 404, "Unknown job id.");
        sendJson(res, R"({"ok":true})");
    });
    http.Get(R"(/api/jobs/(\w+)/files/([\w.]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto job = jobs.get(req.matches[1]);
        if (!job || !job->output) return sendError(res, 404, "No finished job with that id.");
        const auto file = job->output->files.find(req.matches[2]);
        if (file == job->output->files.end()) return sendError(res, 404, "Unknown export file.");
        res.set_header("Content-Disposition", "attachment; filename=\"" + file->first + "\"");
        res.set_content(file->second, file->first.size() > 4 && file->first.substr(file->first.size() - 4) == ".csv"
                                          ? "text/csv" : kJson);
    });
}

void ApiServer::Impl::hardwareRoutes() {
    http.Get("/api/hardware", [this](const httplib::Request&, httplib::Response& res) {
        guarded(res, [&] {
            std::lock_guard<std::mutex> lock(hardwareMutex);
            JsonWriter json;
            json.beginObject().field("simulated", true).key("devices").beginArray();
            for (const DeviceAdapter* d : hardware.discover()) writeDevice(json, *d);
            json.endArray().endObject();
            sendJson(res, json.str());
        });
    });
    http.Post(R"(/api/hardware/(\w+)/([\w-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        guarded(res, [&] {
            std::lock_guard<std::mutex> lock(hardwareMutex);
            DeviceAdapter& device = hardware.get(req.matches[1]);
            const std::string action = req.matches[2];
            JsonWriter json;
            json.beginObject().field("ok", true).field("action", action);
            if (action == "connect") device.connect();
            else if (action == "disconnect") device.disconnect();
            else if (action == "inject-timeout") device.injectTimeouts(1);
            else if (action == "inject-persistent-timeout") device.injectTimeouts(DeviceManager::kMaxAttempts + 2);
            else if (action == "telemetry") {
                const TelemetryOutcome out = hardware.readWithRetry(device.descriptor().id);
                json.field("telemetry_ok", out.ok).key("attempts").beginArray();
                for (const auto& a : out.attempts)
                    json.beginObject().field("attempt", a.attempt).field("timed_out", a.timed_out)
                        .field("message", a.message).endObject();
                json.endArray();
                if (out.ok) {
                    json.key("frame").beginObject().field("device_id", out.frame.device_id)
                        .field("sequence", static_cast<int>(out.frame.sequence)).key("fields").beginArray();
                    for (const auto& f : out.frame.fields)
                        json.beginObject().field("name", f.name).field("value", f.value).field("unit", f.unit).endObject();
                    json.endArray().endObject();
                }
            } else throw std::invalid_argument("Unknown hardware action '" + action + "'.");
            json.key("device");
            writeDevice(json, device);
            json.endObject();
            sendJson(res, json.str());
        });
    });
}

ApiServer::ApiServer(std::string webRoot, std::string sampleRoot, Logger log, Clock msSinceLaunch)
    : impl_(std::make_unique<Impl>()) {
    impl_->webRoot = std::move(webRoot);
    impl_->sampleRoot = std::move(sampleRoot);
    impl_->log = std::move(log);
    impl_->sinceLaunch = std::move(msSinceLaunch);
}

ApiServer::~ApiServer() { stop(); }

int ApiServer::start() {
    Impl& s = *impl_;
    if (!s.http.set_mount_point("/", s.webRoot))
        throw std::runtime_error("Web UI folder not found: " + s.webRoot);
    if (!s.sampleRoot.empty()) s.http.set_mount_point("/samples", s.sampleRoot);
    s.http.set_payload_max_length(kMaxInputBytes + 1024 * 1024);
    // Reject cross-site requests: only pages served by this server may call the API.
    s.http.set_pre_routing_handler([&s](const httplib::Request& req, httplib::Response& res) {
        const std::string origin = req.get_header_value("Origin");
        if (!origin.empty() && origin != "http://127.0.0.1:" + std::to_string(s.port)) {
            sendError(res, 403, "Cross-origin request refused.");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    s.http.set_logger([&s](const httplib::Request& req, const httplib::Response& res) {
        s.log(req.method + " " + req.path + " -> " + std::to_string(res.status));
    });
    s.routes();
    s.port = s.http.bind_to_any_port("127.0.0.1");
    if (s.port <= 0) throw std::runtime_error("Could not bind a local port on 127.0.0.1.");
    s.thread = std::thread([&s] { s.http.listen_after_bind(); });
    s.http.wait_until_ready();
    s.serverReadyMs = s.sinceLaunch();
    s.log("Server listening on http://127.0.0.1:" + std::to_string(s.port) + " (launch-to-server-ready " +
          std::to_string(s.serverReadyMs) + " ms)");
    return s.port;
}

void ApiServer::stop() {
    if (!impl_ || !impl_->thread.joinable()) return;
    impl_->http.stop();
    impl_->thread.join();
}

}  // namespace uew
