#!/bin/sh
gcc  -o extract.x86_64 -m64 -mtune=generic -O2 extract.c
gcc -o reformat.x86_64 -m64 -mtune=generic -O2 reformat.c
gcc  -o extract.x86 -m32 -mtune=generic -O2 extract.c
gcc -o reformat.x86 -m32 -mtune=generic -O2 reformat.c
i686-w64-mingw32-gcc  -o extract.x86.exe -mtune=generic -O2 extract.c
i686-w64-mingw32-gcc -o reformat.x86.exe -mtune=generic -O2 reformat.c
x86_64-w64-mingw32-gcc  -o extract.x86_64.exe -mtune=generic -O2 extract.c
x86_64-w64-mingw32-gcc -o reformat.x86_64.exe -mtune=generic -O2 reformat.c
