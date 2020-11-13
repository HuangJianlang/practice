cmake_minimum_required(VERSION 3.15)
project(linux)

set(CMAKE_CXX_STANDARD 11)

add_executable(linux main.cpp linux_6_3.cpp threadpool.cpp threadpool.h locker.cpp locker.h)