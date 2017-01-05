/* The MIT License (MIT)
Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* size of input/output buffer used by the program */
/* NOTE: the minimal size of the resulting buffer is 4+3
   this construct is here to show theoretical limitation of the algorithm
   memory usage */
#ifdef BUFSIZ
#if BUFSIZ >= 4
#define BUFFER_SIZE BUFSIZ
#else
#define BUFFER_SIZE 4
#endif /* BUFSIZ >= 4 */
#else
#define BUFFER_SIZE 4096
#endif /* BUFSIZ */

/* TODO: if input is directory, process all the files inside */
/* TODO: let the program take input from stdin */
int main(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s FileName1 [FileName2...]\n"
                "Extracts anything resembling wave (RIFF) files and all data stored after that from provided resource files,\n"
                "places extracted files in the same directory where input files are stored.\n"
                /* "ignores stored length of data.\n" */
                "Version r4\n"
                , argv[0]);
        return 1;
    }
    uint32_t riffMark = *(uint32_t *)"RIFF";
    for(size_t i = 1; i < (size_t)argc; ++i)
    {
        FILE *file = fopen(argv[i], "rb");
        if(file == NULL)
        {
            fprintf(stderr, "%s: Error opening file.\n", argv[i]);
            continue;
        }
        /* NOTE: turn buffered i/o off because
         * 1) program is already going to do that;
         * 2) C runtime library might do something weird. See note in reformat.c*/
        if(setvbuf(file, NULL, _IONBF, 0))
        {
            fprintf(stderr, "%s: Error using file?!\n", argv[i]);
            fclose(file);
            continue;
        }
        uint8_t aborting = 0;
        static uint8_t readFile[BUFFER_SIZE + 3];
        *(uint32_t *)readFile = ~riffMark; /* definitely not a "RIFF" */

        /* name_%08x.wav */
        size_t outNameLength = strlen(argv[i]) + 5 + 16 + 1;
        char *outName = malloc(outNameLength);
        size_t offset = -3; /* starts at 3 bytes rollback,
                               size_t is unsigned, overflow is defined */
        FILE *fileWriter = NULL;

        /* read file, split it by RIFF marks */
        while(1)
        {
            size_t ioStatus = fread(readFile+3, 1, BUFFER_SIZE, file);
            uint8_t *writeFile = readFile,
                    *inspectPointer = readFile,
                    *inspectEnd = readFile + ioStatus;
            if(ioStatus == 0) /* file has ended or there is error */
            {
                if(fileWriter) /* output the last 3 bytes of the input file */
                {
                    fwrite(readFile, 3, 1, fileWriter);
                    /* there is check for errors just outside the loop */
                }

                if(ferror(file))
                {
                    fprintf(stderr, "%s: Error reading from a file.\n",
                            argv[i]);
                } /* else file is at EOF, which is good */
                break;
            }

            /* NOTE: the program searches for RIFF files and assumes that
               RIFF chunk length can be shorter then recorded,
               so when there is another "RIFF" sequence
               it just starts writing another file */
            /* TODO: stop writing at other RIFF file _or_
               at recorded RIFF length */
            /* includes rollback check */
            for(; inspectPointer < inspectEnd;
                    ++inspectPointer)
            {
                /* when it finds "RIFF", finish writing previous file
                 * and open new file for writing */
                if(*(uint32_t *)inspectPointer == riffMark)
                {
                    if(fileWriter)
                    {
                        ioStatus = fwrite(writeFile,
                                inspectPointer - writeFile, 1, fileWriter);
                        /* try to close the file in any case */
                        if(ioStatus == 0)
                        {
                            fprintf(stderr,
                                    "%s: Error writing to a file.\nAborting.\n",
                                    outName);
                            fclose(fileWriter);
                            aborting = 1;
                            goto nextFile;
                        }
                        fprintf(stdout, "%s\n", outName);
                    }
                    offset += inspectPointer - writeFile;
                    snprintf(outName, outNameLength,
                            "%s_%08lx.wav", argv[i], offset);
                    writeFile = inspectPointer;
                    fileWriter = fopen(outName, "wb");
                    if(fileWriter == NULL)
                    {
                        fprintf(stderr, "%s: Error creating file.\nAborting.\n",
                                outName);
                        aborting = 1;
                        goto nextFile;
                    }
                    inspectPointer += 3;
                    /* TODO: read RIFF chunk lenght? */
                }
            }
            if(fileWriter)
            {
                ioStatus = fwrite(writeFile,
                        inspectPointer - writeFile, 1, fileWriter);
                if(ioStatus == 0)
                {
                    /* there is an error, it should have set ferror() */
                    /* there is check for errors just outside the loop */
                    break;
                }
            }
            offset += inspectPointer - writeFile;
            readFile[0] = inspectEnd[0],
            readFile[1] = inspectEnd[1],
            readFile[2] = inspectEnd[2];
        }
        /* check for errors and try to close the file */
        if(fileWriter && (ferror(fileWriter) | fclose(fileWriter)))
        {
            fprintf(stderr, "%s: Error writing to a file.\nAborting.\n",
                    outName);
            aborting = 1;
            goto nextFile;
        }
        fprintf(stdout, "%s\n", outName);

nextFile:
        free(outName);
        fclose(file);
        if(aborting) return 1;
    }
    return 0;
}
