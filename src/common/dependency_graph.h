#pragma once

#include <string>
#include <unordered_map>
#include "build_task.h"

namespace build {

// Parse JSON dependency graph
std::unordered_map<std::string, Task> parse_dependency_graph(const std::string& json_file);

// Topological sort to determine build order
std::vector<std::string> topological_sort(std::unordered_map<std::string, Task>& tasks);

} // namespace build

