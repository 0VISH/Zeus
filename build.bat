@echo off

if not exist bin\win\ (
    mkdir bin\win\
)

clang++ src/main.cc --debug -o bin/win/zeus.exe -D WIN=1

if %errorlevel% equ 0 (
    bin\win\zeus.exe test/t1.zs
)