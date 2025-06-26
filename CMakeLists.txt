cmake_minimum_required(VERSION 3.10)

project(MyServerProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(SOURCES
    key_value_server/server_4.cpp
    key_value_server/hash.cpp
    key_value_server/avl.cpp
    key_value_server/range.cpp
)

add_executable(server_4 ${SOURCES})
