#!/bin/bash

if [ ! -d "bin" ]; then
    mkdir bin
fi

clang++ src/main.cc -o bin/zeus.o

if [ $? -eq 0 ]; then
    ./bin/zeus.o
fi