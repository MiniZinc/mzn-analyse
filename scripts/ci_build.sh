#!/usr/bin/env bash
# Configure, build and install mzn-analyse into $ROOT/mzn-analyse, against the
# libminizinc SDK and Gecode already in $ROOT (see fetch_minizinc.sh).
#
# Env: ROOT, CMAKE_GENERATOR, CMAKE_OSX_ARCHITECTURES, CMAKEDIR (where the SDK
# keeps its CMake packages: lib64/cmake on manylinux, lib/cmake on macOS,
# CMake on Windows)
set -eux

: "${ROOT:?ROOT must be set}"
: "${CMAKEDIR:?CMAKEDIR must be set}"

cmake -S "$ROOT" -B "$ROOT/build" -G "${CMAKE_GENERATOR:-Ninja}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_SYSTEM_LIBMINIZINC=1 \
  -Dlibminizinc_DIR="$ROOT/minizinc/${CMAKEDIR}/libminizinc" \
  -DGecode_ROOT="$ROOT/vendor/gecode" \
  -DOsiCBC_ROOT="$ROOT/vendor/cbc" \
  -DCMAKE_INSTALL_PREFIX="$ROOT/mzn-analyse" \
  -DCMAKE_OSX_ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES:-arm64}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="12.0"
cmake --build "$ROOT/build" --config Release --target install
