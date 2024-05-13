#!/bin/bash

if [ ! -d "bin/lin/" ]; then
    mkdir bin/lin
fi

clang++ src/main.cc -O2 -march=native -o bin/lin/zeus.o -D LIN=1 -D SIMD=1