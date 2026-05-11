#!/usr/bin/env bash
# benchmark.sh — run the latensor pipeline (and a clang -O3 baseline) over
# two benchmark sets:
#
#   1. A curated list of microbenchmarks under src/tests/, each with its own
#      kernel.cpp + driver.cpp pair. Failures here abort the script — these
#      are the canaries.
#
#   2. Every benchmark listed in PolyBench's official utilities/benchmark_list,
#      each a single .c file linked against utilities/polybench.c. Failures
#      here do NOT abort the script; they're collected into a summary table
#      at the end. PolyBench programs are expected to fail at various stages
#      while polly_pass and the frame contract are still being extended.

set -uo pipefail

BASELOC="/home/bhavya/cosmos/life/UIUC/academics/coursework/CS526/latensor"
PIPELINEDIR="${BASELOC}/src/pipeline"
TESTDIR="${BASELOC}/src/tests"
POLYBENCH_ROOT="${TESTDIR}/PolyBenchC-4.2.1"
POLYBENCH_UTILS="${POLYBENCH_ROOT}/utilities"
TVMLIB="/home/bhavya/cosmos/cse/projects/tvm/build:/home/bhavya/cosmos/cse/projects/tvm/build/lib"
LLVMBIN="${BASELOC}/llvm-project/build/bin"
CLANGXX="${LLVMBIN}/clang++"
CLANG="${LLVMBIN}/clang"

BUILD_ROOT="${PIPELINEDIR}/build_dir"
mkdir -p "${BUILD_ROOT}"

# Per-benchmark timing results. One line per microbenchmark:
#   <name> <cpp_time> <tvm_linked_time>
# Times are the single numeric value on each binary's "Execution time" line.
RESULTS_FILE="${BUILD_ROOT}/micro_results.txt"
: > "${RESULTS_FILE}"

# Tuning budget for both loops. PolyBench × 30 makes total tuning time
# scale with this — drop it for smoke tests, raise it for real numbers.
TUNING_TRIALS="${TUNING_TRIALS:-20}"

# PolyBench dataset macro + the SCALAR_LB flag the current frame/polly path
# requires + POLYBENCH_TIME so each benchmark prints the kernel's elapsed
# seconds to stdout (single floating-point number per run). Keeping these
# together so they always move as one.
#POLYBENCH_DEFINES="-DMINI_DATASET -DPOLYBENCH_USE_SCALAR_LB -DPOLYBENCH_TIME"
POLYBENCH_DEFINES="-DMEDIUM_DATASET -DPOLYBENCH_USE_SCALAR_LB -DPOLYBENCH_TIME"

# ─── Loop 1: curated microbenchmarks ────────────────────────────────────────
# Each entry is a subdirectory name under src/tests/ that contains a
# kernel.cpp and a driver.cpp.
MICRO_BENCHMARKS=(
    #"matmul_naive"
    #"triangular_matmul"
    #"simple_reduction"
    #"scan_array"
    #"mixed"
    #"attention"
    # add more here
)

# ─── Loop 2 list: PolyBench benchmarks ──────────────────────────────────────
# Path under PolyBenchC-4.2.1/, without the trailing `.c`. Currently scoped
# to the subset known to pass the full pipeline; add more as polly_pass /
# the frame contract grow to cover them. Run a benchmark by hand against
# benchmark_list to see why it's not here.
POLYBENCH_BENCHMARKS=(
    #"linear-algebra/kernels/mvt/mvt"
    "linear-algebra/blas/gemm/gemm"
    #"linear-algebra/blas/syr2k/syr2k"
    #"linear-algebra/blas/syrk/syrk"
    #"medley/deriche/deriche"
    #"medley/nussinov/nussinov"
)

set -e
for BENCHMARK in "${MICRO_BENCHMARKS[@]}"; do
    BENCH_DIR="${TESTDIR}/${BENCHMARK}"
    OUT_DIR="${BUILD_ROOT}/${BENCHMARK}"
    mkdir -p "${OUT_DIR}"

    echo
    echo "########## [${BENCHMARK}] compiling cpp baseline ##########"
    "${CLANGXX}" -O3 -march=native -std=c++17 \
        -o "${OUT_DIR}/cpp.out" \
        "${BENCH_DIR}/driver.cpp" "${BENCH_DIR}/kernel.cpp"

    echo "########## [${BENCHMARK}] running latensor pipeline ##########"
    bash "${PIPELINEDIR}/latensor.sh" \
        --build-dir "${OUT_DIR}" \
        --tuning-trials "${TUNING_TRIALS}" \
        -o "${OUT_DIR}/tvm.out" \
        "${BENCH_DIR}/kernel.cpp" "${BENCH_DIR}/driver.cpp"

    # Run binaries with stdout tee'd to the terminal AND captured to a log,
    # so a hanging or slow binary is visible in real time (command-sub alone
    # swallows output until the process exits — looks like silent failure).
    echo "########## [${BENCHMARK}] cpp baseline output ##########"
    "${OUT_DIR}/cpp.out" 2>&1 | tee "${OUT_DIR}/cpp.run.log"
    CPP_TIME="$(grep "Execution time" "${OUT_DIR}/cpp.run.log" \
        | grep -oE '[0-9]+(\.[0-9]+)?' | tail -1)"

    echo "########## [${BENCHMARK}] TVM-linked output ##########"
    LD_LIBRARY_PATH="${TVMLIB}" "${OUT_DIR}/tvm.out" 2>&1 \
        | tee "${OUT_DIR}/tvm.run.log"
    TVM_TIME="$(grep "Execution time" "${OUT_DIR}/tvm.run.log" \
        | grep -oE '[0-9]+(\.[0-9]+)?' | tail -1)"

    echo "${BENCHMARK} ${CPP_TIME} ${TVM_TIME}" >> "${RESULTS_FILE}"
done
set +e

echo
echo "########## plotting microbenchmark results ##########"
echo "  results file: ${RESULTS_FILE}"
python3 "${PIPELINEDIR}/plot_benchmark.py" "${RESULTS_FILE}" \
    "${BUILD_ROOT}/micro_results.png"

# ─── Loop 2: PolyBench (every entry in utilities/benchmark_list) ────────────
# Each iteration runs in a subshell with `set -e`, so a failure aborts that
# iteration without taking the whole script down.

PASS_LIST=()
FAIL_LIST=()

for ENTRY in "${POLYBENCH_BENCHMARKS[@]}"; do
    # ENTRY is like "linear-algebra/blas/gemm/gemm" — no leading ./, no .c
    SRC="${POLYBENCH_ROOT}/${ENTRY}.c"
    BENCH_DIR="$(dirname "${SRC}")"
    BENCH_NAME="$(basename "${ENTRY}")"
    REL_DIR="$(dirname "${ENTRY}")"
    OUT_DIR="${BUILD_ROOT}/polybench/${REL_DIR}"
    mkdir -p "${OUT_DIR}"

    echo
    echo "########## [polybench/${BENCH_NAME}] (${REL_DIR}) ##########"

    if (
        set -e

        echo "  - compiling cpp baseline"
        "${CLANG}" -O3 -march=native -std=gnu99 \
            ${POLYBENCH_DEFINES} \
            -I"${POLYBENCH_UTILS}" -I"${BENCH_DIR}" \
            -o "${OUT_DIR}/cpp.out" \
            "${SRC}" "${POLYBENCH_UTILS}/polybench.c" \
            -lm

        echo "  - running latensor pipeline"
        bash "${PIPELINEDIR}/latensor.sh" \
            --build-dir "${OUT_DIR}" \
            --tuning-trials "${TUNING_TRIALS}" \
            --cflags "-x c -std=gnu99 ${POLYBENCH_DEFINES} -I${POLYBENCH_UTILS} -I${BENCH_DIR}" \
            -o "${OUT_DIR}/tvm.out" \
            "${SRC}" \
            "${POLYBENCH_UTILS}/polybench.c"

        # POLYBENCH_TIME makes each binary print a single elapsed-seconds
        # number to stdout — capture it so we can label cpp vs tvm.
        echo "  - running cpp baseline"
        CPP_TIME="$("${OUT_DIR}/cpp.out")"
        echo "      cpp time : ${CPP_TIME}s"

        echo "  - running TVM-linked version"
        TVM_TIME="$(LD_LIBRARY_PATH="${TVMLIB}" "${OUT_DIR}/tvm.out")"
        echo "      tvm time : ${TVM_TIME}s"
    ); then
        echo "  RESULT: PASS"
        PASS_LIST+=("${REL_DIR}/${BENCH_NAME}")
    else
        echo "  RESULT: FAIL"
        FAIL_LIST+=("${REL_DIR}/${BENCH_NAME}")
    fi
done

echo
echo "===================== POLYBENCH SUMMARY ====================="
echo "PASS (${#PASS_LIST[@]}/$((${#PASS_LIST[@]} + ${#FAIL_LIST[@]}))):"
for b in "${PASS_LIST[@]}"; do echo "    ${b}"; done
echo "FAIL (${#FAIL_LIST[@]}/$((${#PASS_LIST[@]} + ${#FAIL_LIST[@]}))):"
for b in "${FAIL_LIST[@]}"; do echo "    ${b}"; done
echo "============================================================="
