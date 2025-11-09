#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <chrono>
#include "build.pb.h"

namespace build {

// Task structure for internal scheduling
struct Task {
    std::string id;
    std::string content_hash;
    std::vector<std::string> source_files;
    std::vector<std::string> dependencies;
    std::vector<std::string> compile_flags;
    std::string output_path;
    int timeout_seconds;
    std::unordered_set<std::string> depends_on;  // Task IDs this depends on
    int in_degree;  // Number of unresolved dependencies
    TaskStatus status;
};

// Task execution tracking for fault tolerance
struct TaskExecution {
    std::string task_id;
    int worker_idx;
    std::chrono::steady_clock::time_point start_time;
    int timeout_seconds;
    int retry_count;
    std::string content_hash;
    bool is_failed;
};

} // namespace build

