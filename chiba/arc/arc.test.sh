#!/usr/bin/env bash

export CFLAGS="-I.. -pthread -std=c11 -Wall -Wextra -O2 -g -Wno-macro-redefined"
export SOURCES="../basic_memory.c"
export VALGRIND_FLAGS="--leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 --suppressions=/dev/null"
export USE_WASM=true

chmod +x ../chiba_testing_boot.sh
../chiba_testing_boot.sh