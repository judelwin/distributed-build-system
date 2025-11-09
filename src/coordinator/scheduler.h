#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>
#include "build_task.h"

namespace coordinator {

// Scheduler for managing build task execution order
class Scheduler {
public:
    // Initialize scheduler with tasks
    void initialize(const std::unordered_map<std::string, build::Task>& tasks);
    
    // Get total number of tasks
    size_t get_total_tasks() const { return total_tasks_; }
    
    // Get next ready tasks (tasks with no pending dependencies)
    std::vector<std::string> get_ready_tasks(size_t max_count);
    
    // Mark task as completed and update dependencies
    void mark_completed(const std::string& task_id);
    
    // Check if all tasks are completed
    bool all_completed(size_t total_tasks) const;
    
    // Check if task is already completed
    bool is_completed(const std::string& task_id) const;
    
    // Get reverse dependency map
    const std::unordered_map<std::string, std::vector<std::string>>& get_reverse_deps() const {
        return reverse_deps_;
    }

private:
    size_t total_tasks_ = 0;
    std::unordered_map<std::string, int> remaining_deps_;
    std::unordered_map<std::string, std::vector<std::string>> reverse_deps_;
    std::queue<std::string> ready_queue_;
    std::unordered_set<std::string> completed_tasks_;
    mutable std::mutex mutex_;
};

} // namespace coordinator

