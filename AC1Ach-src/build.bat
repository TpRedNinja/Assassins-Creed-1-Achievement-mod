@echo off
rem Needs a 32-bit MinGW-w64 gcc on PATH (e.g. MSYS2 mingw32 shell).
gcc -O2 -Wall -shared -o AC1Ach.asi ac1ach.c -lwinmm -lshell32 -static-libgcc
