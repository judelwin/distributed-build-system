#include <iostream>
#include <string>
#include <cstdlib>
#include <grpcpp/grpcpp.h>
#include "build_service_impl.h"

namespace worker {

void RunServer(const std::string& server_address, const std::string& redis_host, int redis_port) {
    BuildServiceImpl service(redis_host, redis_port);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Worker server listening on " << server_address << std::endl;

    server->Wait();
}

} // namespace worker

int main(int argc, char** argv) {
    std::string server_address = "0.0.0.0:50052";
    std::string redis_host = "redis";
    int redis_port = 6379;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            std::string port = argv[++i];
            server_address = "0.0.0.0:" + port;
        } else if (arg == "--host" && i + 1 < argc) {
            std::string host = argv[++i];
            size_t colon_pos = server_address.find(':');
            if (colon_pos != std::string::npos) {
                server_address = host + server_address.substr(colon_pos);
            } else {
                server_address = host + ":50052";
            }
        } else if (arg == "--redis-host" && i + 1 < argc) {
            redis_host = argv[++i];
        } else if (arg == "--redis-port" && i + 1 < argc) {
            redis_port = std::stoi(argv[++i]);
        }
    }

    // Override from environment variables
    const char* env_host = std::getenv("WORKER_HOST");
    const char* env_port = std::getenv("WORKER_PORT");
    if (env_host && env_port) {
        server_address = std::string(env_host) + ":" + std::string(env_port);
    }

    const char* env_redis_host = std::getenv("REDIS_HOST");
    if (env_redis_host) {
        redis_host = env_redis_host;
    }
    const char* env_redis_port = std::getenv("REDIS_PORT");
    if (env_redis_port) {
        redis_port = std::stoi(env_redis_port);
    }

    worker::RunServer(server_address, redis_host, redis_port);
    return 0;
}
