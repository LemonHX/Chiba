#!/usr/bin/env bash

export CFLAGS="-I.. -pthread -std=c11 -Wall -Wextra -O3 -g"
export SOURCES="../basic_memory.c thread_pool.c"
export VALGRIND_FLAGS="--leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 --suppressions=/dev/null"

chmod +x ../chiba_testing_boot.sh
../chiba_testing_boot.sh
