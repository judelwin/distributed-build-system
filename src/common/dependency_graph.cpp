#include "dependency_graph.h"
#include "hash_utils.h"
#include "build_task.h"
#include "build.pb.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <queue>
#include <stdexcept>

using json = nlohmann::json;

namespace build {

std::unordered_map<std::string, Task> parse_dependency_graph(const std::string& json_file) {
    std::ifstream file(json_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + json_file);
    }

    json j;
    file >> j;

    std::unordered_map<std::string, Task> tasks;

    // Parse tasks from JSON
    if (j.contains("tasks") && j["tasks"].is_array()) {
        for (const auto& task_json : j["tasks"]) {
            Task task;
            task.id = task_json["id"];
            task.output_path = task_json.value("output_path", "");
            
            if (task_json.contains("source_files")) {
                for (const auto& file : task_json["source_files"]) {
                    task.source_files.push_back(file);
                }
            }
            
            if (task_json.contains("dependencies")) {
                for (const auto& dep : task_json["dependencies"]) {
                    task.dependencies.push_back(dep);
                }
            }
            
            if (task_json.contains("compile_flags")) {
                for (const auto& flag : task_json["compile_flags"]) {
                    task.compile_flags.push_back(flag);
                }
            }
            
            task.timeout_seconds = task_json.value("timeout_seconds", 60);
            task.status = TaskStatus::PENDING;
            
            // Parse task dependencies (other task IDs)
            if (task_json.contains("depends_on")) {
                for (const auto& dep_id : task_json["depends_on"]) {
                    task.depends_on.insert(dep_id);
                }
            }
            
            task.in_degree = task.depends_on.size();
            task.content_hash = generate_content_hash(task);
            tasks[task.id] = task;
        }
    }

    return tasks;
}

std::vector<std::string> topological_sort(std::unordered_map<std::string, Task>& tasks) {
    // Build reverse dependency graph (which tasks depend on this task)
    std::unordered_map<std::string, std::vector<std::string>> reverse_deps;
    for (const auto& [id, task] : tasks) {
        for (const auto& dep_id : task.depends_on) {
            reverse_deps[dep_id].push_back(id);
        }
    }

    std::queue<std::string> ready_queue;
    std::unordered_map<std::string, int> in_degree;
    std::vector<std::string> result;

    // Initialize in-degree counts
    for (const auto& [id, task] : tasks) {
        in_degree[id] = task.in_degree;
        if (task.in_degree == 0) {
            ready_queue.push(id);
        }
    }

    // Process ready tasks
    while (!ready_queue.empty()) {
        std::string current = ready_queue.front();
        ready_queue.pop();
        result.push_back(current);

        // Reduce in-degree for tasks that depend on current
        if (reverse_deps.count(current)) {
            for (const auto& dependent_id : reverse_deps[current]) {
                in_degree[dependent_id]--;
                if (in_degree[dependent_id] == 0) {
                    ready_queue.push(dependent_id);
                }
            }
        }
    }

    // Check for cycles
    if (result.size() != tasks.size()) {
        throw std::runtime_error("Dependency graph contains cycles");
    }

    return result;
}

} // namespace build

