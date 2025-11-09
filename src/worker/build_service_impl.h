#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "build.grpc.pb.h"

namespace worker {

// Forward declaration
class RedisClient;

// Task execution state
struct TaskState {
    std::string task_id;
    build::TaskStatus status;
    std::string error_message;
    std::string artifact_path;
    std::string artifact_hash;
    bool success;
    int64_t build_time_ms;
    int64_t timestamp;
    std::mutex mutex;
    std::thread execution_thread;
};

// BuildService implementation
class BuildServiceImpl final : public build::BuildService::Service {
public:
    BuildServiceImpl(const std::string& redis_host, int redis_port);
    ~BuildServiceImpl();

    grpc::Status SubmitTask(grpc::ServerContext* context, const build::TaskRequest* request,
                           build::TaskResponse* response) override;

    grpc::Status GetStatus(grpc::ServerContext* context, const build::StatusRequest* request,
                          build::StatusResponse* response) override;

    grpc::Status GetResult(grpc::ServerContext* context, const build::ResultRequest* request,
                          build::ResultResponse* response) override;

private:
    void execute_task(std::shared_ptr<TaskState> state, const build::TaskRequest& request);
    
    std::unordered_map<std::string, std::shared_ptr<TaskState>> tasks_;
    std::mutex tasks_mutex_;
    RedisClient* redis_;
};

} // namespace worker

