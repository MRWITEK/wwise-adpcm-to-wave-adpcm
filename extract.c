/* The MIT License (MIT)
Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* size of input/output buffer used by the program */
#define BUFFER_SIZE BUFSIZ

/* TODO: if input is directory, process all the files inside */
/* TODO: let the program take input from stdin */
int main(int argc, char **argv)
{
    if(argc < 2)
    {
	printf("Usage: %s FileName1 [FileName2...]\n"
		"Extracts anything resembling wave (RIFF) files and all data stored after that from provided resource files,\n"
		"places extracted files in the same directory where input files are stored.\n"
		/* "ignores stored length of data.\n" */
		"Version r1\n"
		, argv[0]);
	return 1;
    }
    uint32_t riffMark = *(uint32_t *)"RIFF";
    for(size_t i = 1; i < argc; ++i)
    {
	FILE *file = fopen(argv[i], "r");
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
	    printf("%s: Error using file?!\n", argv[i]);
	    fclose(file);
	    continue;
	}
	size_t ioStatus = 1;
	static uint8_t readFile[BUFFER_SIZE + 3];
	readFile[0] =
	    readFile[1] =
	    readFile[2] = 0;
	uint8_t *writeFile = readFile+3;

	/* name_%08x.wav */
	size_t outNameLength = strlen(argv[i]) + 5 + 16 + 1;
	char *outName = malloc(outNameLength);
	size_t offset = 0;
	FILE *fileWriter = NULL;

	/* read file, write it splitted by RIFF marks */
	size_t reading = 0;
	while(1)
	{
	    ioStatus = fread(readFile+3, 1, BUFFER_SIZE-reading, file);
	    reading += ioStatus;
	    if(ioStatus == 0 || reading == BUFFER_SIZE)
	    {
		if(reading == 0)
		{
		    break; /* file has ended or something */
		}

		/* includes rollback check */
		uint8_t *inspectPointer = readFile;
		uint8_t *inspectEnd = readFile + reading;
		for(; inspectPointer < inspectEnd;
			++inspectPointer)
		{
		    /* when it finds "RIFF", finish writing previous file
		     * and start writing new file */
		    if(*(uint32_t *)inspectPointer == riffMark)
		    {
			if(fileWriter)
			{
			    ioStatus = fwrite(writeFile,
				    inspectPointer - writeFile, 1, fileWriter);
			    if((ioStatus == 0) | (fclose(fileWriter) != 0))
			    {
				fprintf(stderr, "%s: Error writing a file.\n",
					outName);
			    }
			    printf("%s\n", outName);
			}
			offset += inspectPointer - writeFile;
			snprintf(outName, outNameLength,
				"%s_%08lx.wav", argv[i], offset);
			writeFile = inspectPointer,
				  fileWriter = fopen(outName, "w");
			inspectPointer += 3;
		    }
		}
		if(fileWriter)
		{
		    ioStatus = fwrite(writeFile,
			    inspectPointer - writeFile, 1, fileWriter);
		    if(ioStatus == 0)
		    {
			fprintf(stderr, "%s: Error writing a file.\n", outName);
		    }
		}
		offset += inspectPointer - writeFile;
		readFile[0] = inspectEnd[0],
		readFile[1] = inspectEnd[1],
		readFile[2] = inspectEnd[2],
		reading = 0,
		writeFile = readFile;
	    }
	}
	if(fileWriter && fclose(fileWriter))
	{
	    fprintf(stderr, "%s: Error writing a file.\n", outName);
	}
	printf("%s\n", outName);

	free(outName);
	fclose(file);
    }
    return 0;
}
