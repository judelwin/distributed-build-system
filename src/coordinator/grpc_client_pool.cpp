#include "grpc_client_pool.h"
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <mutex>

namespace coordinator {

WorkerClient::WorkerClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(build::BuildService::NewStub(channel)) {}

bool WorkerClient::SubmitTask(const build::TaskRequest& request, build::TaskResponse& response) {
    grpc::ClientContext context;
    grpc::Status status = stub_->SubmitTask(&context, request, &response);
    return status.ok() && response.accepted();
}

bool WorkerClient::GetStatus(const std::string& task_id, build::StatusResponse& response) {
    grpc::ClientContext context;
    build::StatusRequest request;
    request.set_task_id(task_id);
    grpc::Status status = stub_->GetStatus(&context, request, &response);
    return status.ok();
}

bool WorkerClient::GetResult(const std::string& task_id, build::ResultResponse& response) {
    grpc::ClientContext context;
    build::ResultRequest request;
    request.set_task_id(task_id);
    grpc::Status status = stub_->GetResult(&context, request, &response);
    return status.ok();
}

GrpcClientPool::GrpcClientPool(const std::vector<std::string>& worker_addresses)
    : current_worker_(0) {
    for (const auto& addr : worker_addresses) {
        auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
        clients_.push_back(std::make_unique<WorkerClient>(channel));
    }
}

int GrpcClientPool::select_worker() {
    std::lock_guard<std::mutex> lock(mutex_);
    int selected = current_worker_;
    current_worker_ = (current_worker_ + 1) % clients_.size();
    return selected;
}

WorkerClient* GrpcClientPool::get_client(int worker_idx) {
    if (worker_idx < 0 || worker_idx >= static_cast<int>(clients_.size())) {
        return nullptr;
    }
    return clients_[worker_idx].get();
}

} // namespace coordinator

