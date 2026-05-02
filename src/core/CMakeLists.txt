cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(MyPollyPass LANGUAGES C CXX)

# Point this to your Homebrew LLVM install
set(LLVM_DIR "/opt/homebrew/opt/llvm/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Polly is shipped with Homebrew LLVM
find_package(Polly REQUIRED CONFIG)

include_directories(${Polly_INCLUDE_DIRS})
link_directories(${Polly_LIBRARY_DIRS})

# Build plugin
add_library(MyPollyPass MODULE polly_pass.cpp)

# LLVM plugins must not have "lib" prefix
set_target_properties(MyPollyPass PROPERTIES
    PREFIX ""
    LINK_FLAGS "-undefined dynamic_lookup"
)