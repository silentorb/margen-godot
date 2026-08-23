FROM mcr.microsoft.com/devcontainers/cpp:1-debian-12

# Shares the C++ toolchain style with margen. godot-cpp will be added when the
# extension is implemented; until then this image is enough to configure CMake.
RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake make g++ git \
    && rm -rf /var/lib/apt/lists/*
