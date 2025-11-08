#!/usr/bin/env bash

set -euo pipefail

gcc -o scheched_coroutine.bench \
  scheched_coroutine.bench.c \
  ../basic_memory.c \
  ../coroutine/coroutine.c \
  scheched_coroutine.c \
  -I.. -pthread -std=c11 -Wall -Wextra -O2 -g

echo "Running scheched_coroutine.bench..."
./scheched_coroutine.bench

if [ $? -ne 0 ]; then
    echo "✗ scheched_coroutine.bench FAILED"
    exit 1
else
    echo "✓ scheched_coroutine.bench PASSED"
fi

echo "Deleting benchmark binary..."
rm -f scheched_coroutine.bench
