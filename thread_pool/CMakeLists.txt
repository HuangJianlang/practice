cmake_minimum_required(VERSION 3.15)
project(thread_pool)

set(CMAKE_CXX_STANDARD 11)

add_executable(thread_pool main.cpp ProcessPool.cpp ProcessPool.h simple_cgi.cpp linux_6_2.cpp)