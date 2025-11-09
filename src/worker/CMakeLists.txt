# Worker service: receives and executes compilation tasks

set(WORKER_SOURCES
    main.cpp
)

add_executable(worker ${WORKER_SOURCES})

target_link_libraries(worker
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
    target_link_libraries(worker PRIVATE ${REDIS_LIBRARIES})
    target_include_directories(worker PRIVATE ${REDIS_INCLUDE_DIRS})
    target_compile_definitions(worker PRIVATE HAVE_REDIS)
endif()

target_include_directories(worker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

