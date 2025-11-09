#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include "build_task.h"

namespace coordinator {

// Fault tolerance manager for task retries and failure handling
class FaultTolerance {
public:
    FaultTolerance(int max_retries = 3) : max_retries_(max_retries) {}
    
    // Check if task needs retry
    bool needs_retry(const std::string& task_id) const;
    
    // Get retry count for task
    int get_retry_count(const std::string& task_id) const;
    
    // Get failed worker index for retry avoidance
    int get_failed_worker(const std::string& task_id) const;
    
    // Record task execution start (for new task or retry)
    void record_start(const std::string& task_id, int worker_idx, 
                     int timeout_seconds, const std::string& content_hash, bool is_retry = false);
    
    // Record task failure and increment retry count
    void record_failure(const std::string& task_id);
    
    // Get worker index for a task
    int get_worker_idx(const std::string& task_id) const;
    
    // Check if task has timed out
    bool is_timed_out(const std::string& task_id) const;
    
    // Check if task exceeded max retries
    bool exceeded_max_retries(const std::string& task_id) const;
    
    // Mark task execution as successful
    void record_success(const std::string& task_id);

private:
    std::unordered_map<std::string, build::TaskExecution> executions_;
    int max_retries_;
    mutable std::mutex mutex_;
};

} // namespace coordinator

