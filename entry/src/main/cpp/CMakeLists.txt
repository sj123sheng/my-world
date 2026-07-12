cmake_minimum_required(VERSION 3.5)
project(native_game)

add_library(native_game SHARED native_bridge.cpp)
target_link_libraries(native_game PUBLIC libace_napi.z.so)
