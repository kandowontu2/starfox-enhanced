# Cross-compilation environment for the PS5 homebrew port. It builds the
# pinned Mihawk PS5 payload SDK fork, RADV (Mesa) and the game; nothing in it
# runs on the console.
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates clang-18 lld-18 llvm-18 cmake ninja-build git curl unzip \
    python3 python3-mako python3-yaml python3-ply python3-pip python3-venv \
    python3-packaging glslang-tools pkg-config make libssl-dev zlib1g-dev \
    rsync wget bison flex libclang-18-dev llvm-18-dev libclc-18-dev \
    libllvmspirvlib-18-dev spirv-tools libdrm-dev libzstd-dev libexpat1-dev \
    libclang-cpp18-dev libclang-rt-18-dev util-linux && rm -rf /var/lib/apt/lists/*
# Mesa 26.x needs Meson >= 1.4; Ubuntu 24.04 ships 1.3. The venv keeps the
# distribution's Mako/PyYAML/ply visible to Mesa's generators.
RUN python3 -m venv --system-site-packages /opt/meson \
    && /opt/meson/bin/pip install --no-cache-dir meson==1.7.2
ENV PATH="/opt/meson/bin:/usr/lib/llvm-18/bin:${PATH}"
ENV LLVM_CONFIG=/usr/bin/llvm-config-18
WORKDIR /src
