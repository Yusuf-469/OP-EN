// Unit and numerical tests for the C++ core. Each test is registered with
// CTest separately (see CMakeLists.txt): `uew_tests <name>` or `uew_tests all`.
// A tiny harness is used instead of GoogleTest to keep the build offline and
// dependency-free.

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

#include "backprop.h"
#include "batch.h"
#include "geometry_io.h"
#include "hardware.h"
#include "job_registry.h"
#include "report_writers.h"
#include "thermal_fea.h"

using namespace uew;

namespace {

struct Failure { std::string message; };

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) throw Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": CHECK(" #cond ")"}; \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                          \
    do {                                                                               \
        const double va_ = (a), vb_ = (b);                                             \
        if (!(std::abs(va_ - vb_) <= (tol))) {                                         \
            std::ostringstream os_;                                                    \
            os_.precision(17);                                                         \
            os_ << __FILE__ << ":" << __LINE__ << ": " #a " = " << va_ << ", expected " << vb_ << " +/- " << (tol); \
            throw Failure{os_.str()};                                                  \
        }                                                                              \
    } while (0)

// Passes only if `fn` throws E and the message contains `fragment`.
template <typename E>
void checkThrows(const std::function<void()>& fn, const std::string& fragment, int line) {
    try {
        fn();
    } catch (const E& e) {
        if (std::string(e.what()).find(fragment) != std::string::npos) return;
        throw Failure{"line " + std::to_string(line) + ": wrong message: " + e.what()};
    } catch (const std::exception& e) {
        throw Failure{"line " + std::to_string(line) + ": wrong exception type: " + e.what()};
    }
    throw Failure{"line " + std::to_string(line) + ": expected an exception containing '" + fragment + "'"};
}
#define CHECK_THROWS(E, expr, fragment) checkThrows<E>([&] { (void)(expr); }, fragment, __LINE__)

RodProblem taskC(int n) {
    RodProblem p;
    p.elements = n;
    return p;
}

// ---------------------------------------------------------------- thermal FEA
void thermalBoundaryConditions() {
    for (int n : {1, 10, 80}) {
        const RodSolution s = solveRod(taskC(n));
        CHECK(s.temperature_C.size() == static_cast<std::size_t>(n + 1));
        CHECK_NEAR(s.temperature_C.front(), 100.0, 1e-12);
        CHECK_NEAR(s.temperature_C.back(), 20.0, 1e-12);
        CHECK_NEAR(s.x_m.back(), 1.0, 1e-15);
    }
}

// Linear elements on this 1D problem reproduce the exact solution at the nodes
// (nodal superconvergence); the interpolation error lives inside elements.
void thermalNodalExactness() {
    for (int n : {10, 20, 40, 80}) {
        const ErrorMeasures e = measureError(taskC(n), solveRod(taskC(n)));
        CHECK(e.max_nodal_error_C < 1e-9);
    }
}

// Midpoint error of linear interpolation of a quadratic: q h^2 / (8 k).
void thermalMidpointErrorMatchesTheory() {
    for (int n : {10, 20, 40, 80}) {
        const RodProblem p = taskC(n);
        const double h = p.length_m / n;
        const double expected = p.heat_generation_W_per_m3 * h * h / (8.0 * p.conductivity_W_per_mK);
        CHECK_NEAR(measureError(p, solveRod(p)).max_midpoint_error_C, expected, 1e-9);
    }
}

void thermalConvergenceOrderTwo() {
    double previous = 0.0;
    for (int n : {10, 20, 40, 80}) {
        const double err = measureError(taskC(n), solveRod(taskC(n))).max_midpoint_error_C;
        if (previous > 0.0) CHECK_NEAR(std::log2(previous / err), 2.0, 1e-6);
        previous = err;
    }
}

// All heat generated (q A L = 1 W) must leave through the two ends.
void thermalEnergyBalance() {
    const RodSolution s = solveRod(taskC(10));
    CHECK_NEAR(s.heat_out_left_W + s.heat_out_right_W, 1.0, 1e-12);
    CHECK_NEAR(s.heat_out_left_W, 0.42, 1e-12);   // = k A dT/dx(0)
    CHECK_NEAR(s.heat_out_right_W, 0.58, 1e-12);  // = -k A dT/dx(L)
}

void thermalNoSourceIsLinear() {
    RodProblem p = taskC(7);
    p.heat_generation_W_per_m3 = 0.0;
    const RodSolution s = solveRod(p);
    CHECK_NEAR(interpolate(s, 0.3), 100.0 - 80.0 * 0.3, 1e-12);
    CHECK_NEAR(measureError(p, s).max_midpoint_error_C, 0.0, 1e-12);
}

void thermalRejectsInvalidInput() {
    RodProblem p = taskC(10);
    p.conductivity_W_per_mK = 0.0;
    CHECK_THROWS(std::invalid_argument, solveRod(p), "conductivity");
    p = taskC(0);
    CHECK_THROWS(std::invalid_argument, solveRod(p), "Element count");
    p = taskC(10);
    p.length_m = NAN;
    CHECK_THROWS(std::invalid_argument, solveRod(p), "finite");
    p = taskC(10);
    p.temp_right_C = -300.0;
    CHECK_THROWS(std::invalid_argument, solveRod(p), "absolute zero");
}

// --------------------------------------------------------------- batch / pool
void batchGeneratorIsReproducible() {
    const BatchSpec spec{42, 50, 10};
    const auto a = generateBatch(spec), b = generateBatch(spec);
    CHECK(a.size() == 50);
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].conductivity_W_per_mK == b[i].conductivity_W_per_mK);
        CHECK(a[i].conductivity_W_per_mK >= 5.0 && a[i].conductivity_W_per_mK < 50.0);
        CHECK(a[i].heat_generation_W_per_m3 >= 1e3 && a[i].heat_generation_W_per_m3 < 5e4);
    }
    CHECK(generateBatch({43, 50, 10})[0].conductivity_W_per_mK != a[0].conductivity_W_per_mK);
}

void batchSerialParallelAgree() {
    const auto cases = generateBatch({7, 64, 500});
    const BatchResult serial = runSerial(cases);
    for (int workers : {1, 2, 3, 4, 8}) {
        const BatchResult parallel = runParallel(cases, workers);
        CHECK(maxAbsDifference(serial, parallel) <= kAgreementToleranceC);
    }
}

void batchKeepsInputOrder() {
    const auto cases = generateBatch({11, 40, 50});
    const BatchResult parallel = runParallel(cases, 4);
    for (std::size_t i = 0; i < cases.size(); ++i)
        CHECK(parallel[i] == solveRod(cases[i]).temperature_C);
}

void batchCancellation() {
    const auto cases = generateBatch({1, 200, 1000});
    RunControl control;
    control.cancel = true;
    CHECK_THROWS(Cancelled, runParallel(cases, 3, &control), "cancelled");
    CHECK_THROWS(Cancelled, runSerial(cases, &control), "cancelled");
}

void batchWorkerErrorPropagates() {
    auto cases = generateBatch({1, 20, 10});
    cases[13].conductivity_W_per_mK = -1.0;  // invalid case in the middle of the batch
    CHECK_THROWS(std::invalid_argument, runParallel(cases, 4), "conductivity");
}

void batchRejectsInvalidSpec() {
    CHECK_THROWS(std::invalid_argument, generateBatch({1, 0, 10}), "Case count");
    CHECK_THROWS(std::invalid_argument, generateBatch({1, 10, 0}), "Mesh size");
    CHECK_THROWS(std::invalid_argument, generateBatch({1, 100'000, 1'000'000}), "memory");
    CHECK_THROWS(std::invalid_argument, runParallel({}, 0), "Worker count");
}

void benchmarkReportIsConsistent() {
    BenchmarkConfig cfg;
    cfg.batch = {3, 20, 200};
    cfg.repetitions = 3;
    const BenchmarkReport r = runBenchmark(cfg);
    const int expectedPools = std::min(4, static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
    CHECK(r.rows.size() == static_cast<std::size_t>(1 + expectedPools));
    CHECK(r.agreement_ok);
    for (const TimingRow& row : r.rows) {
        CHECK(row.samples_ms.size() == 3);
        CHECK(row.median_ms > 0.0);
        CHECK_NEAR(row.efficiency, row.speedup / row.workers, 1e-12);
    }
}

// A NaN anywhere must fail the agreement check, never be ignored.
void agreementDetectsNan() {
    BatchResult a{{1.0, 2.0}}, b{{1.0, 2.0}};
    CHECK(maxAbsDifference(a, b) == 0.0);
    b[0][1] = NAN;
    CHECK(maxAbsDifference(a, b) > kAgreementToleranceC);
    b[0].pop_back();
    CHECK(maxAbsDifference(a, b) > kAgreementToleranceC);  // shape mismatch
}

// The allocation-free path must give bit-identical results to solveRod.
void workspaceSolveMatchesSolveRod() {
    SolverWorkspace ws;
    std::vector<double> T;
    for (int n : {80, 3, 500}) {  // shrinking then growing reuses the same buffers
        solveTemperatures(taskC(n), ws, T);
        CHECK(T == solveRod(taskC(n)).temperature_C);
    }
}

void medianOddAndEven() {
    CHECK_NEAR(median({3, 1, 2}), 2.0, 0.0);
    CHECK_NEAR(median({4, 1, 3, 2}), 2.5, 0.0);
}

// ----------------------------------------------------------------- STL / CSV
const char* kTetra =
    "solid tetra\n"
    " facet normal 0 0 -1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\n endfacet\n"
    " facet normal 0 -1 0\n  outer loop\n   vertex 0 0 0\n   vertex 0 0 1\n   vertex 1 0 0\n  endloop\n endfacet\n"
    " facet normal -1 0 0\n  outer loop\n   vertex 0 0 0\n   vertex 0 1 0\n   vertex 0 0 1\n  endloop\n endfacet\n"
    " facet normal 1 1 1\n  outer loop\n   vertex 1 0 0\n   vertex 0 0 1\n   vertex 0 1 0\n  endloop\n endfacet\n"
    "endsolid tetra\n";

void stlParsesValidFile() {
    const TriangleMesh m = parseAsciiStl(kTetra);
    CHECK(m.solid_name == "tetra");
    CHECK(m.triangles == 4);
    CHECK(m.vertices.size() == 36);
    CHECK(m.min == (Vec3{0, 0, 0}) && m.max == (Vec3{1, 1, 1}));
    CHECK_NEAR(m.surface_area, 1.5 + std::sqrt(3.0) / 2.0, 1e-12);
    CHECK(m.degenerate_triangles == 0);
}

void stlRejectsMalformed() {
    std::string text = kTetra;
    CHECK_THROWS(ParseError, parseAsciiStl(""), "empty");
    CHECK_THROWS(ParseError, parseAsciiStl("solid x\nendsolid x\n"), "no facets");
    CHECK_THROWS(ParseError, parseAsciiStl(std::string("solid\0binary", 12)), "binary");
    CHECK_THROWS(ParseError, parseAsciiStl("hello"), "must start with 'solid'");

    std::string nan = text;
    nan.replace(nan.find("vertex 1 0 0"), 12, "vertex nan 0 0");
    CHECK_THROWS(ParseError, parseAsciiStl(nan), "Line 5: Vertex component 'nan'");

    std::string quad = text;  // a fourth vertex in the first facet
    quad.insert(quad.find("  endloop"), "   vertex 1 1 0\n");
    CHECK_THROWS(ParseError, parseAsciiStl(quad), "facets must be triangles");

    CHECK_THROWS(ParseError, parseAsciiStl(text.substr(0, text.find("endsolid"))), "end of file");
}

void csvParsesValidFile() {
    const NodalResults r = parseNodalCsv(
        "\xEF\xBB\xBFnode_id,x_m,y_m,z_m,temperature_C\r\n1,0,0,0,20.5\r\n\r\n2, 0.1 ,0,0,80\r\n3,0,0.2,-0.1,50\r\n");
    CHECK(r.nodes.size() == 3);
    CHECK(r.nodes[1].id == 2 && r.nodes[1].x_m == 0.1);
    CHECK_NEAR(r.min_C, 20.5, 0.0);
    CHECK_NEAR(r.max_C, 80.0, 0.0);
    CHECK_NEAR(r.mean_C, (20.5 + 80.0 + 50.0) / 3.0, 1e-12);
    CHECK(r.min_m == (Vec3{0, 0, -0.1}));
}

void csvRejectsMalformed() {
    const std::string h = "node_id,x_m,y_m,z_m,temperature_C\n";
    CHECK_THROWS(ParseError, parseNodalCsv(""), "empty");
    CHECK_THROWS(ParseError, parseNodalCsv(h), "no data rows");
    CHECK_THROWS(ParseError, parseNodalCsv("\n\n"), "blank lines");
    CHECK_THROWS(ParseError, parseNodalCsv("id,x,y,z,T\n1,0,0,0,1\n"), "header must be exactly");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0\n"), "Line 2: expected 5");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0,1,9\n"), "found 6");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0,NaN\n"), "temperature_C value 'NaN'");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,inf,0,0,1\n"), "x_m value 'inf'");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0,1e999\n"), "not a finite number");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0,abc\n"), "not a finite number");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1.5,0,0,0,1\n"), "positive integer");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0,1\n1,1,0,0,1\n"), "Line 3: duplicate node_id 1");
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,0,0,0,-300\n"), "absolute zero");
}

// ------------------------------------------------------------ backpropagation
void backpropInitialValues() {
    const Params p;
    const Sample s;
    const ForwardPass f = forward(p, s);
    CHECK_NEAR(f.h, std::tanh(0.5), 1e-15);
    CHECK_NEAR(f.loss, 0.5 * std::pow(std::tanh(0.5) - 2.0, 2), 1e-15);
    const Gradients g = backward(p, s);
    CHECK_NEAR(g.dc, std::tanh(0.5) - 2.0, 1e-15);
    CHECK_NEAR(g.dv, (std::tanh(0.5) - 2.0) * std::tanh(0.5), 1e-15);
    CHECK_NEAR(g.dw, g.db, 0.0);  // x = 1, so dJ/dw = dJ/dz * x = dJ/db
    CHECK_NEAR(g.dw, -1.2094635, 1e-6);
}

void backpropMatchesFiniteDifference() {
    const Gradients a = backward(Params{}, Sample{});
    const Gradients n = finiteDifference(Params{}, Sample{}, 1e-6);
    CHECK_NEAR(a.dw, n.dw, 1e-5);
    CHECK_NEAR(a.db, n.db, 1e-5);
    CHECK_NEAR(a.dv, n.dv, 1e-5);
    CHECK_NEAR(a.dc, n.dc, 1e-5);
}

void backpropUpdateAndTraining() {
    const Gradients g = backward(Params{}, Sample{});
    const Params next = gradientStep(Params{}, g, 0.1);
    CHECK_NEAR(next.w, 0.5 - 0.1 * g.dw, 1e-15);
    CHECK_NEAR(next.c, 0.0 - 0.1 * g.dc, 1e-15);
    const auto losses = lossCurve(Params{}, Sample{}, 0.1, 100);
    CHECK(losses.size() == 101);
    for (std::size_t i = 1; i < losses.size(); ++i) CHECK(losses[i] < losses[i - 1]);
    CHECK(losses.back() < 1e-3 * losses.front());
}

// -------------------------------------------------------------- hardware mock
void hardwareDiscoverConnectTelemetry() {
    DeviceManager manager;
    const auto devices = manager.discover();
    CHECK(devices.size() == 3);
    CHECK(devices[2]->descriptor().kind == DeviceKind::JtagAdapter);
    for (auto* d : devices) {
        CHECK(!d->capabilities().empty());
        CHECK(d->state() == LinkState::Disconnected);
        CHECK_THROWS(std::logic_error, d->readTelemetry(), "not connected");
        d->connect();
        const TelemetryOutcome out = manager.readWithRetry(d->descriptor().id);
        CHECK(out.ok && out.frame.sequence == 1 && out.frame.fields.size() == 3);
        d->disconnect();
        CHECK(d->state() == LinkState::Disconnected);
    }
    CHECK_THROWS(std::invalid_argument, manager.get("nope"), "Unknown device");
}

void hardwareTimeoutRecovery() {
    DeviceManager manager;
    DeviceAdapter& fpga = manager.get("fpga0");
    fpga.connect();
    fpga.injectTimeouts(1);  // transient: second attempt succeeds
    TelemetryOutcome out = manager.readWithRetry("fpga0");
    CHECK(out.ok && out.attempts.size() == 2 && out.attempts[0].timed_out);

    fpga.injectTimeouts(5);  // persistent: retries exhausted
    out = manager.readWithRetry("fpga0");
    CHECK(!out.ok && fpga.state() == LinkState::Error);
    CHECK_THROWS(std::logic_error, fpga.readTelemetry(), "error state");

    fpga.connect();  // recovery path
    CHECK(manager.readWithRetry("fpga0").ok);
}

// --------------------------------------------------------- job registry (app)
JobSnapshot waitUntilFinished(JobRegistry& jobs, const std::string& id) {
    for (int i = 0; i < 500; ++i) {
        const auto s = jobs.get(id);
        if (s && s->state != "running") return *s;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw Failure{"job " + id + " did not finish within 5 s"};
}

void jobsBusyCancelAndFailure() {
    JobRegistry jobs;
    const std::string slow = jobs.start("slow", 1, [](RunControl& c, const JobRegistry::StageFn&) -> JobOutput {
        while (!c.cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        throw Cancelled();
    });
    CHECK_THROWS(std::logic_error, jobs.start("other", 1, {}), "already running");  // one job at a time
    CHECK(jobs.cancel(slow));
    const JobSnapshot cancelled = waitUntilFinished(jobs, slow);
    CHECK(cancelled.state == "cancelled" && !cancelled.output);  // no partial result published

    const std::string bad = jobs.start("bad", 1, [](RunControl&, const JobRegistry::StageFn&) -> JobOutput {
        throw std::runtime_error("boom");
    });
    const JobSnapshot failed = waitUntilFinished(jobs, bad);
    CHECK(failed.state == "failed" && failed.error == "boom" && !failed.output);

    const std::string good = jobs.start("good", 1, [](RunControl&, const JobRegistry::StageFn& stage) {
        stage("working");
        return JobOutput{"{\"x\":1}", {{"a.csv", "a\n"}}};
    });
    const JobSnapshot done = waitUntilFinished(jobs, good);
    CHECK(done.state == "done" && done.output && done.output->files.count("a.csv") == 1);
    CHECK(!jobs.get("nope") && !jobs.cancel("nope"));
}

// q = 0: FE is exact everywhere, so the observed order is undefined, not noise.
void convergenceOrderUndefinedAtRoundOff() {
    RodProblem p;
    p.heat_generation_W_per_m3 = 0.0;
    const std::string csv = convergenceCsv(runThermalStudy(p, {10, 20}));
    CHECK(csv.find("nan") == std::string::npos && csv.find("inf") == std::string::npos);
    CHECK(csv.back() == '\n' && csv[csv.size() - 2] == ',');  // empty order cell
}

void parsersRejectSignAndRangeAbuse() {
    const std::string h = "node_id,x_m,y_m,z_m,temperature_C\n";
    CHECK_THROWS(ParseError, parseNodalCsv(h + "1,+-5,0,0,1\n"), "x_m value '+-5'");
    CHECK(parseNodalCsv(h + "1,+5,0,0,1\n").nodes[0].x_m == 5.0);
    std::string huge = kTetra;
    huge.replace(huge.find("vertex 1 0 0"), 12, "vertex 1e300 0 0");
    CHECK_THROWS(ParseError, parseAsciiStl(huge), "too large");
}

const std::map<std::string, void (*)()>& registry() {
    static const std::map<std::string, void (*)()> tests = {
        {"thermal_boundary_conditions", thermalBoundaryConditions},
        {"thermal_nodal_exactness", thermalNodalExactness},
        {"thermal_midpoint_error_theory", thermalMidpointErrorMatchesTheory},
        {"thermal_convergence_order", thermalConvergenceOrderTwo},
        {"thermal_energy_balance", thermalEnergyBalance},
        {"thermal_no_source_linear", thermalNoSourceIsLinear},
        {"thermal_rejects_invalid", thermalRejectsInvalidInput},
        {"batch_reproducible", batchGeneratorIsReproducible},
        {"batch_serial_parallel_agree", batchSerialParallelAgree},
        {"batch_input_order", batchKeepsInputOrder},
        {"batch_cancellation", batchCancellation},
        {"batch_worker_error", batchWorkerErrorPropagates},
        {"batch_rejects_invalid", batchRejectsInvalidSpec},
        {"benchmark_report", benchmarkReportIsConsistent},
        {"median", medianOddAndEven},
        {"agreement_detects_nan", agreementDetectsNan},
        {"workspace_solve_matches", workspaceSolveMatchesSolveRod},
        {"stl_valid", stlParsesValidFile},
        {"stl_malformed", stlRejectsMalformed},
        {"csv_valid", csvParsesValidFile},
        {"csv_malformed", csvRejectsMalformed},
        {"backprop_initial_values", backpropInitialValues},
        {"backprop_finite_difference", backpropMatchesFiniteDifference},
        {"backprop_training", backpropUpdateAndTraining},
        {"hardware_discover_connect", hardwareDiscoverConnectTelemetry},
        {"hardware_timeout_recovery", hardwareTimeoutRecovery},
        {"jobs_busy_cancel_failure", jobsBusyCancelAndFailure},
        {"convergence_order_roundoff", convergenceOrderUndefinedAtRoundOff},
        {"parsers_sign_and_range", parsersRejectSignAndRangeAbuse},
    };
    return tests;
}

bool run(const std::string& name, void (*test)()) {
    try {
        test();
        std::cout << "PASS " << name << "\n";
        return true;
    } catch (const Failure& f) {
        std::cout << "FAIL " << name << ": " << f.message << "\n";
    } catch (const std::exception& e) {
        std::cout << "FAIL " << name << ": unexpected exception: " << e.what() << "\n";
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string wanted = argc > 1 ? argv[1] : "all";
    if (wanted == "--list") {
        for (const auto& entry : registry()) std::cout << entry.first << "\n";
        return 0;
    }
    int failures = 0, ran = 0;
    for (const auto& [name, test] : registry()) {
        if (wanted != "all" && wanted != name) continue;
        ++ran;
        failures += run(name, test) ? 0 : 1;
    }
    if (ran == 0) {
        std::cerr << "Unknown test '" << wanted << "'. Use --list.\n";
        return 2;
    }
    std::cout << ran - failures << "/" << ran << " passed\n";
    return failures == 0 ? 0 : 1;
}
