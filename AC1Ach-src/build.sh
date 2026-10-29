#!/bin/sh
# Needs a 32-bit MinGW-w64 compiler (Linux: gcc-mingw-w64-i686; Windows: MSYS2 mingw32 toolchain).
i686-w64-mingw32-gcc -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -shared -o AC1Ach.asi src/*.c -lwinmm -lshell32 -static-libgcc
