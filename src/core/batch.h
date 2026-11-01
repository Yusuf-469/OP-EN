#pragma once
// Batch of independent thermal solves, run serially or on a bounded worker pool.

#include <atomic>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "thermal_fea.h"

namespace uew {

struct BatchSpec {
    std::uint64_t seed = 20261101;
    int cases = 400;
    int elements = 20'000;
};

constexpr int kMaxBatchCases = 100'000;
constexpr std::size_t kMaxBatchNodes = 50'000'000;  // cases * (elements + 1): ~400 MB of results

// Shared between a running computation and whoever controls it (UI job, CLI).
struct RunControl {
    std::atomic<bool> cancel{false};
    std::atomic<std::size_t> completed{0};  // solves finished so far
};

class Cancelled : public std::runtime_error {
public:
    Cancelled() : std::runtime_error("Computation cancelled by user; no partial results were kept.") {}
};

using BatchResult = std::vector<std::vector<double>>;  // nodal temperatures per case, input order

void validate(const BatchSpec& spec);

// Reproducible on every platform: uses raw mt19937_64 output (fully specified
// by the standard) rather than std::uniform_real_distribution (implementation-defined).
// k in [5, 50] W/(m K), q in [1e3, 5e4] W/m^3, other parameters as in Task C.
std::vector<RodProblem> generateBatch(const BatchSpec& spec);

// Result storage with every slot already sized, so the timed region contains
// no allocation or first-touch page faults.
BatchResult allocateResults(const std::vector<RodProblem>& cases);

// Each solve reuses one SolverWorkspace per thread (no per-solve heap allocation).
void runSerial(const std::vector<RodProblem>& cases, BatchResult& results, RunControl* control = nullptr);

// Fixed pool of `workers` std::threads pulling case indices from an atomic
// counter; each result is written to its own slot, so output order is the input order.
void runParallel(const std::vector<RodProblem>& cases, int workers, BatchResult& results, RunControl* control = nullptr);

// Convenience forms that allocate and return the results.
BatchResult runSerial(const std::vector<RodProblem>& cases, RunControl* control = nullptr);
BatchResult runParallel(const std::vector<RodProblem>& cases, int workers, RunControl* control = nullptr);

double maxAbsDifference(const BatchResult& a, const BatchResult& b);

struct TimingRow {
    std::string variant;  // "serial" or "pool"
    int workers = 1;
    std::vector<double> samples_ms;
    double median_ms = 0.0;
    double speedup = 0.0;     // T_serial / T_p
    double efficiency = 0.0;  // speedup / p
    double max_abs_diff_C = 0.0;
};

struct BenchmarkConfig {
    BatchSpec batch;
    int repetitions = 5;
    int max_workers = 4;  // additionally capped at the logical CPU count
};

struct BenchmarkReport {
    BenchmarkConfig config;
    unsigned logical_cpus = 1;
    std::vector<TimingRow> rows;
    double warmup_ms = 0.0;
    bool agreement_ok = false;  // every pool result within 1e-8 C of serial
};

constexpr double kAgreementToleranceC = 1e-8;

// Warm-up, then `repetitions` timed runs of serial and of the pool for
// p = 1..max_workers. Only the solve loop is inside the timed region.
BenchmarkReport runBenchmark(const BenchmarkConfig& config, RunControl* control = nullptr,
                             const std::function<void(const std::string&)>& onStage = {});

double median(std::vector<double> values);

}  // namespace uew
