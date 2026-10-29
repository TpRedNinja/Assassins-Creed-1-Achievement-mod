@echo off
rem Needs a 32-bit MinGW-w64 gcc on PATH (e.g. MSYS2 mingw32 shell).
gcc -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -shared -o AC1Ach.asi src\*.c -lwinmm -lshell32 -static-libgcc
