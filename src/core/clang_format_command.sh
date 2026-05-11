#!/bin/bash
FILE=$1

clang-format -i \
  --style="{BasedOnStyle: LLVM, IndentWidth: 4, TabWidth: 4, UseTab: Never}" \
  ${FILE}
