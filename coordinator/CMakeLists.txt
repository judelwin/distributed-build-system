# coordinator/CMakeLists.txt
# CMake build configuration for the Coordinator service.
# Future dependencies: gRPC, Protocol Buffers, Redis client (to be added).

cmake_minimum_required(VERSION 3.15)
project(coordinator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(coordinator main.cpp)

# TODO: Link gRPC, Protobuf, and Redis libraries when implementation begins.
