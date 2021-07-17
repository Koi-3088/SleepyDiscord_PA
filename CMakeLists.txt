cmake_minimum_required (VERSION 3.6)
project(Sleepy)

add_executable(Sleepy "Sleepy.cpp" "Sleepy.h")
add_subdirectory(deps/sleepy-discord)
target_link_libraries(Sleepy sleepy-discord)