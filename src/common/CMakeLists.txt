# Common library: shared types and utilities

set(COMMON_SOURCES
    dependency_graph.cpp
    hash_utils.cpp
)

add_library(common_lib STATIC ${COMMON_SOURCES})

target_link_libraries(common_lib
    PRIVATE
    proto_lib
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Find or download nlohmann/json for JSON parsing
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

target_link_libraries(common_lib PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(common_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/proto
    ${CMAKE_BINARY_DIR}/proto
)

