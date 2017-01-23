/* The MIT License (MIT)
Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* size of input and output buffers used by the program */
#ifdef BUFSIZ
#define BUFFER_SIZE BUFSIZ
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
and (uint16_t *)
work as usual when dereferenced, which, as far as I can tell,
is Undefined Behavior, and can be compiled in unexpected ways
so, you should probably compile with -fno-strict-aliasing */

static size_t findChunk(uint32_t chunkMark, FILE *file, char *fileName)
{
    size_t chunkLength = 0;
    for(size_t ioStatus = 1; ioStatus > 0;)
    {
        uint32_t chunkHead[2];
        ioStatus = fread(chunkHead, 1, 4*2, file);
        if(ioStatus < 4*2)
        {
            printf("%s: Can't find \"%.4s\" chunk.\n",
                    fileName, (char *)&chunkMark);
            break;
        }
        if(chunkHead[0] == chunkMark)
        {
            chunkLength = chunkHead[1];
            return chunkLength;
        }
        if(fseek(file, chunkHead[1], SEEK_CUR))
        {
            printf("%s: Can't read enough of file.\n", fileName);
            break;
        }
    }
    return chunkLength;
}

/* TODO: if input is directory, process all the files inside */
/* TODO: if stored length is less then current file length, make file smaller */
/* TODO: account stored length of the file */
int main(int argc, char **argv)
{
    static uint32_t riffMark = string4ToInt32('R','I','F','F');
    static uint32_t waveMark = string4ToInt32('W','A','V','E');
    static uint32_t  fmtMark = string4ToInt32('f','m','t',' ');
    static uint32_t dataMark = string4ToInt32('d','a','t','a');
    if(argc < 2)
    {
        printf("Usage: %s FileName1 [FileName2...]\n"
                "Assumes provided files are Wwise IMA ADPCM wave files,\n"
                "changes CONTENTS of the provided files in a way\n"
                "that lets them be read by complete IMA ADPCM decoders (for example, SoX).\n"
                "Version r4\n"
                , argv[0]);
        return -1;
    }
    static uint16_t waveId = 0x0011;
    for(size_t i = 1; i < (size_t)argc; ++i)
    {
        FILE *file = fopen(argv[i], "r+b");
        if(file == NULL)
        {
            printf("%s: Error opening file.\n", argv[i]);
            continue;
        }
        /* NOTE: turn buffered i/o off because
         * 1) program is already going to do that;
         * 2) C runtime library might do something weird. See another note later*/
        if(setvbuf(file, NULL, _IONBF, 0))
        {
            printf("%s: Error using file?!\n", argv[i]);
            fclose(file); continue;
        }
        size_t ioStatus = 1;

        uint32_t riffLenght = 0;
        {
            uint32_t magick[3] = {0,0,0};
            ioStatus = fread(magick, 1, 4*3, file);
            if(ioStatus == 0)
            {
                printf("%s: Can't read file.\n", argv[i]);
                fclose(file); continue;
            }
            if(ioStatus < 4*3)
            {
                printf("%s: File is too small.\n", argv[i]);
                fclose(file); continue;
            }
            if(magick[0] != riffMark)
            {
                printf("%s: Wrong format. Expected \"RIFF\", got \"%.4s\"\n",
                        argv[i], (char *)magick);
                fclose(file); continue;
            }

            riffLenght = magick[1];

            if(magick[2] != waveMark)
            {
                printf("%s: Can't recognize format. Got \"RIFF\", "
                        "but no \"WAVE\". Got \"%.4s\" instead\n",
                        argv[i], (char *)(magick+2));
                fclose(file); continue;
            }
        }

        size_t fmtLength = findChunk(fmtMark, file, argv[i]);
        if(fmtLength == 0)
        {
            fclose(file); continue;
        }
        if(fmtLength != 24)
        {
            printf("%s: File has wrong format of data. "
                    "fmt chunk is of unexpected size (%u bytes).\n",
                    argv[i], fmtLength);
            fclose(file); continue;
        }

        /* IMA ADPCM fmt chunk as described in specification
           fmtLength = 20;
           0 rw uint16_t wFormatTag = 0x0011;
           2  r uint16_t nChannels;
           4    uint32_t nSamplesPerSec;
           8    uint32_t nAvgBytesPerSec = (rounded up)
                                  = nBlockAlign*nSamplesPerSec/wSamplesPerBlock;
                  nBlockAlign -- bytes per block
           12 r uint16_t nBlockAlign [usually] = (N+1)*4*nChannels, N=0,1,2...;
           14 r uint16_t wBitsPerSample [usually] = 4;
           16   uint16_t cbSize = 2;
           18 w uint16_t wSamplesPerBlock =
           = (nBlockAlign-4*nChannels)*8/(wBitsPerSample*nChannels)+1 =
           = [usually] N*8+1 */
        /* if(wBitsPerSample==3) nBlockAlign = ((N*3)+1)*4*nChannels;
           uint16_t wSamplesPerBlock = N*4*8+1 = N*32+1 */

        /* Wwise IMA ADPCM fmt chunk as it appears in the wild
           fmtLength = 0x18 = 24;
           0  uint16_t wFormatTag = 0x0002;
           2  uint16_t nChannels;
           4  uint32_t nSamplesPerSec;
           8  uint32_t nAvgBytesPerSec;
           12 uint16_t nBlockAlign = (0x0024 = 36)*nChannels =
                                   = (N+1)*4*nChannels, N = 8;
           14 uint16_t wBitsPerSample = 4;
           16 uint16_t cbSize = 6;
           18 uint16_t unknown;
           20 uint32_t dwChannelMask */

        /* IMA ADPCM fmt chunk adapted to multichannel wave specification
           TODO: add short description */

        long fmtOffset = ftell(file);
        if(fmtOffset < 0)
        {
            printf("%s: Error using file?!\n", argv[i]);
            fclose(file); continue;
        }
        /* fmtLength == 24 */
        static uint8_t fmtDescription[24];
        ioStatus = fread(fmtDescription, 1, 24, file);
        if(ioStatus < 24)
        {
            printf("%s: Can't read fmt chunk.\n", argv[i]);
            fclose(file); continue;
        }
        uint16_t channels = *(uint16_t *)(fmtDescription+2),
                 align = *(uint16_t *)(fmtDescription+12);

        {
            uint16_t formatID = *(uint16_t *)fmtDescription,
                     sampleBits = *(uint16_t *)(fmtDescription+14),
                     extraSize = *(uint16_t *)(fmtDescription+16);
            /* probably shouldn't accept 0x11 */
            /* (formatID == 0x0002 || formatID == 0x0011) */
            if(!( formatID == 0x0002 && sampleBits == 4 &&
                        extraSize == 6 ))
            {
                printf("%s: File doesn't seem to be (Wwise) IMA ADPCM wave."
                        " It has format ID 0x%04x, bits per sample %u, "
                        "%u bytes of extra format info.\n",
                        argv[i], formatID, sampleBits, extraSize);
                fclose(file); continue;
            }
            /* wFormatTag */
            *(uint16_t *)fmtDescription = waveId;
            /* wSamplesPerBlock */
            *(uint16_t *)(fmtDescription+18) = (align-4*channels)*8/(sampleBits*channels)+1;

            /* TODO: dwChannelMask */
            /* NOTE: in Wwise files, when there is only one channel, data is
             * structured the same way as described in IMA ADPCM specification,
             * when there is more -- data is "clustered", but samples (not samples,
             * but uint32 words worth of data) are not interleaved, they are
             * clustered one channel after another */
#if 0
            static uint8_t *extraSection = fmtDescription+18;
            printf("%s ch:%u ", argv[i], channels);
            for(size_t n = 0; n < extraSize; ++n)
                printf("%02x", extraSection[n]);
            printf("\n");
#endif

            if(fseek(file, fmtOffset, SEEK_SET))
            {
                printf("%s: Error using file?!\n", argv[i]);
                fclose(file); continue;
            }
            ioStatus = fwrite(fmtDescription, 1, 24, file);
            if(ioStatus == 0)
            {
                printf("%s: Error writing to file.\nAborting.\n", argv[i]);
                fclose(file);
                return 1;
            }
        }
        if(ioStatus < 24)
        {
            printf("%s: Error writing to file.\nAborting.\n", argv[i]);
            fclose(file);
            return 1;
        }

        if(channels > 1)
        { /* shuffle data to make it interleaved */
                if(fseek(file, 4*3, SEEK_SET))
                {
                    printf("%s: Error using file?!\n", argv[i]);
                    fclose(file); continue;
                }
                /* find "data" chunk */
                size_t dataLength = findChunk(dataMark, file, argv[i]);
                if(dataLength == 0)
                {
                    printf("%s: File has no usable data...\n", argv[i]);
                    fclose(file); continue;
                }
                static uint8_t transformInData[BUFFER_SIZE], transformOutData[BUFFER_SIZE];
                uint8_t *transformIn = transformInData,
                        *transformOut = transformOutData;
                /* how much blocks of data can be placed in buffers (integer divide) */
                size_t transformCount = BUFFER_SIZE / align;
                if(align > BUFFER_SIZE) /* just in case */
                {
                    transformIn = malloc(2*align);
                    transformOut = transformIn + align;
                    transformCount = 1;
                }
                    for(size_t blockCount = 0;
                            blockCount*align < dataLength; )
                    {
                        /* remember the place for the later write */
                        long dataPoint = ftell(file);
                        if(dataPoint < 0)
                        {
                            printf("%s: Error using file?!\n", argv[i]);
                            break;
                        }
                        /* NOTE: Example of C runtime library weirdness
                         * (probably a bug, Linux x86-64, glibc 2.23): */
                        ioStatus = fread(transformIn, align, transformCount, file);
                        /* if align*transformCount is more then 4096 (8192)
                         * with buffering set to default
                         * ioStatus is always assigned transformCount and
                         * ftell(file) is only increased by 4096. */
#if 0
                        printf("read: %zu\n", ftell(file) - dataPoint);
#endif
                        if(ioStatus == 0)
                        {
                            printf("%s: Unexpected end of file.\n", argv[i]);
                            break;
                        }
                        /* should tell you how much blocks of data
                         * the program have read */
                        size_t blockAmount = ioStatus;
                        /* NOTE: THIS IS WHERE WE INCREMENT THE LOOP INDEX */
                        blockCount += blockAmount;
                        /* NOTE: assumes nBlockAlign is multiple of nChannels
                         * as required by the spec */
                        for(size_t block = 0; block < blockAmount; ++block)
                            for(size_t n = 0;
                                    /* NOTE: there is 4 in here because
                                     * data is copied 4 bytes at a time */
                                    n < align/(channels*4);
                                    ++n)
                                for(size_t s = 0; s < channels; ++s)
                                {
                                    ((uint32_t *)(transformOut+block*align))[n*channels+s] =
                                        ((uint32_t *)(transformIn+block*align))[s*align/(channels*4)+n];
                                }
                        if(fseek(file, dataPoint, SEEK_SET))
                        {
                            printf("%s: Error using file?!\n", argv[i]);
                            break;
                        }
                        ioStatus = fwrite(transformOut, (blockAmount*align), 1, file);
                        if(ioStatus == 0)
                        {
                            printf("%s: Error writing file.\nAborting.\n", argv[i]);
                            fclose(file); return 1;
                        }
                    }
                if(align > BUFFER_SIZE) /* just in case */
                {
                    free(transformIn);
                }
        }
        fclose(file);
        printf("Finished processing %s\n", argv[i]);
    }
    return 0;
}
