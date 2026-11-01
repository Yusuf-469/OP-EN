#include "job_registry.h"

#include <chrono>
#include <stdexcept>

namespace uew {

struct JobRegistry::Job {
    std::string id, kind;
    std::size_t total = 0;
    RunControl control;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    double finished_ms = -1.0;

    // Guarded by JobRegistry::mutex_.
    std::string state = "running", stage = "Queued", error;
    std::shared_ptr<const JobOutput> output;
};

JobRegistry::~JobRegistry() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : jobs_) entry.second->control.cancel = true;
    }
    for (auto& t : threads_) t.join();
}

std::string JobRegistry::start(const std::string& kind, std::size_t totalUnits, Work work) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : jobs_)
        if (entry.second->state == "running")
            throw std::logic_error("A " + entry.second->kind + " job is already running. Cancel it or wait for it to finish.");

    auto job = std::make_shared<Job>();
    job->id = "job" + std::to_string(nextId_++);
    job->kind = kind;
    job->total = totalUnits;
    jobs_[job->id] = job;
    threads_.emplace_back([this, job, work = std::move(work)] { execute(job, work); });
    return job->id;
}

void JobRegistry::execute(const std::shared_ptr<Job>& job, const Work& work) {
    const StageFn setStage = [&](const std::string& stage) {
        std::lock_guard<std::mutex> lock(mutex_);
        job->stage = stage;
    };
    std::string state = "done", error;
    std::shared_ptr<const JobOutput> output;
    try {
        output = std::make_shared<const JobOutput>(work(job->control, setStage));
    } catch (const Cancelled& e) {
        state = "cancelled";
        error = e.what();
    } catch (const std::exception& e) {
        state = "failed";
        error = e.what();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    job->state = state;
    job->error = error;
    job->output = output;  // published atomically with the final state
    job->finished_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - job->started).count();
}

std::optional<JobSnapshot> JobRegistry::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) return std::nullopt;
    const Job& job = *it->second;
    JobSnapshot s;
    s.id = job.id;
    s.kind = job.kind;
    s.state = job.state;
    s.stage = job.stage;
    s.error = job.error;
    s.completed = job.control.completed.load();
    s.total = job.total;
    s.elapsed_ms = job.finished_ms >= 0 ? job.finished_ms
                                        : std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - job.started).count();
    s.output = job.output;
    return s;
}

bool JobRegistry::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) return false;
    it->second->control.cancel = true;
    return true;
}

}  // namespace uew
