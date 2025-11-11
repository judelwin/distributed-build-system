#include "metrics.h"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>

namespace coordinator {

MetricsCollector::MetricsCollector()
    : num_workers_(0), total_tasks_(0), completed_tasks_(0),
      cached_tasks_(0), failed_tasks_(0), retried_tasks_(0) {
}

void MetricsCollector::start_build(int num_workers) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    build_start_time_ = std::chrono::steady_clock::now();
    num_workers_ = num_workers;
    total_tasks_ = 0;
    completed_tasks_ = 0;
    cached_tasks_ = 0;
    failed_tasks_ = 0;
    retried_tasks_ = 0;
    task_start_times_.clear();
    task_build_times_.clear();
    task_from_cache_.clear();
    task_worker_map_.clear();
    task_failed_.clear();
    task_retry_count_.clear();
}

void MetricsCollector::record_task_start(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    task_start_times_[task_id] = std::chrono::steady_clock::now();
    total_tasks_++;
}

void MetricsCollector::record_task_complete(const std::string& task_id, 
                                           int worker_idx,
                                           int64_t build_time_ms,
                                           bool from_cache) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    completed_tasks_++;
    if (from_cache) {
        cached_tasks_++;
    }
    task_build_times_[task_id] = build_time_ms;
    task_from_cache_[task_id] = from_cache;
    task_worker_map_[task_id] = worker_idx;
}

void MetricsCollector::record_task_failure(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    task_failed_[task_id] = true;
    failed_tasks_++;
}

void MetricsCollector::record_task_retry(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    task_retry_count_[task_id]++;
    retried_tasks_++;
}

BuildMetrics MetricsCollector::finish_build() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return compute_metrics();
}

BuildMetrics MetricsCollector::get_current_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return const_cast<MetricsCollector*>(this)->compute_metrics();
}

BuildMetrics MetricsCollector::compute_metrics() const {
    BuildMetrics metrics;
    metrics.total_tasks = total_tasks_;
    metrics.completed_tasks = completed_tasks_;
    metrics.cached_tasks = cached_tasks_;
    metrics.failed_tasks = failed_tasks_;
    metrics.retried_tasks = retried_tasks_;
    
    // Compute total build time
    auto build_end_time = std::chrono::steady_clock::now();
    auto build_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        build_end_time - build_start_time_);
    metrics.total_build_time_ms = build_duration.count();
    
    // Compute task timing statistics
    metrics.task_timings_ms.clear();
    for (const auto& [task_id, build_time] : task_build_times_) {
        if (!task_failed_.count(task_id) || !task_failed_.at(task_id)) {
            metrics.task_timings_ms.push_back(build_time);
        }
    }
    
    if (!metrics.task_timings_ms.empty()) {
        auto minmax = std::minmax_element(metrics.task_timings_ms.begin(),
                                         metrics.task_timings_ms.end());
        metrics.min_task_time_ms = *minmax.first;
        metrics.max_task_time_ms = *minmax.second;
        
        int64_t sum = std::accumulate(metrics.task_timings_ms.begin(),
                                     metrics.task_timings_ms.end(), 0LL);
        metrics.avg_task_time_ms = sum / metrics.task_timings_ms.size();
    }
    
    // Compute throughput
    if (metrics.total_build_time_ms > 0) {
        metrics.tasks_per_second = (completed_tasks_ * 1000.0) / metrics.total_build_time_ms;
    }
    
    // Compute cache hit rate
    if (total_tasks_ > 0) {
        metrics.cache_hit_rate = (cached_tasks_ * 100.0) / total_tasks_;
    }
    
    // Compute worker distribution
    for (const auto& [task_id, worker_idx] : task_worker_map_) {
        metrics.tasks_per_worker[worker_idx]++;
    }
    
    return metrics;
}

bool MetricsCollector::export_to_csv(const std::string& filename,
                                     const std::vector<BuildMetrics>& metrics_list,
                                     int num_workers) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header
    file << "num_workers,total_tasks,completed_tasks,cached_tasks,failed_tasks,"
         << "retried_tasks,total_build_time_ms,min_task_time_ms,max_task_time_ms,"
         << "avg_task_time_ms,tasks_per_second,cache_hit_rate\n";
    
    // Write data rows
    for (const auto& metrics : metrics_list) {
        file << num_workers << ","
             << metrics.total_tasks << ","
             << metrics.completed_tasks << ","
             << metrics.cached_tasks << ","
             << metrics.failed_tasks << ","
             << metrics.retried_tasks << ","
             << metrics.total_build_time_ms << ","
             << metrics.min_task_time_ms << ","
             << metrics.max_task_time_ms << ","
             << metrics.avg_task_time_ms << ","
             << std::fixed << std::setprecision(2) << metrics.tasks_per_second << ","
             << std::fixed << std::setprecision(2) << metrics.cache_hit_rate << "\n";
    }
    
    file.close();
    return true;
}

} // namespace coordinator

