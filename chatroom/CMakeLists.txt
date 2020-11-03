cmake_minimum_required(VERSION 3.16)
project(chatroom)

set(CMAKE_CXX_STANDARD 11)

add_executable(chatroom main.cpp client.cpp test_lt_et.cpp test_oneshot.cpp non_block_connect.cpp chatroom_server.cpp)