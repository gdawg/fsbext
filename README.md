fsbext
======

FSB files extractor (extracts from FMOD sound fx archives) 

The code here comes from <http://aluigi.altervista.org/search.php?src=fsbext> and is the 
excellent work of <aluigi@autistici.org>

At time of fork the fsbext version was listed as 0.3. 

I've forked it here as I couldn't find a compiled version for osx and figured it'd be useful to publish any changes necessary to make it work online. 

## Building

There's some basic autoconf scripts now, standard build should follow    

    ./configure
    make

optionally
    make install 

## Usage

Taken directly from the programs output:

    FSB files extractor 0.3
    by Luigi Auriemma
    e-mail: aluigi@autistici.org
    web:    aluigi.org    
    

    Usage: ./fsbext [options] <file.FSB>    

    -d DIR   output folder where extracting the files
    -l       list files without extracting them
    -s FILE  binary file containing the informations for rebuilding the FSB file
    -r       rebuild the original file, in short when you use -s:
             if you do NOT use -r will be created the binary file with the info
             if you use -r will be read the binary file (-s) and will be created a
             new FSB file (so it becomes the output and not the input)
             Example:   fsbext -s files.dat    input.fsb
                        fsbext -s files.dat -r output.fsb
             the tool will remove any header in the imported files automatically
    -p PASS  if the file is protected by password the tool will use this one
    -o OFF   offset where is located the FSB data, use -1 to scan the input file
    -e P T   only encrypt/decrypt the file without doing other operations using
             the password P and the algorithm type T (0 and 1 supported)
    -E T     raw decryption using an empty password and algorithm T (read above)
    -f FILE  dump the list of extracted/listed files in FILE
    -A       assign the ima_adpcm tag (0x11) instead of the xbox adpcm one (0x69)
             to the output files that use an adpcm codec
    -v       verbose output, debugging informations
    -R       raw output files (by default the tool adds headers and extensions)
             this option is NO longer needed if you plan to rebuild a fsb archive    

    NOTE: use EVER an empty folder where placing the extracted files because this
          tool adds a sequential number if a file with same name already exists
          since filenames in FSB archives are truncated at 30 chars.
