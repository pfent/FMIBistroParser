cmake_minimum_required(VERSION 3.5)
project(FMIBistroParser)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Wnon-virtual-dtor -Wpedantic")

set(SOURCE_FILES main.cpp)
add_executable(FMIBistroParser ${SOURCE_FILES})
target_link_libraries(FMIBistroParser poppler-cpp curl)