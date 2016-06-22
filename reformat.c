/* The MIT License (MIT)
Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

/* size of input and output buffers used by the program */
#define BUFFER_SIZE BUFSIZ

/* TODO: if input is directory, process all the files inside */
/* TODO: if stored length is less then current file length, make file smaller*/
int main(int argc, char **argv)
{
    if(argc < 2)
    {
	printf("Usage: %s FileName1 [FileName2...]\n"
		"Assumes provided files are Wwise IMA ADPCM wave files,\n"
		"changes CONTENTS of the provided files in a way\n"
		"that lets them be read by normal IMA ADPCM decoders (for example, SoX).\n"
		"Version r3\n"
		, argv[0]);
	return 1;
    }
    static uint16_t waveId = 0x11;
    for(size_t i = 1; i < argc; ++i)
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
	    fclose(file);
	    continue;
	}
	size_t ioStatus = 1;
	{
	    uint32_t magick[3] = {0,0,0};
	    ioStatus = fread(magick, 4, 3, file);
	    if(ioStatus == 0)
	    {
		printf("%s: Can't read enough of file.\n", argv[i]);
		fclose(file);
		continue;
	    }

	    if(magick[0] != *(uint32_t *)"RIFF")
	    {
		printf("%s: Wrong format.\n",
			argv[i]);
		fclose(file);
		continue;
	    }
	    if(magick[2] != *(uint32_t *)"WAVE")
	    {
		printf("%s: Can't recognize format. Maybe concatenate with previous file?\n",
			argv[i]);
		fclose(file);
		continue;
	    }
	}
	size_t fmtLength = 0;
	/* find "fmt " chunk */
	while(ioStatus > 0)
	{
	    uint32_t chunkHead[2];
	    ioStatus = fread(chunkHead, 4, 2, file);
	    if(ioStatus == 0)
	    {
		printf("%s: Can't read enough of file.\n", argv[i]);
		break;
	    }
	    if(chunkHead[0] == *(uint32_t *)"fmt ")
	    {
		fmtLength = chunkHead[1];
		break;
	    }
	    if(fseek(file, chunkHead[1], SEEK_CUR))
	    {
		printf("%s: Can't read enough of file.\n", argv[i]);
		break;
	    }
	}
	if(fmtLength == 0)
	{
	    fclose(file);
	    continue;
	}
	if(fmtLength < 20)
	{
	    printf("%s: File has wrong format of data.\n", argv[i]);
	    fclose(file);
	    continue;
	}
	size_t fmtOffset = ftell(file);
	/* IMA ADPCM fmt chunk as described in specification
	uint16_t wFormatTag;       w 0
	uint16_t nChannels;        r 2
	uint32_t nSamplesPerSec;     4
	uint32_t nAvgBytesPerSec;    8
	uint16_t nBlockAlign;      r 12
	uint16_t wBitsPerSample;   r 14
	uint16_t cbSize;             16
	uint16_t wSamplesPerBlock; w 18
	(nBlockAlign-4*nChannels)*8/(wBitsPerSample*nChannels)+1
	*/

	uint8_t fmtDescription[18];
	ioStatus = fread(fmtDescription, 18, 1, file);
	if(ioStatus == 0)
	{
	    printf("%s: Can't read enough of file.\n", argv[i]);
	    fclose(file);
	    continue;
	}
	uint16_t formatID = *(uint16_t *)fmtDescription;
	uint16_t channels = *(uint16_t *)(fmtDescription+2);
	uint16_t align = *(uint16_t *)(fmtDescription+12);
	uint16_t sampleBits = *(uint16_t *)(fmtDescription+14);
	if(!(( formatID == 0x02 || formatID == 0x11) && sampleBits == 4))
	{
	    printf("%s: File doesn't seem to be (Wwise) IMA ADPCM wave."
		    " It has format ID 0x%02x and bit depth %u.\n",
		    argv[i], formatID, sampleBits);
	    fclose(file);
	    continue;
	}

	/* NOTE: not all Wwise WAVE files are right,
	 * some of them are marked bigger size then they are.
	 * NOTE: Content of extra section in fmt chunk (extended header)
	 * cbSize is 6
	 * 0x000004000000:
	 * file contains normal IMA ADPCM (don't change data chunk!)
	 *
	 * 0x000003000000
	 * 0x18000b000000
	 * 0x18000c000000
	 * 0x10000b000000
	 * 0x10000c000000
	 * "clustered" IMA ADPCM
	 * it is IMA ADPCM, but samples
	 * (not samples but uint32 words worth of data) are not interleaved,
	 * they are clustered one channel after another */
	uint16_t extraSize = *(uint16_t *)(fmtDescription+16);
	static uint8_t extraSectionData[6];
	uint8_t *extraSection;
	if(extraSize <= 6)
	    extraSection = extraSectionData;
	else
	    extraSection = malloc(extraSize);
	ioStatus = fread(extraSection, extraSize, 1, file);
	if(ioStatus == 0)
	{
	    printf("%s: Can't read enough of file.\n", argv[i]);
	    if(extraSize > 6) free(extraSection);
	    fclose(file);
	    continue;
	}
#if 0
	printf("%s ch:%u ", argv[i], channels);
	for(size_t n = 0; n < extraSize; ++n)
	    printf("%02x", extraSection[n]);
	printf("\n");
#endif

	if(fseek(file, fmtOffset, SEEK_SET))
	{
	    printf("%s: Error using file?!\n", argv[i]);
	    if(extraSize > 6) free(extraSection);
	    fclose(file);
	    continue;
	}
	ioStatus = fwrite(&waveId, 2, 1, file);
	if(ioStatus == 0)
	{
	    printf("%s: Error writing to file.\n", argv[i]);
	    if(extraSize > 6) free(extraSection);
	    fclose(file);
	    continue;
	}

	if(fseek(file, fmtOffset+18, SEEK_SET))
	{
	    printf("%s: Error using file?!\n", argv[i]);
	    if(extraSize > 6) free(extraSection);
	    fclose(file);
	    continue;
	}
	uint16_t sampleBlock = (align-4*channels)*8/(sampleBits*channels)+1;
	ioStatus = fwrite(&sampleBlock, 2, 1, file);
	if(ioStatus == 0)
	{
	    printf("%s: Error writing to file.\n", argv[i]);
	    if(extraSize > 6) free(extraSection);
	    fclose(file);
	    continue;
	}

	if(channels > 1 && extraSize == 6)
	{
	    uint32_t extraSectionPart1 = *(uint32_t *)extraSection;
	    uint16_t extraSectionPart2 = *(uint16_t *)(extraSection+4);
	    size_t comparison;
#if 0
	    comparison = 0;
	    static uint8_t compare[][6] = {
	    {0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	    {0x18, 0x00, 0x0b, 0x00, 0x00, 0x00},
	    {0x18, 0x00, 0x0c, 0x00, 0x00, 0x00},
	    {0x10, 0x00, 0x0b, 0x00, 0x00, 0x00},
	    {0x10, 0x00, 0x0c, 0x00, 0x00, 0x00}};
	    for(size_t count = 0; count < sizeof(compare)/sizeof(uint8_t[6]);
		    ++count)
	    {
		if(extraSectionPart1 == *(uint32_t *)compare[count] &&
			extraSectionPart2 == *(uint16_t *)(compare[count]+4))
		{
		    comparison = 1;
		    break;
		}
	    }
#else
	    comparison = 1;
	    static uint8_t compare[][6] = {
	    /* probably only files with this as an extra section in fmt chunk
	     * don't need to change data packing */
	    {0x00, 0x00, 0x04, 0x00, 0x00, 0x00},
	    {0x41, 0x00, 0x04, 0x00, 0x00, 0x00} // processed files
	    };
	    for(size_t count = 0; count < sizeof(compare)/sizeof(uint8_t[6]);
		    ++count)
	    {
		if(extraSectionPart1 == *(uint32_t *)compare[count] &&
			extraSectionPart2 == *(uint16_t *)(compare[count]+4))
		{
		    comparison = 0;
		    break;
		}
	    }
#endif
	    if(extraSize > 6) free(extraSection);
	    if(comparison)
	    {
		/* shuffle data to make it interleaved */
		if(fseek(file, 12, SEEK_SET))
		{
		    printf("%s: Error using file?!\n", argv[i]);
		    fclose(file);
		    continue;
		}
		ioStatus = 1;
		size_t dataLength = 0;
		/* find "data" chunk */
		while(1)
		{
		    uint32_t chunkHead[2];
		    ioStatus = fread(chunkHead, 4, 2, file);
		    if(ioStatus == 0)
		    {
			printf("%s: Can't read enough of file.\n", argv[i]);
			break;
		    }
		    if(chunkHead[0] == *(uint32_t *)"data")
		    {
			dataLength = chunkHead[1];
			break;
		    }
		    /* Not a data chunk */
		    if(fseek(file, chunkHead[1], SEEK_CUR))
		    {
			printf("%s: Can't read enough of file.\n", argv[i]);
			break;
		    }
		}
		if(dataLength == 0)
		{
		    printf("%s: File has no usable data...\n", argv[i]);
		    fclose(file);
		    continue;
		}
		if(align <= BUFFER_SIZE)
		{
		    static uint8_t transformIn[BUFFER_SIZE];
		    static uint8_t transformOut[BUFFER_SIZE];
		    /* how much blocks of data can be placed in buffers */
		    size_t transformCount = BUFFER_SIZE/align;
		    /* how much space these blocks will take up */
		    size_t transformAmount = transformCount * align;
		    for(size_t count = 0;
			    count*transformAmount < dataLength;
			    ++count)
		    {
			/* remember the place for the later write */
			size_t dataPoint = ftell(file);
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
			 * program have read */
			size_t blockAmount = ioStatus;
			/* NOTE: assumes nBlockAlign is multiple of nChannels */
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
			    printf("%s: Error writing file.\n", argv[i]);
			    break;
			}
		    }
		} else { /* align > BUFFER_SIZE (just in case) */
		    uint32_t *transformIn = malloc(2*align);
		    uint32_t *transformOut = ((void *)transformIn) + align;
		    for(size_t count = 0; count < dataLength/align; ++count)
		    {
			/* remember the place for the later write */
			size_t dataPoint = ftell(file);
			ioStatus = fread(transformIn, align, 1, file);
			if(ioStatus == 0)
			{
			    printf("%s: Unexpected end of file.\n", argv[i]);
			    break;
			}
			/* NOTE: assumes nBlockAlign is multiple of nChannels */
			for(size_t n = 0; n < align/(channels*4); ++n)
			    for(size_t s = 0; s < channels; ++s)
			    {
				transformOut[n*channels+s] =
				    transformIn[s*align/(channels*4)+n];
			    }
			if(fseek(file, dataPoint, SEEK_SET))
			{
			    printf("%s: Error using file?!\n", argv[i]);
			    break;
			}
			ioStatus = fwrite(transformOut, align, 1, file);
			if(ioStatus == 0)
			{
			    printf("%s: Error writing file.\n", argv[i]);
			    break;
			}
		    }
		    free(transformIn);
		}
	    }
	}
	fclose(file);
	printf("Finished processing %s\n", argv[i]);
    }
    return 0;
}
