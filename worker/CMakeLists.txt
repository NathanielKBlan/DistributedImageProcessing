cmake_minimum_required(VERSION 3.10)
project(distributed-image-worker-node)

find_package(OpenMP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -O2 -Wextra -Wno-missing-field-initializers")

set(SOURCE_FILES )

add_subdirectory(include/kissnet)
include_directories(include/kissnet)

add_executable(worker src/worker.cpp ${SOURCE_FILES})
target_include_directories(worker PRIVATE include)
target_link_libraries(worker PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(kissnet)