cmake_minimum_required(VERSION 3.1)
project(distributed-image-worker-node)

find_package(OpenMP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -O2 -Wextra -Wno-missing-field-initializers")

set(SOURCE_FILES )

add_subdirectory(include/kissnet)
include_directories(include)

add_executable(worker src/worker.cpp ${SOURCE_FILES})
target_include_directories(worker PRIVATE include)
target_link_libraries(worker PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(worker PUBLIC kissnet)