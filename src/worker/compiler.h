#pragma once

#include <string>
#include <chrono>
#include "build.grpc.pb.h"

namespace worker {

// Compiler interface for executing build tasks
class Compiler {
public:
    struct CompileResult {
        bool success;
        std::string artifact_path;
        std::string artifact_hash;
        int64_t build_time_ms;
        std::string error_message;
    };

    // Execute compilation task
    static CompileResult compile(const build::TaskRequest& request);
};

} // namespace worker

