#!/bin/bash

if [ ! -d "bin/lin/" ]; then
    mkdir bin/lin
fi

clang++ src/main.cc --debug -o bin/lin/zeus.o -D LIN=1

if [ $? -eq 0 ]; then
    ./bin/lin/zeus.o
fi