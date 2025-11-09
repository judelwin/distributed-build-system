#include "compiler.h"
#include "hash_utils.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace worker {

Compiler::CompileResult Compiler::compile(const build::TaskRequest& request) {
    CompileResult result;
    auto start_time = std::chrono::steady_clock::now();

    // Simulate compilation time (1-5 seconds based on number of source files)
    int sleep_ms = 1000 + (request.source_files_size() * 500);
    if (sleep_ms > 5000) sleep_ms = 5000;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    // Create output directory if needed
    std::filesystem::path output_path(request.output_path());
    std::filesystem::create_directories(output_path.parent_path());

    // Generate mock artifact (simulate compiled output)
    std::string artifact_content = "compiled:" + request.task_id();
    for (const auto& file : request.source_files()) {
        artifact_content += ":" + file;
    }
    
    // Write mock artifact
    std::ofstream outfile(request.output_path());
    if (outfile.is_open()) {
        outfile << artifact_content;
        outfile.close();
        result.success = true;
    } else {
        result.success = false;
        result.error_message = "Failed to write artifact to " + request.output_path();
    }

    // Compute artifact hash
    if (result.success) {
        result.artifact_path = request.output_path();
        result.artifact_hash = build::compute_hash(artifact_content);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}

} // namespace worker

