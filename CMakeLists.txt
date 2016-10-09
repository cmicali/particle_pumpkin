cmake_minimum_required(VERSION 2.8.4)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(PROJECT_NAME pumpkin)

project(${PROJECT_NAME})

set(ARDUINO_SDK_PATH /Applications/Arduino.app/Contents/Java)
link_directories(~/Documents/Arduino/libraries)

set(${CMAKE_PROJECT_NAME}_SKETCH pumpkin.ino)
generate_arduino_firmware(${CMAKE_PROJECT_NAME})
