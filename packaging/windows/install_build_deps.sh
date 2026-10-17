#!/usr/bin/env bash
# Installs what the Windows build needs on Ubuntu 24.04 (the release
# workflow's runner, .github/workflows/build.yml), as root: clang and lld
# with the mingw-w64 runtime (cmake/mingw-clang.cmake), CMake and Ninja.
# Adapted from SilentHeritage's.
#
# Usage: sudo packaging/windows/install_build_deps.sh
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends ca-certificates git cmake ninja-build python3 zip \
  clang lld llvm mingw-w64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
