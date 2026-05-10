#!/bin/bash

BASELOC="/home/bhavya/cosmos/life/UIUC/academics/coursework/CS526/latensor"
PIPELINEDIR="${BASELOC}/src/pipeline"

bash latensor.sh --build-dir "${PIPELINE_DIR}/build_dir" -o "${PIPELINE_DIR}"  src/tests/reduction_1/kernel.cpp src/tests/reduction_1/driver.cpp

