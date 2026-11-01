// Command-line front end to the same C++ core the UI uses. Regenerates every
// file in results/ and data/ reproducibly (see scripts/regenerate_results.ps1).

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "batch.h"
#include "geometry_io.h"
#include "hardware.h"
#include "report_writers.h"
#include "sample_data.h"
#include "system_info.h"

using namespace uew;

namespace {

const char* kUsage =
    "Usage: uew_cli <command> [args]\n"
    "  thermal-study <out_dir>                      Task C: N = 10, 20, 40, 80 -> CSV files\n"
    "  benchmark <out_dir> [cases] [mesh] [seed]    Task D: serial vs worker pool timings\n"
    "  backprop <out_dir>                           Task F: gradients, check, loss curve\n"
    "  sysinfo                                      CPU / cores / RAM / compiler as JSON\n"
    "  parse-stl <file>  |  parse-csv <file>        Validate a file and print a summary\n"
    "  make-samples <data_dir>                      Write the self-created sample files\n"
    "  make-large-stl <file> <triangles>  |  make-large-csv <file> <nodes>\n"
    "  hw-demo                                      Simulated device session incl. timeout recovery\n";

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open '" + path + "'.");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out || !(out << content)) throw std::runtime_error("Cannot write '" + path + "'.");
    std::cout << "wrote " << path << "\n";
}

int intArg(const std::vector<std::string>& args, std::size_t i, int fallback) {
    return i < args.size() ? std::stoi(args[i]) : fallback;
}

void thermalStudy(const std::string& dir) {
    const auto study = runThermalStudy(RodProblem{}, {10, 20, 40, 80});
    writeFile(dir + "/thermal_nodes.csv", thermalNodesCsv(study));
    writeFile(dir + "/thermal_midpoints.csv", thermalMidpointsCsv(study));
    writeFile(dir + "/thermal_convergence.csv", convergenceCsv(study));
    writeFile(dir + "/thermal_error_profile.csv", errorProfileCsv(study, 8));
    std::cout << convergenceCsv(study);
}

void benchmark(const std::vector<std::string>& args) {
    BenchmarkConfig cfg;
    cfg.batch.cases = intArg(args, 3, cfg.batch.cases);
    cfg.batch.elements = intArg(args, 4, cfg.batch.elements);
    if (args.size() > 5) cfg.batch.seed = std::stoull(args[5]);
    const BenchmarkReport report = runBenchmark(cfg, nullptr, [](const std::string& s) { std::cerr << s << "\n"; });

    JsonWriter json;
    json.beginObject().key("system");
    writeSystemInfo(json, querySystemInfo());
    json.key("benchmark");
    writeBenchmark(json, report);
    json.endObject();
    writeFile(args[2] + "/benchmark.json", json.str());
    writeFile(args[2] + "/benchmark_summary.csv", benchmarkSummaryCsv(report));
    writeFile(args[2] + "/benchmark_samples.csv", benchmarkSamplesCsv(report));
    std::cout << benchmarkSummaryCsv(report) << "agreement_ok=" << (report.agreement_ok ? "true" : "false") << "\n";
}

void backprop(const std::string& dir) {
    const BackpropStudy study = runBackpropStudy();
    JsonWriter json;
    writeBackprop(json, study);
    writeFile(dir + "/backprop.json", json.str());
    writeFile(dir + "/backprop_gradients.csv", backpropGradientCsv(study));
    writeFile(dir + "/backprop_loss.csv", backpropLossCsv(study));
    std::cout << backpropGradientCsv(study);
}

template <typename Fn>
void timedParse(const std::string& path, Fn&& parse) {
    const std::string text = readFile(path);
    const auto start = std::chrono::steady_clock::now();
    const std::string summary = parse(text);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << summary << " bytes=" << text.size() << " parse_ms=" << ms << "\n";
}

void hardwareDemo() {
    DeviceManager manager;
    for (DeviceAdapter* d : manager.discover()) {
        const auto desc = d->descriptor();
        std::cout << "[SIMULATED] found " << desc.id << " (" << toString(desc.kind) << "): " << desc.model << "\n";
        d->connect();
        const auto out = manager.readWithRetry(desc.id);
        std::cout << "  telemetry #" << out.frame.sequence << ":";
        for (const auto& f : out.frame.fields) std::cout << " " << f.name << "=" << f.value << " " << f.unit;
        std::cout << "\n";
    }
    manager.get("jtag0").injectTimeouts(1);
    for (const auto& a : manager.readWithRetry("jtag0").attempts)
        std::cout << "  jtag0 attempt " << a.attempt << ": " << a.message << "\n";
}

int run(const std::vector<std::string>& args) {
    const std::string cmd = args.size() > 1 ? args[1] : "";
    auto need = [&](std::size_t n) {
        if (args.size() < n) throw std::invalid_argument(std::string("Missing argument.\n") + kUsage);
    };
    if (cmd == "thermal-study") { need(3); thermalStudy(args[2]); }
    else if (cmd == "benchmark") { need(3); benchmark(args); }
    else if (cmd == "backprop") { need(3); backprop(args[2]); }
    else if (cmd == "sysinfo") {
        JsonWriter json;
        writeSystemInfo(json, querySystemInfo());
        std::cout << json.str() << "\n";
    } else if (cmd == "parse-stl") {
        need(3);
        timedParse(args[2], [](const std::string& t) {
            const auto m = parseAsciiStl(t);
            return "OK triangles=" + std::to_string(m.triangles);
        });
    } else if (cmd == "parse-csv") {
        need(3);
        timedParse(args[2], [](const std::string& t) {
            const auto r = parseNodalCsv(t);
            return "OK nodes=" + std::to_string(r.nodes.size());
        });
    } else if (cmd == "make-samples") {
        need(3);
        writeHeatSinkStl(args[2] + "/heat_sink.stl");
        writeHeatSinkCsv(args[2] + "/heat_sink_temperature_synthetic.csv");
    } else if (cmd == "make-large-stl") { need(4); writeLargeStl(args[2], std::stoull(args[3])); }
    else if (cmd == "make-large-csv") { need(4); writeLargeCsv(args[2], std::stoull(args[3])); }
    else if (cmd == "hw-demo") hardwareDemo();
    else { std::cerr << kUsage; return 2; }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (!disablePowerThrottling()) std::cerr << "note: could not opt out of power throttling\n";
        return run(std::vector<std::string>(argv, argv + argc));
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
