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
)

# Add Redis if found
if(REDIS_FOUND)
    target_link_libraries(coordinator PRIVATE ${REDIS_LIBRARIES})
    target_include_directories(coordinator PRIVATE ${REDIS_INCLUDE_DIRS})
    target_compile_definitions(coordinator PRIVATE HAVE_REDIS)
endif()

target_include_directories(coordinator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

