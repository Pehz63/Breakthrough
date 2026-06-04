cmake_minimum_required(VERSION 3.10)
project(Breakthrough CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(breakthrough
    src/main.cpp
    src/board_io.cpp
    src/settings.cpp
    src/board_analysis.cpp
    src/moves.cpp
    src/ai_eval.cpp
    src/ai_random.cpp
    src/ai_minimax.cpp
)

target_include_directories(breakthrough PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
