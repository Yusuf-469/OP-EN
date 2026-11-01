#include "batch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <mutex>
#include <random>
#include <thread>

namespace uew {
namespace {

double uniform01(std::mt19937_64& gen) {
    return static_cast<double>(gen() >> 11) * 0x1.0p-53;  // 53 random bits -> [0, 1)
}

void solveOne(const RodProblem& problem, SolverWorkspace& workspace, std::vector<double>& out, RunControl* control) {
    if (control && control->cancel.load(std::memory_order_relaxed)) throw Cancelled();
    solveTemperatures(problem, workspace, out);
    if (control) control->completed.fetch_add(1, std::memory_order_relaxed);
}

template <typename Fn>
double timeMs(Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

}  // namespace

void validate(const BatchSpec& spec) {
    if (spec.cases < 1 || spec.cases > kMaxBatchCases)
        throw std::invalid_argument("Case count must be between 1 and " + std::to_string(kMaxBatchCases) + ".");
    if (spec.elements < 1 || spec.elements > kMaxElements)
        throw std::invalid_argument("Mesh size must be between 1 and " + std::to_string(kMaxElements) + " elements.");
    const auto nodes = static_cast<std::size_t>(spec.cases) * static_cast<std::size_t>(spec.elements + 1);
    if (nodes > kMaxBatchNodes)
        throw std::invalid_argument("cases x (elements + 1) must not exceed " + std::to_string(kMaxBatchNodes) +
                                    " to keep result memory bounded; reduce cases or mesh size.");
}

std::vector<RodProblem> generateBatch(const BatchSpec& spec) {
    validate(spec);
    std::mt19937_64 gen(spec.seed);
    std::vector<RodProblem> cases(static_cast<std::size_t>(spec.cases));
    for (auto& c : cases) {
        c.elements = spec.elements;
        c.conductivity_W_per_mK = 5.0 + 45.0 * uniform01(gen);
        c.heat_generation_W_per_m3 = 1e3 + 4.9e4 * uniform01(gen);
    }
    return cases;
}

BatchResult allocateResults(const std::vector<RodProblem>& cases) {
    BatchResult results;
    results.reserve(cases.size());
    for (const RodProblem& c : cases) results.emplace_back(static_cast<std::size_t>(c.elements) + 1, 0.0);
    return results;
}

void runSerial(const std::vector<RodProblem>& cases, BatchResult& results, RunControl* control) {
    results.resize(cases.size());
    SolverWorkspace workspace;
    for (std::size_t i = 0; i < cases.size(); ++i) solveOne(cases[i], workspace, results[i], control);
}

BatchResult runSerial(const std::vector<RodProblem>& cases, RunControl* control) {
    BatchResult results = allocateResults(cases);
    runSerial(cases, results, control);
    return results;
}

BatchResult runParallel(const std::vector<RodProblem>& cases, int workers, RunControl* control) {
    BatchResult results = allocateResults(cases);
    runParallel(cases, workers, results, control);
    return results;
}

void runParallel(const std::vector<RodProblem>& cases, int workers, BatchResult& results, RunControl* control) {
    if (workers < 1) throw std::invalid_argument("Worker count must be at least 1.");
    results.resize(cases.size());  // sized before any thread starts; threads never resize the outer vector
    std::atomic<std::size_t> next{0};
    std::atomic<bool> stop{false};
    std::exception_ptr firstError;
    std::mutex errorMutex;

    auto worker = [&] {
        SolverWorkspace workspace;  // one per thread, reused for every case it takes
        try {
            for (std::size_t i = next.fetch_add(1); i < cases.size() && !stop; i = next.fetch_add(1))
                solveOne(cases[i], workspace, results[i], control);
        } catch (...) {
            // An exception escaping a std::thread would terminate the process;
            // keep the first one and stop the other workers instead.
            std::lock_guard<std::mutex> lock(errorMutex);
            if (!firstError) firstError = std::current_exception();
            stop = true;
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(workers));
    for (int w = 0; w < workers; ++w) pool.emplace_back(worker);
    for (auto& t : pool) t.join();
    if (firstError) std::rethrow_exception(firstError);
}

double maxAbsDifference(const BatchResult& a, const BatchResult& b) {
    if (a.size() != b.size()) return INFINITY;
    double worst = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].size() != b[i].size()) return INFINITY;
        for (std::size_t j = 0; j < a[i].size(); ++j) {
            const double d = std::abs(a[i][j] - b[i][j]);
            if (std::isnan(d)) return INFINITY;  // std::max would silently drop a NaN
            worst = std::max(worst, d);
        }
    }
    return worst;
}

double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    return values.size() % 2 ? values[mid] : 0.5 * (values[mid - 1] + values[mid]);
}

BenchmarkReport runBenchmark(const BenchmarkConfig& config, RunControl* control,
                             const std::function<void(const std::string&)>& onStage) {
    if (config.repetitions < 1 || config.repetitions > 20)
        throw std::invalid_argument("Repetitions must be between 1 and 20.");
    const auto stage = [&](const std::string& s) { if (onStage) onStage(s); };

    BenchmarkReport report;
    report.config = config;
    report.logical_cpus = std::max(1u, std::thread::hardware_concurrency());
    const int maxWorkers = std::max(1, std::min({config.max_workers, 4, static_cast<int>(report.logical_cpus)}));
    report.config.max_workers = maxWorkers;

    stage("Generating cases");
    const std::vector<RodProblem> cases = generateBatch(config.batch);  // outside every timed region

    stage("Warm-up (untimed)");
    BatchResult reference = allocateResults(cases);
    BatchResult out = allocateResults(cases);  // reused by every timed run: no allocation inside timing
    report.warmup_ms = timeMs([&] { runSerial(cases, reference, control); });
    runParallel(cases, maxWorkers, out, control);

    // NaN-fill before each run so a slot the run failed to write cannot pass the agreement check.
    const auto poison = [&] { for (auto& field : out) std::fill(field.begin(), field.end(), NAN); };

    TimingRow serial;
    serial.variant = "serial";
    for (int r = 0; r < config.repetitions; ++r) {
        stage("Serial run " + std::to_string(r + 1) + "/" + std::to_string(config.repetitions));
        poison();
        serial.samples_ms.push_back(timeMs([&] { runSerial(cases, out, control); }));
        serial.max_abs_diff_C = std::max(serial.max_abs_diff_C, maxAbsDifference(reference, out));
    }
    serial.median_ms = median(serial.samples_ms);
    serial.speedup = serial.efficiency = 1.0;
    report.rows.push_back(serial);

    report.agreement_ok = true;
    for (int p = 1; p <= maxWorkers; ++p) {
        TimingRow row;
        row.variant = "pool";
        row.workers = p;
        for (int r = 0; r < config.repetitions; ++r) {
            stage("Pool p=" + std::to_string(p) + " run " + std::to_string(r + 1) + "/" +
                  std::to_string(config.repetitions));
            poison();
            row.samples_ms.push_back(timeMs([&] { runParallel(cases, p, out, control); }));
            row.max_abs_diff_C = std::max(row.max_abs_diff_C, maxAbsDifference(reference, out));
        }
        row.median_ms = median(row.samples_ms);
        row.speedup = serial.median_ms / row.median_ms;
        row.efficiency = row.speedup / p;
        report.agreement_ok = report.agreement_ok && row.max_abs_diff_C <= kAgreementToleranceC;
        report.rows.push_back(row);
    }
    stage("Done");
    return report;
}

}  // namespace uew
