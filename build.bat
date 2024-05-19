@echo off

if not exist bin\win\ (
    mkdir bin\win\
)

cl /nologo src/main.cc /Zi /Fo:bin/win/zeus.obj /Fe:bin/win/zeus.exe /Fd:bin/win/zeus.pdb /D WIN=1 /D DBG=1 /D SIMD=1

if %errorlevel% equ 0 (
    bin\win\zeus.exe test/t1.zs bin/win/out.asm
)