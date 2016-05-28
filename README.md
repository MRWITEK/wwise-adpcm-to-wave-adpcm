# Description
This repository contains 2 programs.

`extract` is intended to simply extract WAVE files from any kind of
uncompressed resource files.

`reformat` is intended to change header and data of
Wwise IMA ADPCM Wave files to make them
recognizable as normal IMA ADPCM wave files.

# Usage
On Windows you can just drag-and-drop files on executable
to run it as if these files was specified on command line.

```
./extract FileName1 [FileName2...]
```

To get all wave files from data files all you need is
to specify these files on command line.
The files you will get in result are stored in the same
directories as input files.

**WARNING:**

- extracted files can be longer then they are supposed to be;
- extracted files can be smaller then they claim to be;
- extracted files can be of any type of RIFF, not only WAVE;
- extracted files might be split in multiple pieces
(I suppose this is unlikely...)

```
./reformat FileName1 [FileName2...]
```

To change format of wave files from Wwise IMA ADPCM WAVE
to normal IMA ADPCM WAVE all you need is to specify these files on command line.

**WARNING:**

- input files are overwritten;
- size of files is not changed;
- files might not be usable anyway.

# Installation
Installation isn't needed, put executables wherever you want.

# Compilation
Just compile it with C compiler.
