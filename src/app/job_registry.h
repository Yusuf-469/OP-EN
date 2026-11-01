#pragma once
// Long-running computations (the benchmark) run on their own thread so HTTP
// requests, and therefore the UI, stay responsive. The UI polls progress and
// can cancel; a cancelled or failed job never publishes partial results.

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "batch.h"  // RunControl

namespace uew {

struct JobOutput {
    std::string json;                          // result shown in the UI
    std::map<std::string, std::string> files;  // downloadable exports, e.g. "summary.csv"
};

struct JobSnapshot {
    std::string id, kind, state, stage, error;  // state: running | done | failed | cancelled
    std::size_t completed = 0, total = 0;
    double elapsed_ms = 0.0;
    std::shared_ptr<const JobOutput> output;  // set only when state == done
};

class JobRegistry {
public:
    using StageFn = std::function<void(const std::string&)>;
    using Work = std::function<JobOutput(RunControl&, const StageFn&)>;

    ~JobRegistry();

    // Only one job may run at a time so timings are not disturbed by another
    // job; throws std::logic_error when busy.
    std::string start(const std::string& kind, std::size_t totalUnits, Work work);
    std::optional<JobSnapshot> get(const std::string& id) const;
    bool cancel(const std::string& id);

private:
    struct Job;
    void execute(const std::shared_ptr<Job>& job, const Work& work);

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Job>> jobs_;
    std::vector<std::thread> threads_;
    int nextId_ = 1;
};

}  // namespace uew
