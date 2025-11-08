# Protocol Buffers definitions for gRPC services

set(PROTO_FILE build.proto)

# Generate C++ code from .proto file using gRPC's protobuf plugin
get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")

# Locate gRPC plugin for code generation
get_target_property(gRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)
if(NOT gRPC_CPP_PLUGIN)
    # Fallback: try to find it in PATH or common locations
    find_program(gRPC_CPP_PLUGIN 
        NAMES grpc_cpp_plugin
        PATHS ${CMAKE_INSTALL_PREFIX}/bin /usr/bin /usr/local/bin
    )
endif()

# Generate protobuf and gRPC code
add_custom_command(
    OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND protobuf::protoc
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${CMAKE_CURRENT_SOURCE_DIR}"
         --plugin=protoc-gen-grpc="${gRPC_CPP_PLUGIN}"
         "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}"
    COMMENT "Running C++ gRPC compiler on ${PROTO_FILE}"
    VERBATIM
)

# Create proto library for linking
add_library(proto_lib STATIC
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_SRCS}
    ${GRPC_HDRS}
)

target_link_libraries(proto_lib
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(proto_lib PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

