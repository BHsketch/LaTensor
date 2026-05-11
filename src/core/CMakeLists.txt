cmake_minimum_required(VERSION 3.20)
project(MyPollyPass LANGUAGES C CXX)

set(CMAKE_SKIP_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Paths to your in-tree LLVM + Polly build
set(LLVM_DIR "/usr/lib/llvm-21/lib/cmake/llvm/")
set(Polly_DIR "/usr/lib/llvm-21/lib/cmake/polly")

find_package(ZLIB REQUIRED)
find_package(zstd REQUIRED)
find_package(LLVM REQUIRED CONFIG)
find_package(Polly REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM dir: ${LLVM_DIR}")
message(STATUS "Polly dir: ${Polly_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${Polly_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Match RTTI settings with LLVM
if(LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Build plugin
add_library(MyPollyPass MODULE polly_pass.cpp)

# Remove "lib" prefix
set_target_properties(MyPollyPass PROPERTIES PREFIX "")

# Link against Polly (provides ScopInfoAnalysis::Key et al.)
# Do NOT link the LLVM component libs — opt/clang already has them loaded.
target_link_libraries(MyPollyPass PRIVATE)

target_link_options(MyPollyPass PRIVATE "LINKER:--allow-shlib-undefined")
