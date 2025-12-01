cmake_minimum_required(VERSION 3.10)

# 项目名称
project(ConcurrentSystems_Task1to3)

# [cite_start]设置 C++ 标准为 C++11 (作业要求 [cite: 7])
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找系统线程库 (解决 Windows MinGW 或 Linux 下的 pthread 链接问题)
find_package(Threads REQUIRED)

# 指定你的源文件 (确保文件名和你修改后的一致)
set(SOURCE_FILES "11314389_Xu_Task5_Benchmark.cpp")

# 创建可执行文件 RaceSimulation
add_executable(RaceSimulation ${SOURCE_FILES})

# 将线程库链接到可执行文件
target_link_libraries(RaceSimulation Threads::Threads)

target_include_directories(RaceSimulation PRIVATE "${CMAKE_SOURCE_DIR}/Franks_websocket")

# Winsock is required on Windows regardless of compiler
if (WIN32)
    target_link_libraries(RaceSimulation ws2_32 mswsock)
endif()
