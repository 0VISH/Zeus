#!/bin/bash

if [ ! -d "bin/lin/" ]; then
    mkdir bin/lin
fi

#clang++ src/main.cc -O2 -march=native -o bin/lin/zeus.o -D LIN=1 -D SIMD=1
clang++ src/main.cc -o bin/lin/zeus.o -D LIN=1 -D SIMD=1

if [ $? -eq 0 ]; then
    bin/win/zeus.exe test/t1.zs bin/lin/out.asm
    riscv64-linux-gnu-as bin/lin/out.asm -o bin/lin/out.o
fi