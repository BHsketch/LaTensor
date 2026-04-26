add_library(Translator MODULE for_loop_detector.cpp)

target_include_directories(Translator PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_link_libraries(Translator PRIVATE
    clangAST
    clangASTMatchers
    clangFrontend
    clangTooling
)
