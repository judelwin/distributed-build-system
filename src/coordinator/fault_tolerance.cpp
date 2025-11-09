#include "fault_tolerance.h"
#include <mutex>

namespace coordinator {

bool FaultTolerance::needs_retry(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id) == 0) return false;
    const auto& exec = executions_.at(task_id);
    return exec.is_failed && exec.retry_count < max_retries_;
}

int FaultTolerance::get_retry_count(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id) == 0) return 0;
    return executions_.at(task_id).retry_count;
}

int FaultTolerance::get_failed_worker(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id) == 0) return -1;
    return executions_.at(task_id).worker_idx;
}

void FaultTolerance::record_start(const std::string& task_id, int worker_idx,
                                  int timeout_seconds, const std::string& content_hash, bool is_retry) {
    std::lock_guard<std::mutex> lock(mutex_);
    build::TaskExecution exec;
    exec.task_id = task_id;
    exec.worker_idx = worker_idx;
    exec.start_time = std::chrono::steady_clock::now();
    exec.timeout_seconds = timeout_seconds;
    if (is_retry && executions_.count(task_id)) {
        exec.retry_count = executions_[task_id].retry_count + 1;
    } else {
        exec.retry_count = executions_.count(task_id) ? executions_[task_id].retry_count : 0;
    }
    exec.content_hash = content_hash;
    exec.is_failed = false;
    executions_[task_id] = exec;
}

int FaultTolerance::get_worker_idx(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id) == 0) return -1;
    return executions_.at(task_id).worker_idx;
}

void FaultTolerance::record_failure(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id)) {
        executions_[task_id].is_failed = true;
        executions_[task_id].retry_count++;
    }
}

bool FaultTolerance::is_timed_out(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id) == 0) return false;
    const auto& exec = executions_.at(task_id);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - exec.start_time).count();
    return elapsed > exec.timeout_seconds;
}

bool FaultTolerance::exceeded_max_retries(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id) == 0) return false;
    return executions_.at(task_id).retry_count >= max_retries_;
}

void FaultTolerance::record_success(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (executions_.count(task_id)) {
        executions_[task_id].is_failed = false;
    }
}

} // namespace coordinator

