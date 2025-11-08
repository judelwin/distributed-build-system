# Coordinator service: schedules and dispatches build tasks

set(COORDINATOR_SOURCES
    main.cpp
)

add_executable(coordinator ${COORDINATOR_SOURCES})

target_link_libraries(coordinator
    PRIVATE
    proto_lib
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Add Redis if found
if(REDIS_FOUND)
    target_link_libraries(coordinator PRIVATE ${REDIS_LIBRARIES})
    target_include_directories(coordinator PRIVATE ${REDIS_INCLUDE_DIRS})
    target_compile_definitions(coordinator PRIVATE HAVE_REDIS)
endif()

# Find or download nlohmann/json for JSON parsing
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

target_link_libraries(coordinator PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(coordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

