# Use a recent Ubuntu base (Docker automatically pulls arm64 for Mac, x86 for Linux)
FROM ubuntu:24.04

# Avoid timezone prompts during apt installations
ENV DEBIAN_FRONTEND=noninteractive

# 1. Install system dependencies and build tools
RUN apt-get update && apt-get install -y \
    wget \
    git \
    build-essential \
    cmake \
    ninja-build \
    software-properties-common \
    lsb-release \
    gnupg \
    zlib1g-dev \
    libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Install LLVM 21 (llvm.sh automatically detects arm64 vs x86!)
RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 21 all && \
    rm llvm.sh

# Symlink clang-21 and opt-21 to standard commands
RUN ln -s /usr/bin/clang-21 /usr/bin/clang && \
    ln -s /usr/bin/clang++-21 /usr/bin/clang++ && \
    ln -s /usr/bin/opt-21 /usr/bin/opt && \
    ln -s /usr/bin/FileCheck-21 /usr/bin/FileCheck

# 3. Install Miniconda (ARCHITECTURE AWARE)
ENV CONDA_DIR=/opt/conda
RUN ARCH=$(uname -m) && \
    echo "Detected architecture: $ARCH" && \
    if [ "$ARCH" = "x86_64" ]; then \
        MINICONDA_URL="https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh"; \
    elif [ "$ARCH" = "aarch64" ]; then \
        MINICONDA_URL="https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-aarch64.sh"; \
    else \
        echo "Unsupported architecture: $ARCH" && exit 1; \
    fi && \
    wget --quiet $MINICONDA_URL -O ~/miniconda.sh && \
    /bin/bash ~/miniconda.sh -b -p /opt/conda && \
    rm ~/miniconda.sh
ENV PATH=$CONDA_DIR/bin:$PATH

# 4. Setup TVM Conda Environment
COPY environment.yml /tmp/environment.yml
RUN conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main
RUN conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/r
RUN conda env create -f /tmp/environment.yml && \
    conda clean -a -y

# Make RUN commands use the new environment
SHELL ["conda", "run", "-n", "tvm-build-venv", "/bin/bash", "-c"]

# 5. Setup Workspace
WORKDIR /workspace

# Set TVM paths relative to the Docker workspace
ENV TVM_HOME=/workspace/tvm
ENV PYTHONPATH=/workspace/tvm/python:$PYTHONPATH

# By default, start a bash shell with the conda environment activated
ENTRYPOINT ["conda", "run", "--no-capture-output", "-n", "tvm-build-venv", "/bin/bash"]
