/* The MIT License (MIT)
Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h> /* uint32_t,size_t,uint8_t */
#include <stdlib.h> /* malloc,free */
#include <stdio.h> /* fprintf,printf,fopen,setvbuf,fclose,fread,fwrite,ferror,
                      [feof],snprintf */
#include <string.h> /* strlen */

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

/* These macros let you assign 4 byte string to 4 byte integer and
initialize static variable because the following line has undefined behavior and
has to be executed at run time.
    uint32_t riffMark = *(uint32_t *)"RIFF";
This is just overkill for 4 bytes:
    #includes <strings.h>
    memcpy(&riffMark, "RIFF", 4);
*/
/* char,char,char,char */
/* little endian */
#define string4ToInt32LE(a,b,c,d)  \
    (((a) &0xff) << 0*8) |         \
    (((b) &0xff) << 1*8) |         \
    (((c) &0xff) << 2*8) |         \
    (((d) &0xff) << 3*8)
/* big endian */
#define string4ToInt32BE(a,b,c,d)  \
    (((d) &0xff) << 0*8) |         \
    (((c) &0xff) << 1*8) |         \
    (((b) &0xff) << 2*8) |         \
    (((a) &0xff) << 3*8)
#undef string4ToInt32BE
#define string4ToInt32(a,b,c,d) string4ToInt32LE(a,b,c,d)

/* NOTE: relies on that pointers when cast from (uint8_t *) to (uint32_t *)
work as usual when dereferenced, which, as far as I can tell,
is Undefined Behavior, and can be compiled in unexpected ways
so, you should probably compile with -fno-strict-aliasing */
/* NOTE: in this program integers are only unsigned. Neat! */

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
        return -1;
    }
    static uint32_t riffMark = string4ToInt32('R','I','F','F');

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
        if(outName == NULL)
        {
            fprintf(stderr, "Error allocating memory?!\nAborting.\n");
            aborting = 1;
            /* free(NULL) should work */
            goto nextFile;
        }
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
                        if((ioStatus == 0) | (fclose(fileWriter) != 0))
                        {
                            fprintf(stderr,
                                    "%s: Error writing to a file.\nAborting.\n",
                                    outName);
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
                    /* TODO: read RIFF chunk length? */
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
