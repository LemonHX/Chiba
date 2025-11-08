#!/usr/bin/env bash

export CFLAGS="-I.. -std=c11 -Wall -Wextra -O2 -g"
export SOURCES="../basic_memory.c coroutine.c"
export VALGRIND_FLAGS="--leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 --suppressions=/dev/null"
# export USE_WASM=true
chmod +x ../chiba_testing_boot.sh
../chiba_testing_boot.sh
