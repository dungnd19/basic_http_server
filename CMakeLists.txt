cmake_minimum_required(VERSION 3.22)
project(basic_http_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(basic_http_server src/main.cpp src/http_message.cpp src/http_message.h src/uri.h src/http_server.cpp src/http_server.h)
