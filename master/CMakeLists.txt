cmake_minimum_required(VERSION 3.1)
project(distributed-image-master-node)

find_package(OpenMP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -O2 -Wextra -Wno-missing-field-initializers")

set(SOURCE_FILES )

add_subdirectory(include/kissnet)
include_directories(include)

add_executable(master src/master.cpp ${SOURCE_FILES})
target_include_directories(master PRIVATE include)
target_link_libraries(master PUBLIC OpenMP::OpenMP_CXX)
# target_link_libraries(kissnet)

target_link_libraries(master PUBLIC kissnet)