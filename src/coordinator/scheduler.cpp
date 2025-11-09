#include "scheduler.h"
#include <mutex>

namespace coordinator {

void Scheduler::initialize(const std::unordered_map<std::string, build::Task>& tasks) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    total_tasks_ = tasks.size();
    remaining_deps_.clear();
    reverse_deps_.clear();
    completed_tasks_.clear();
    
    // Clear queue and rebuild
    while (!ready_queue_.empty()) {
        ready_queue_.pop();
    }
    
    // Build reverse dependency graph
    for (const auto& [id, task] : tasks) {
        remaining_deps_[id] = task.in_degree;
        if (task.in_degree == 0) {
            ready_queue_.push(id);
        }
        for (const auto& dep_id : task.depends_on) {
            reverse_deps_[dep_id].push_back(id);
        }
    }
}

std::vector<std::string> Scheduler::get_ready_tasks(size_t max_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    while (!ready_queue_.empty() && result.size() < max_count) {
        result.push_back(ready_queue_.front());
        ready_queue_.pop();
    }
    
    return result;
}

void Scheduler::mark_completed(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    completed_tasks_.insert(task_id);
    
    // Mark dependent tasks as ready if all dependencies completed
    if (reverse_deps_.count(task_id)) {
        for (const auto& dependent_id : reverse_deps_[task_id]) {
            remaining_deps_[dependent_id]--;
            if (remaining_deps_[dependent_id] == 0 && 
                completed_tasks_.count(dependent_id) == 0) {
                ready_queue_.push(dependent_id);
            }
        }
    }
}

bool Scheduler::all_completed(size_t total_tasks) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_tasks_.size() >= total_tasks;
}

bool Scheduler::is_completed(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_tasks_.count(task_id) > 0;
}

} // namespace coordinator

