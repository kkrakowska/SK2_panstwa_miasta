cmake_minimum_required(VERSION 3.10)
project(PanstwaMiastaProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

include_directories(src/common)

add_executable(game_server src/server/server.cpp)

add_executable(test_client src/client_test/client.cpp)

add_executable(gui_client 
    src/client_gui/main.cpp
    src/client_gui/mainwindow.cpp
)

target_link_libraries(gui_client Qt6::Widgets Qt6::Network)
