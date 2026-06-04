cmake_minimum_required(VERSION 3.10)
project(Breakthrough CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(breakthrough
    main.cpp
    board_io.cpp
    settings.cpp
    board_analysis.cpp
    moves.cpp
    ai_random.cpp
    ai_minimax.cpp
)

target_include_directories(breakthrough PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
