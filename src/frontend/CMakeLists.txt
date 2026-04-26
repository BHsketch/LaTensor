cmake_minimum_required(VERSION 3.13)
project(Translator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64")

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

add_subdirectory(plugin)
