cmake_minimum_required(VERSION 3.20)
project(DistributedBuildSystem VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build configuration
set(CMAKE_BUILD_TYPE Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages (gRPC, Protocol Buffers, Redis, OpenSSL)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)

# Find hiredis (Redis client library)
# Try pkg-config first, fall back to find_package or manual search
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(REDIS QUIET hiredis)
endif()

# If pkg-config didn't find it, try to find it manually
if(NOT REDIS_FOUND)
    find_path(REDIS_INCLUDE_DIRS
        NAMES hiredis/hiredis.h
        PATHS /usr/include /usr/local/include ${CMAKE_INSTALL_PREFIX}/include
    )
    find_library(REDIS_LIBRARIES
        NAMES hiredis
        PATHS /usr/lib /usr/local/lib ${CMAKE_INSTALL_PREFIX}/lib
    )
    if(REDIS_INCLUDE_DIRS AND REDIS_LIBRARIES)
        set(REDIS_FOUND TRUE)
    endif()
endif()

if(NOT REDIS_FOUND)
    message(WARNING "hiredis not found. Redis caching will not be available.")
    set(REDIS_INCLUDE_DIRS "")
    set(REDIS_LIBRARIES "")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Protocol Buffers generation
add_subdirectory(proto)

# Common library (shared types and utilities)
add_subdirectory(src/common)

# Coordinator service
add_subdirectory(src/coordinator)

# Worker service
add_subdirectory(src/worker)

