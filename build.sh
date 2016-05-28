#!/bin/sh
# Compile executables for and only for current machine,
# produced executables may not work on other computers.
# clang warnings are nicer, maybe
clang  -o extract -march=native -mtune=native -O2 -Wall extract.c
clang -o reformat -march=native -mtune=native -O2 -Wall reformat.c
# example of usage:
# /path/to/extract resources to extract from | xargs /path/to/reformat > log.log
