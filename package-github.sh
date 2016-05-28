#!/bin/sh
tar -cjf wwise-adpcm-to-wave-adpcm.linux.x86.tar.bz2 extract.x86 reformat.x86 LICENSE.txt README.md
tar -cjf wwise-adpcm-to-wave-adpcm.linux.x86_64.tar.bz2 extract.x86_64 reformat.x86_64 LICENSE.txt README.md
zip wwise-adpcm-to-wave-adpcm.windows.x86.zip extract.x86.exe reformat.x86.exe LICENSE.txt README.md
zip wwise-adpcm-to-wave-adpcm.windows.x86_64.zip extract.x86_64.exe reformat.x86_64.exe LICENSE.txt README.md
