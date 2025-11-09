#pragma once

#include <string>
#include <vector>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "build.grpc.pb.h"

namespace coordinator {

// gRPC client wrapper for worker communication
class WorkerClient {
public:
    WorkerClient(std::shared_ptr<grpc::Channel> channel);

    bool SubmitTask(const build::TaskRequest& request, build::TaskResponse& response);
    bool GetStatus(const std::string& task_id, build::StatusResponse& response);
    bool GetResult(const std::string& task_id, build::ResultResponse& response);

private:
    std::unique_ptr<build::BuildService::Stub> stub_;
};

// Pool of gRPC clients for worker communication
class GrpcClientPool {
public:
    GrpcClientPool(const std::vector<std::string>& worker_addresses);
    
    // Select worker using round-robin
    int select_worker();
    
    // Get client for specific worker
    WorkerClient* get_client(int worker_idx);
    
    size_t size() const { return clients_.size(); }

private:
    std::vector<std::unique_ptr<WorkerClient>> clients_;
    int current_worker_;
    std::mutex mutex_;
};

} // namespace coordinator

