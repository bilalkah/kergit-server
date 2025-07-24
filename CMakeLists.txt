cmake_minimum_required(VERSION 3.10)
project(ws_example)

set(CMAKE_CXX_STANDARD 17)

# JSON include path
set(JSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/json/single_include)

# Server
add_executable(server server.cpp)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/third_party/uWebSockets/src)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/third_party/uWebSockets/uSockets/src)
target_include_directories(server PRIVATE ${JSON_INCLUDE_DIR})
target_link_libraries(server pthread ${CMAKE_SOURCE_DIR}/third_party/uWebSockets/uSockets/uSockets.a z)

# Client
add_executable(client client.cpp)
target_include_directories(client PRIVATE ${JSON_INCLUDE_DIR})
target_link_libraries(client pthread)