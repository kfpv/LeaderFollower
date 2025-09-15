cmake_minimum_required(VERSION 3.10)
project(test_ui)

set(CMAKE_CXX_STANDARD 17)

add_executable(test_ui
  main.cpp
  ../web_ui.h
  ../anim_schema.h
)

if (APPLE)
  add_definitions(-DARDUINO_ARCH_ESP32)
endif()

target_compile_definitions(test_ui PRIVATE ARDUINO=1)
