#!/usr/bin/env bash

gcc -o thread_pool.bench thread_pool.bench.c ../basic_memory.c thread_pool.c -I.. -pthread -std=c11 -Wall -Wextra -O2 -g

echo "Running thread_pool.bench..."
./thread_pool.bench

if [ $? -ne 0 ]; then
    echo "✗ thread_pool.bench FAILED"
    exit 1
else
    echo "✓ thread_pool.bench PASSED"
fi

echo "Deleting benchmark binary..."
rm -f thread_pool.bench