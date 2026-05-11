#!/usr/bin/env bash
# latensor.sh — env-setup wrapper for latensor.py.
#
# Activates the project's TVM venv, sets TVM_HOME if unset, prepends the
# in-tree clang/opt to PATH, then invokes latensor.py with all forwarded
# arguments.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# TVM build location — same default as benchmarking/unified/run_unified.sh
export TVM_HOME="${TVM_HOME:-$HOME/cosmos/cse/projects/tvm}"

# Activate the venv if present
#if [ -f "$HOME/tvm-venv/bin/activate" ]; then
    ## shellcheck disable=SC1091
    #source "$HOME/tvm-venv/bin/activate"
#fi

# Prefer the in-tree clang/opt the polly plugin was built against
LLVM_BIN="${REPO_ROOT}/llvm-project/build/bin"
if [ -d "${LLVM_BIN}" ]; then
    export PATH="${LLVM_BIN}:${PATH}"
fi

exec python3 "${SCRIPT_DIR}/latensor.py" "$@"
