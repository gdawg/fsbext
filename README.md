fsbext
======

FSB files extractor (extracts from FMOD sound fx archives) 

## NOTE: Please check the [project homepage] for updates.

The code here comes from <http://aluigi.altervista.org/search.php?src=fsbext> and is the 
excellent work of Luigi Auriemma <me@aluigi.org>.

The code here is from the version listed as 0.3.5.

I've forked it here to share any changes necessary for use on osx.

[project homepage]: http://aluigi.altervista.org/search.php?src=fsbext

## Building

There's some basic autoconf scripts now, standard build should follow    

    ./configure
    make

optionally
    make install 

## Usage

As embedded in app:

```sh
Usage: ./fsbext [options] <file.FSB>

Options:
-d DIR   output folder where extracting the files
-l       list files without extracting them
-A       assign the ima_adpcm tag (0x11) instead of the xbox adpcm one (0x69)
         to the output files that use an adpcm codec
-M       split the multichannel mp3s in multiple files containing the single
         channels, load them in a multitrack software to listen the original
         multichannel file. this option is suggested for maximum quality

Rebuilding options:
-s FILE  binary file containing the information for rebuilding the FSB file,
         specify it during the extraction (will be created) and rebuilding
-r       rebuild the original file:
         Example:   fsbext -d myfolder -s files.dat    input.fsb
                    fsbext -d myfolder -s files.dat -r output.fsb
         the tool will remove any header in the imported files automatically

Debugging options:
-v       verbose output, debugging information
-p PASS  use this password if the file is protected
-o OFF   offset where is located the FSB data, use -o -1 to scan the input file
-e P T   only encrypt/decrypt the file without doing other operations using
         the password P and the algorithm type T (0 and 1 supported)
-E T     raw decryption using an empty password and algorithm T (read above)
-f FILE  dump the list of extracted/listed files in FILE
-m       disable the mpeg modifications, by default the tool automatically
         removes the non-standard padding from mp3 files and dumps only the
         first channel of multichannel files, note that this is not a downmix
         but just the selection of the first stereo channel.
         use this option to disable them, but the mp3 will be not playable
-R       raw output files (by default the tool adds headers and extensions)
         this option is NO longer needed if you plan to rebuild a fsb archive

NOTE: by default this tool dumps only the first one or two channels of
      multichannel mp3 files to make them playable on standard music players,
      use -M to avoid this behaviour!
NOTE: use EVER an empty folder where placing the extracted files because this
      tool adds a sequential number if a file with same name already exists,
      this is done because filenames in FSB archives are truncated at 30 chars.
NOTE: OGG files are dumped as-is and are not playable
NOTE: rebuilding was meant for old versions of FSB (<= 4), may work with mp3
      files (or with -M), it just put the raw files in the new archive
```