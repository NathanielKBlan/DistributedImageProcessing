cmake_minimum_required(VERSION 3.1)
project(distributed-image-master-node)

find_package(OpenMP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -O2 -Wextra -Wno-missing-field-initializers")

add_executable(master src/master.cpp)
target_include_directories(master PRIVATE include)
target_link_libraries(master PUBLIC OpenMP::OpenMP_CXX)