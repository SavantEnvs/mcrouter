#!/usr/bin/env bash
# mayhem/build.sh — build mcrouter's fuzz targets and its unit tests for those sources.
# Runs as `mayhem` in /mayhem. Offline and re-runnable: the only external dependency beyond apt is
# upstream's facebook dependency chain (folly, fizz, wangle, mvfst, fbthrift), prebuilt in the image
# under /opt/toolchains/deps/ at the revisions pinned in build/deps/github_hashes/. The mcrouter sources are compiled here, from the
# tree, with $SANITIZER_FLAGS (fuzz build) and with normal flags (test build), via mayhem/CMakeLists.txt.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:?base image must provide STANDALONE_FUZZ_MAIN}"
export CC CXX

cd "$SRC"
DEPS="/opt/toolchains/deps/folly;/opt/toolchains/deps/fizz;/opt/toolchains/deps/wangle;/opt/toolchains/deps/mvfst;/opt/toolchains/deps/fbthrift"

# fuzz-no-link coverage on the project sources only when building with libFuzzer
NOLINK=""
case "$LIB_FUZZING_ENGINE" in *fuzzer*) NOLINK="-fsanitize=fuzzer-no-link" ;; esac

# 1) Sanitized fuzz build: mcrouter sources + harnesses (libFuzzer and standalone reproducers).
#    LeakSanitizer is off for every harness (leaks are not this fleet's bug class): lsan_off.cc,
#    compiled with $SANITIZER_FLAGS, is linked into each fuzz and -standalone binary.
mkdir -p mayhem/build-fuzz
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o mayhem/build-fuzz/lsan_off.o
cmake -S mayhem -B mayhem/build-fuzz -G Ninja -DMCR_MODE=fuzz \
  -DCMAKE_PREFIX_PATH="$DEPS" -DCMAKE_BUILD_TYPE=None \
  -DCMAKE_C_FLAGS="-O1 $SANITIZER_FLAGS $NOLINK $DEBUG_FLAGS" \
  -DCMAKE_CXX_FLAGS="-O1 $SANITIZER_FLAGS $NOLINK $DEBUG_FLAGS" \
  -DSTANDALONE_MAIN="$STANDALONE_FUZZ_MAIN" -DLSAN_OFF_OBJ="$SRC/mayhem/build-fuzz/lsan_off.o" -DFUZZ_ENGINE="$LIB_FUZZING_ENGINE"
HARNESSES="fuzz_lz4immutable fuzz_config_preprocessor fuzz_server_parser fuzz_client_parser"
targets=""; for h in $HARNESSES; do targets="$targets $h $h-standalone"; done
cmake --build mayhem/build-fuzz -j"$MAYHEM_JOBS" --target $targets
for h in $HARNESSES; do
  install -m 0755 "mayhem/build-fuzz/$h" "/mayhem/$h"
  install -m 0755 "mayhem/build-fuzz/$h-standalone" "/mayhem/$h-standalone"
done

# 2) Unit tests (upstream's gtest suites for the same sources), normal flags.
cmake -S mayhem -B mayhem/build-test -G Ninja -DMCR_MODE=test \
  -DCMAKE_PREFIX_PATH="$DEPS" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build mayhem/build-test -j"$MAYHEM_JOBS"
