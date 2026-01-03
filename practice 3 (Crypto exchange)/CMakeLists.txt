cmake_minimum_required(VERSION 3.31)
project(practice3)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(practice3 main.cpp
        server.cpp
        database/database.cpp
        database/filework.cpp
        database/hashchain.cpp
        database/parsing.cpp
        database/table.cpp)