#include "build_service_impl.h"
#include "redis_client.h"
#include "compiler.h"
#include <iostream>
#include <ctime>

namespace worker {

BuildServiceImpl::BuildServiceImpl(const std::string& redis_host, int redis_port)
    : redis_(new RedisClient(redis_host, redis_port)) {}

BuildServiceImpl::~BuildServiceImpl() {
    // Wait for all tasks to complete
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& [task_id, state] : tasks_) {
        if (state->execution_thread.joinable()) {
            state->execution_thread.join();
        }
    }
    delete redis_;
}

grpc::Status BuildServiceImpl::SubmitTask(grpc::ServerContext* context, 
                                          const build::TaskRequest* request,
                                          build::TaskResponse* response) {
    std::string task_id = request->task_id();
    
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        // Check if task already exists
        if (tasks_.count(task_id)) {
            response->set_accepted(false);
            response->set_message("Task already exists");
            response->set_status(build::TaskStatus::IN_PROGRESS);
            return grpc::Status::OK;
        }

        // Create new task state
        auto state = std::make_shared<TaskState>();
        state->task_id = task_id;
        state->status = build::TaskStatus::PENDING;
        state->success = false;
        
        tasks_[task_id] = state;

        // Start task execution in background thread
        state->execution_thread = std::thread(
            &BuildServiceImpl::execute_task, this, state, *request);

        response->set_accepted(true);
        response->set_message("Task accepted");
        response->set_status(build::TaskStatus::PENDING);
    }

    std::cout << "Accepted task: " << task_id << std::endl;
    return grpc::Status::OK;
}

grpc::Status BuildServiceImpl::GetStatus(grpc::ServerContext* context, 
                                        const build::StatusRequest* request,
                                        build::StatusResponse* response) {
    std::string task_id = request->task_id();
    
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    if (tasks_.count(task_id) == 0) {
        response->set_task_id(task_id);
        response->set_status(build::TaskStatus::FAILED);
        response->set_message("Task not found");
        return grpc::Status::OK;
    }

    auto state = tasks_[task_id];
    std::lock_guard<std::mutex> state_lock(state->mutex);
    
    response->set_task_id(task_id);
    response->set_status(state->status);
    
    if (state->status == build::TaskStatus::IN_PROGRESS) {
        response->set_progress_percent(50);  // Mock progress
        response->set_message("Building...");
    } else if (state->status == build::TaskStatus::COMPLETED) {
        response->set_progress_percent(100);
        response->set_message("Completed");
    } else if (state->status == build::TaskStatus::FAILED) {
        response->set_progress_percent(0);
        response->set_message(state->error_message);
    }

    return grpc::Status::OK;
}

grpc::Status BuildServiceImpl::GetResult(grpc::ServerContext* context, 
                                        const build::ResultRequest* request,
                                        build::ResultResponse* response) {
    std::string task_id = request->task_id();
    
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    if (tasks_.count(task_id) == 0) {
        response->set_task_id(task_id);
        response->set_status(build::TaskStatus::FAILED);
        response->set_success(false);
        response->set_error_message("Task not found");
        return grpc::Status::OK;
    }

    auto state = tasks_[task_id];
    std::lock_guard<std::mutex> state_lock(state->mutex);
    
    response->set_task_id(task_id);
    response->set_status(state->status);
    response->set_success(state->success);
    
    if (state->success) {
        response->set_artifact_hash(state->artifact_hash);
        response->set_artifact_path(state->artifact_path);
        response->set_build_time_ms(state->build_time_ms);
        response->set_timestamp(state->timestamp);
    } else {
        response->set_error_message(state->error_message);
    }

    return grpc::Status::OK;
}

void BuildServiceImpl::execute_task(std::shared_ptr<TaskState> state, 
                                   const build::TaskRequest& request) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->status = build::TaskStatus::IN_PROGRESS;
    }

    // Execute compilation
    auto result = Compiler::compile(request);
    
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->status = result.success ? build::TaskStatus::COMPLETED : build::TaskStatus::FAILED;
        state->success = result.success;
        state->artifact_path = result.artifact_path;
        state->artifact_hash = result.artifact_hash;
        state->build_time_ms = result.build_time_ms;
        state->timestamp = std::time(nullptr);
        if (!result.success) {
            state->error_message = result.error_message;
        }
    }

    // Store in Redis cache
    if (redis_->is_available() && result.success) {
        redis_->store_artifact(request.content_hash(), result.artifact_hash, result.artifact_path);
    }
}

} // namespace worker

