cmake_minimum_required(VERSION 3.14)
project(ChatClient)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
  uWebSockets
  GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
  GIT_TAG v20.30.0
)

FetchContent_MakeAvailable(uWebSockets)

add_executable(client main.cpp)

target_include_directories(client PRIVATE ${uWebSockets_SOURCE_DIR}/src)
target_link_libraries(client PRIVATE uWS::uWS)