FROM mcr.microsoft.com/devcontainers/cpp:1-debian-12

# GDExtension toolchain: CMake/godot-cpp (Python binding generation) plus headers/libs
# consumed from margen_ffi (Rust — installed via devcontainer Rust feature).
RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		build-essential \
		cmake \
		ninja-build \
		python3 \
		git \
		ca-certificates \
	&& rm -rf /var/lib/apt/lists/*
