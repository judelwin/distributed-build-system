#pragma once

#include <string>

namespace build {

// Forward declaration
struct Task;

// Compute SHA256 hash of string
std::string compute_hash(const std::string& data);

// Generate content hash from task properties
std::string generate_content_hash(const Task& task);

} // namespace build

