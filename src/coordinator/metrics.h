#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace coordinator {

// Metrics for a single build execution
struct BuildMetrics {
    int total_tasks = 0;
    int completed_tasks = 0;
    int cached_tasks = 0;
    int failed_tasks = 0;
    int retried_tasks = 0;
    
    // Timing metrics (in milliseconds)
    int64_t total_build_time_ms = 0;
    int64_t min_task_time_ms = 0;
    int64_t max_task_time_ms = 0;
    int64_t avg_task_time_ms = 0;
    
    // Throughput metrics
    double tasks_per_second = 0.0;
    
    // Cache metrics
    double cache_hit_rate = 0.0;
    
    // Worker distribution
    std::unordered_map<int, int> tasks_per_worker;
    
    // Task-level timings
    std::vector<int64_t> task_timings_ms;
};

// Metrics collector for tracking build performance
class MetricsCollector {
public:
    MetricsCollector();
    
    // Start tracking a build
    void start_build(int num_workers);
    
    // Record task start
    void record_task_start(const std::string& task_id);
    
    // Record task completion
    void record_task_complete(const std::string& task_id, int worker_idx, 
                             int64_t build_time_ms, bool from_cache);
    
    // Record task failure
    void record_task_failure(const std::string& task_id);
    
    // Record task retry
    void record_task_retry(const std::string& task_id);
    
    // Finish tracking and compute final metrics
    BuildMetrics finish_build();
    
    // Get current metrics snapshot
    BuildMetrics get_current_metrics() const;
    
    // Export metrics to CSV
    static bool export_to_csv(const std::string& filename, 
                              const std::vector<BuildMetrics>& metrics_list,
                              int num_workers);

private:
    std::chrono::steady_clock::time_point build_start_time_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> task_start_times_;
    std::unordered_map<std::string, int64_t> task_build_times_;
    std::unordered_map<std::string, bool> task_from_cache_;
    std::unordered_map<std::string, int> task_worker_map_;
    std::unordered_map<std::string, bool> task_failed_;
    std::unordered_map<std::string, int> task_retry_count_;
    
    int num_workers_;
    int total_tasks_;
    int completed_tasks_;
    int cached_tasks_;
    int failed_tasks_;
    int retried_tasks_;
    
    mutable std::mutex metrics_mutex_;
    
    BuildMetrics compute_metrics() const;
};

} // namespace coordinator

