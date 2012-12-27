/*
    Copyright 2005-2012 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

//#define NOLFS
#ifndef NOLFS   // 64 bit file support not really needed since the tool uses signed 32 bits at the moment, anyway I leave it enabled
    #define _LARGE_FILES        // if it's not supported the tool will work
    #define __USE_LARGEFILE64   // without support for large files
    #define __USE_FILE_OFFSET64
    #define _LARGEFILE_SOURCE
    #define _LARGEFILE64_SOURCE
    #define _FILE_OFFSET_BITS   64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <ctype.h>
#include "show_dump.h"
#include "fsb.h"
#include "mywav.h"
#include "xma_header.h"

#ifdef WIN32
    #include <direct.h>
    #define PATHSLASH   '\\'
    #define make_dir(x) mkdir(x)

    HWND    mywnd   = NULL;
    char *get_file(char *title, int fsb, int multi);
    char *get_folder(char *title);
#else
    #include <unistd.h>
    #define strnicmp    strncasecmp
    #define PATHSLASH   '/'
    #define make_dir(x) mkdir(x, 0755)
#endif

#if defined(_LARGE_FILES)
    #if defined(__APPLE__)
        #define fseek   fseeko
        #define ftell   ftello
    #elif defined(__FreeBSD__)
    #elif !defined(NOLFS)       // use -DNOLFS if this tool can't be compiled on your OS!
        #define off_t   off64_t
        #define fopen   fopen64
        #define fseek   fseeko64
        #define ftell   ftello64
    #endif
#endif

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef int64_t     i64;



#define VER         "0.3"
#define HEXSIZE     176
#define NULLNAME    "%08x.dat"

#define MODEZ(x)    sprintf(name, "%.*s", sizeof(x.name), x.name);      \
                    freq     = x.deffreq;                               \
                    chans    = x.numchannels;                           \
                    mode     = show_mode(x.mode, &codec, NULL, &bits);  \
                    size     = x.lengthcompressedbytes;                 \
                    samples  = x.lengthsamples;                         \
                    moresize = x.size - sizeof(x);
#define FDREB_INIT  if(fdreb) reboff = ftell(fd);
#define REBBUFFCHK  if(rebsize > rebbuffsz) { \
                        rebbuffsz = rebsize + 1000; \
                        rebbuff = realloc(rebbuff, rebbuffsz); \
                        if(!rebbuff) std_err(); \
                    }
#define PATHSZ      1024    // 257 was enough, theoretically the system could support 32kb but it's false



void add_to_reb_file(FILE *fd);
int rebuild_fsb(FILE *fd);

//#define myfr(A,B,C) fread(B, 1, C, A)
//#define myfgetc fgetc
//#define myftell ftell
//#define myfseek fseek
int myfr(FILE *fd, u8 *buff, int size);
int myfgetc(FILE *fd);
i64 myftell(FILE *fd);
int myfseek(FILE *fd, i64 pos, int mode);

void frch(FILE *fd, u8 *data, int size);
void frchs(FILE *fd, u8 *data, int datasz);
void fwch(FILE *fd, u8 *data, int size);
void fwchs(FILE *fd, u8 *data);
u8 fr8(FILE *fd);
u16 (*fr16)(FILE *fd);
u16 fri16(FILE *fd);
u16 frb16(FILE *fd);
u32 (*fr32)(FILE *fd);
u32 fri32(FILE *fd);
u32 frb32(FILE *fd);

void fw08(FILE *fd, int num);
void fwi16(FILE *fd, int num);
void fwi32(FILE *fd, int num);
void fwb16(FILE *fd, int num);
void fwb32(FILE *fd, int num);

void fr_FSOUND_FSB_HEADER_FSB1(FILE *fd, FSOUND_FSB_HEADER_FSB1 *fh);
void fr_FSOUND_FSB_HEADER_FSB2(FILE *fd, FSOUND_FSB_HEADER_FSB2 *fh);
void fr_FSOUND_FSB_HEADER_FSB3(FILE *fd, FSOUND_FSB_HEADER_FSB3 *fh);
void fr_FSOUND_FSB_HEADER_FSB4(FILE *fd, FSOUND_FSB_HEADER_FSB4 *fh);
void fr_FSOUND_FSB_HEADER_FSB5(FILE *fd, FSOUND_FSB_HEADER_FSB5 *fh);

void fr_FSOUND_FSB_SAMPLE_HEADER_1(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_1 *fh);
void fr_FSOUND_FSB_SAMPLE_HEADER_2(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_2 *fh);
void fr_FSOUND_FSB_SAMPLE_HEADER_3_1(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_3_1 *fh);
void fr_FSOUND_FSB_SAMPLE_HEADER_BASIC(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_BASIC *fh, int moresize);

void pcmwav_header(FILE *fd, int freq, u16 chans, u16 bits, u32 rawlen);
void xbox_ima_header(FILE *fd, int freq, u16 chans, u32 rawlen);
void its_header(FILE *fd, u8 *fname, u16 chans, u16 bits, u32 rawlen);
void genh_header(FILE *fd, int freq, u16 chans, u32 rawlen, u8 *coeff, int coeffsz);
void brstm_header(FILE *fd, int freq, u16 chans, u32 rawlen, u8 *coeff, int coeffsz);
void adx_header(FILE *fd, int freq, u16 chans, u32 rawlen);
void ss2_header(FILE *fd, int freq, u16 chans, u32 rawlen);
void vag_header(FILE *fd, u8 *fname, int freq, u32 rawlen);

int check_sign_endian(FILE *fd);
char *show_mode(u32 mode, int *xcodec, u16 *chans, u16 *bits);
void create_dir(u8 *fname);
void experimental_extension_guessing(u8 *fname, u8 *oldext, u8 *end);
void add_extension(u8 *fname, int add, int extract, int add_guess);
u32 putfile(FILE *fd, /*int idx,*/ u8 *fname);
void extract_file(FILE *fd, u8 *fname, int freq, u16 chans, u16 bits, u32 len, u8 *moresize_dump, int moresize, int samples);
void delimit(u8 *data);
int fsb_file_crypt(FILE *fd, FILE *fdo);
int fsb_file_crypt_scan(FILE *fd);
FILE *try_fsbdec(FILE *fd, u8 *fsb_password);
u8 fsbdec(u8 t);
int fsbdec0(u8 *data, int len, u8 *key, int keyc);
int fsbdec1(u8 *data, int len, u8 *key, int keyc);
u32 char_crc(u8 *data);
u8 *mystrrchrs(u8 *str, u8 *chrs);

int get_num(u8 *data);
void read_err(void);
void write_err(void);
void std_err(void);
void myexit(int ret);



FILE    *fdreb      = NULL;
i64     fsb_offset  = 0;
u32     reboff      = 0,
        rebsize     = 0;
int     addhead     = 1,    // from version 0.2.8a it has been enabled by default, yeah I needed to do it many years ago
        codec       = 0,
        head_ver    = 0,
        verbose     = 0,
        nullfiles   = 0,
        force_ima   = 0,
        rebbuffsz   = 0,
        aligned     = 0,
        pcm_endian  = 0,
        fsb_enctype = -1,
        fsb_keyc    = 0,
        fsb_keysz   = 0,
        fsb_scan    = 0,
        force_ima_x = 0;
u8      *rebbuff    = NULL,
        fsb_key[256] = "";



int main(int argc, char *argv[]) {
    static u8   name[256 + 32 + 1024]  = "";    // 1024 is used for FSB5

    FSOUND_FSB_HEADER_FSB1          fh1;
    FSOUND_FSB_HEADER_FSB2          fh2;
    FSOUND_FSB_HEADER_FSB3          fh3;
    FSOUND_FSB_HEADER_FSB4          fh4;
    FSOUND_FSB_HEADER_FSB5          fh5;
    FSOUND_FSB_SAMPLE_HEADER_1      fs1;
    FSOUND_FSB_SAMPLE_HEADER_2      fs2;
    FSOUND_FSB_SAMPLE_HEADER_3_1    fs31;
    FSOUND_FSB_SAMPLE_HEADER_BASIC  fsb;

    FILE    *fd             = NULL,
            *fdo            = NULL,
            *fdlist         = NULL;
    i64     t64;
    u32     nameoff         = 0,
            fileoff         = 0,
            baseoff         = 0,
            size            = 0,
            samples         = 0,
            datasize        = 0,
            current_offset  = 0,
            offset,
            type,
            t32;
    int     i,
            num,
            len,
            sign,
            freq            = 44100,
            head_mode       = 0,
            list            = 0,
            rebuild         = 0,
            moresize_dumpsz = 0,
            do_raw_crypt    = 0;
    u16     chans           = 1,
            bits            = 16,
            moresize        = 0;
    u8      *fname          = NULL,
            *rebfile        = NULL,
            *listfile       = NULL,
            *folder         = NULL,
            *mode           = NULL,
            *moresize_dump  = NULL,
            *fsb_password   = NULL,
            *enc_password   = NULL,
            *p;

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    fputs("\n"
        "FSB files extractor " VER "\n"
        "by Luigi Auriemma\n"
        "e-mail: aluigi@autistici.org\n"
        "web:    aluigi.org\n"
        "\n", stderr);

#ifdef WIN32
    mywnd = GetForegroundWindow();
    if(GetWindowLong(mywnd, GWL_WNDPROC)) {
        num = 0;
        for(i = 1; i < argc; i++) {
            if(((argv[i][0] != '-') && (argv[i][0] != '/')) || (strlen(argv[i]) != 2)) {
                break;
            }
            switch(argv[i][1]) {
                case 'd': i++; num = 1; break;  // output folder already selected
                case 's': i++;          break;
                case 'f': i++;          break;
                default: break;
            }
        }
        if(i > argc) i = argc;
        int rem_args = num ? 1 : 3;
        i = rem_args - (argc - i);
        if(i > 0) {
            printf(
                "- GUI mode activated, remember that the tool works also from command-line\n"
                "  where are available various options\n"
                "\n");
            fname = calloc(argc + i + 3, sizeof(char *));   // was + 1 but this code is a bit chaotic so stay safe
            if(!fname) std_err();
            memcpy(fname, argv, sizeof(char *) * argc);
            argv = (void *)fname;
            argc -= (rem_args - i);
            char    **lame = NULL;  // long story
            if(num) {
                if(i >= 1) argv[argc++] = get_file("select the input FSB archive to extract", 1, 0);
            } else {
                if(i < 3) { // lame backup
                    if(i >= 1) argv[argc + 1] = argv[argc];
                    if(i >= 2) argv[argc + 2] = argv[argc];
                }
                if(i >= 1) argv[argc++] = "-d";
                if(i >= 2) lame = &argv[argc++];
                if(i >= 3) argv[argc] = get_file("select the input FSB archive to extract", 1, 0);
                argc++;
                *lame = get_folder("select the output folder where extracting the files");
            }
        }
    }
#endif

    if(argc < 2) {
        printf("\n"
            "Usage: %s [options] <file.FSB>\n"
            "\n"
            "-d DIR   output folder where extracting the files\n"
            "-l       list files without extracting them\n"
            "-s FILE  binary file containing the informations for rebuilding the FSB file\n"
            "-r       rebuild the original file, in short when you use -s:\n"
            "         if you do NOT use -r will be created the binary file with the info\n"
            "         if you use -r will be read the binary file (-s) and will be created a\n"
            "         new FSB file (so it becomes the output and not the input)\n"
            "         Example:   fsbext -s files.dat    input.fsb\n"
            "                    fsbext -s files.dat -r output.fsb\n"
            "         the tool will remove any header in the imported files automatically\n"
            "-p PASS  if the file is protected by password the tool will use this one\n"
            "-o OFF   offset where is located the FSB data, use -1 to scan the input file\n"
            "-e P T   only encrypt/decrypt the file without doing other operations using\n"
            "         the password P and the algorithm type T (0 and 1 supported)\n"
            "-E T     raw decryption using an empty password and algorithm T (read above)\n"
            "-f FILE  dump the list of extracted/listed files in FILE\n"
            "-A       assign the ima_adpcm tag (0x11) instead of the xbox adpcm one (0x69)\n"
            "         to the output files that use an adpcm codec\n"
            "-v       verbose output, debugging informations\n"
            "%s\n"
            "\n"
            "NOTE: use EVER an empty folder where placing the extracted files because this\n"
            "      tool adds a sequential number if a file with same name already exists\n"
            "      since filenames in FSB archives are truncated at 30 chars.\n"
            "\n", argv[0],
            addhead ?
                "-R       raw output files (by default the tool adds headers and extensions)\n"
                "         this option is NO longer needed if you plan to rebuild a fsb archive":
                "-a       add header to the output files for making them playable (now default)\n"
            );
        myexit(1);
    }

    argc--;
    for(i = 1; i < argc; i++) {
        if(((argv[i][0] != '-') && (argv[i][0] != '/')) || (strlen(argv[i]) != 2)) {
            fprintf(stderr, "\nError: wrong command-line argument (%s)\n\n", argv[i]);
            myexit(1);
        }
        switch(argv[i][1]) {
            case 'd': folder        = argv[++i];    break;
            case 'l': list          = 1;            break;
            case 's': rebfile       = argv[++i];    break;
            case 'r': rebuild       = 1;            break;
            case 'f': listfile      = argv[++i];    break;
            case 'v': verbose       = 1;            break;
            case 'A': force_ima     = 1;            break;
            case 'a': addhead       = 1;            break;
            case 'R': addhead       = 0;            break;
            case 'p': fsb_password  = argv[++i];    break;
            case 'e': {
                enc_password = argv[++i];
                fsb_enctype  = atoi(argv[++i]);
                break;
            }
            case 'E': {
                enc_password = "";
                fsb_enctype  = atoi(argv[++i]);
                do_raw_crypt = 1;
                break;
            }
            case 'o': {
                fsb_offset    = get_num(argv[++i]);
                if(fsb_offset < 0) {
                    fsb_offset = 0;
                    fsb_scan = 1;
                }
                break;
            }
            default: {
                fprintf(stderr, "\nError: wrong command-line argument (%s)\n\n", argv[i]);
                myexit(1);
                break;
            }
        }
    }

    fname = argv[argc];

    if(rebfile) {
        if(listfile) {
            fprintf(stderr, "\n"
                "Error: you can't use -l with -s because from version 0.2.13 the names of the\n"
                "       dumped files are stored directly in the binary output file for avoiding\n"
                "       problems with truncated filenames\n");
            myexit(1);
        }
        /*
        if(addhead) {
            //printf("\n"
                //"Error: you can't use the -a and -s options together or you can't rebuild\n"
                //"       the FSB file correctly because the files MUST be all headerless\n"
                //"\n");
            //myexit(1);
            addhead = 0;
            printf("- \"add header\" option disabled otherwise you can't rebuild the archive\n");
        }
        */
        printf("- reb file:     %s\n", rebfile);
        fdreb = fopen(rebfile, "rb");
        if(!rebuild) {
            if(fdreb) {
                fclose(fdreb);
                printf("\n"
                    "Error: you have selected -s but the output binary file already exists,\n"
                    "       check if you want to recreate this file and delete it\n");
                myexit(1);
            }
            fdreb = fopen(rebfile, "wb");
        }
        if(!fdreb) std_err();
    }

    if(listfile) {
        fdlist = fopen(listfile, "wb");
        if(!fdlist) std_err();
    }

    if(!rebuild) {
        printf("- input file:   %s\n", fname);
        fd = fopen(fname, "rb");
        if(!fd) std_err();
    } else {
        if(!fdreb) {
            printf("\n"
                "Error: you have selected the rebuild option but you have not specified the\n"
                "       file with the rebuild informations using -s\n");
            myexit(1);
        }

        printf("- output file:  %s\n", fname);
        fd = fopen(fname, "rb");
        if(fd) {
            fclose(fd);
            printf("\n"
                "Error: you have selected the rebuild option but the output FSB file already\n"
                "       exists, check if you want to recreate this file and delete it\n");
            myexit(1);
        }
        fd = fopen(fname, "wb");
        if(!fd) std_err();
    }

    if(folder) {
        printf("- enter folder: %s\n", folder);
        if(chdir(folder) < 0) std_err();
    }

    if(enc_password) {
        p = mystrrchrs(fname, "\\/");
        if(p) {
            p++;
        } else {
            p = fname;
        }
        rebfile = malloc(strlen(p) + 64);
        strcpy(rebfile, p);
        p = strrchr(rebfile, '.');
        if(p) *p = 0;
        strcpy(p, "_crypt.fsb");
        for(i = 0;; i++) {
            fdo = fopen(rebfile, "rb");
            if(!fdo) break;
            fclose(fdo);
            sprintf(p, "_%d_crypt.fsb", i);
        }
        printf("- output file:  %s\n", rebfile);
        fdo = fopen(rebfile, "wb");
        if(!fdo) std_err();
        fsb_keysz = sprintf(fsb_key, "%.*s", sizeof(fsb_key) - 1, enc_password);

        printf("- use encryption type %d\n", fsb_enctype);
        if(!do_raw_crypt) {
            i = fsb_enctype;
            fsb_enctype = -1;
            sign = check_sign_endian(fd);
            fsb_enctype = i;
            if(sign < 0) {
                // decrypt
                fsb_enctype = fsb_file_crypt_scan(fd);
            } else {
                // encrypt and switch types (because 0 and 1 are just the same)
                // I have made this "switching" so that people will use
                // what they saw during the extraction/decryption
                printf("- switch encryption from %d to ", fsb_enctype);
                switch(fsb_enctype) {
                    case 0: fsb_enctype = 1;    break;
                    case 1: fsb_enctype = 0;    break;
                    default: break;
                }
                printf("%d (decrypt<->encrypt)\n", fsb_enctype);
            }
        }

        fsb_file_crypt(fd, fdo);
        fclose(fd);
        fclose(fdo);
        myexit(0);
    }

    if(rebuild) {
        i = rebuild_fsb(fd);
        goto quit;
    }

redo:
    if(fsb_scan) {
        printf("\n"
            "- FSB scanner, wait patiently\n"
            "  you will receive an \"unexpected data\" error when the end of file is reached\n");
        while(check_sign_endian(fd) < 0) {
            fsb_offset++;
        }
        printf("- FSB scanner offset: %08x\n", (u32)fsb_offset);
    }

    sign = check_sign_endian(fd);
    if(sign < 0) {
        fd = try_fsbdec(fd, fsb_password);
    }

    sign = check_sign_endian(fd);
    if(sign < 0) {
        printf("\nError: this tool doesn't support the format \"%.4s\"\n\n", (u8 *)&sign);
        myexit(1);
    }

    num = 0;
    if(sign == '1') {
        fr_FSOUND_FSB_HEADER_FSB1(fd, &fh1);
        num       = fh1.numsamples;
        nameoff   = sizeof(fh1);
        fileoff   = sizeof(fh1) + (num * sizeof(fs1));
        datasize  = fh1.datasize;
        head_ver  = 1;
        head_mode = 0;

    } else if(sign == '2') {
        fr_FSOUND_FSB_HEADER_FSB2(fd, &fh2);
        num       = fh2.numsamples;
        nameoff   = sizeof(fh2);
        fileoff   = sizeof(fh2) + fh2.shdrsize;
        datasize  = fh2.datasize;
        head_ver  = 2;
        head_mode = 0;

    } else if(sign == '3') {
        fr_FSOUND_FSB_HEADER_FSB3(fd, &fh3);
        num       = fh3.numsamples;
        nameoff   = sizeof(fh3);
        fileoff   = sizeof(fh3) + fh3.shdrsize;
        datasize  = fh3.datasize;
        head_ver  = 3;
        head_mode = fh3.mode;
        printf("- FSB3 version %hu.%hu mode %u\n",
            (fh3.version >> 16) & 0xffff, fh3.version & 0xffff,
            head_mode);

    } else if(sign == '4') {
        fr_FSOUND_FSB_HEADER_FSB4(fd, &fh4);
        num       = fh4.numsamples;
        nameoff   = sizeof(fh4);
        fileoff   = sizeof(fh4) + fh4.shdrsize;
        datasize  = fh4.datasize;
        head_ver  = 4;
        head_mode = fh4.mode;
        printf("- FSB4 version %hu.%hu mode %u\n",
            (fh4.version >> 16) & 0xffff, fh4.version & 0xffff,
            head_mode);

    } else if(sign == '5') {
        fr_FSOUND_FSB_HEADER_FSB5(fd, &fh5);
        num       = fh5.numsamples;
        nameoff   = sizeof(fh5) + fh5.shdrsize;
        fileoff   = sizeof(fh5) + fh5.shdrsize + fh5.namesize;
        datasize  = fh5.datasize;
        head_ver  = 5;
        head_mode = 0;
        if(fh5.zero[4] & 1) head_mode |= 0x08;  // big endian
        printf("- FSB5 mode %u\n", head_mode);

    } else {
        printf("\nError: this tool doesn't support FSB%c\n\n", sign & 0xff);
        myexit(1);
    }

    if(head_mode & FSOUND_FSB_SOURCE_BASICHEADERS) {
        printf("- small sample headers\n");
    }
    if(head_mode & 0x08) {
        printf("- big endian samples\n");
        pcm_endian = 1;
    }
    if(head_mode & 0x40) {
        printf("- aligned files\n");
        aligned = 1;
    }

    baseoff = fileoff;
    if(verbose) {
        printf(
            "- base  offset  %08x\n"
            "- names offset  %08x\n"
            "- files offset  %08x\n",
            baseoff, nameoff, fileoff);
    }

    printf("\n"
        "Filename                         Size       Mode frequency channels bits\n"
        "========================================================================\n");

    for(i = 0; i < num; i++) {
        memset(name, 0, sizeof(name));  // I need to use it because filenames are truncated!

        if(head_ver == 1) {
            fr_FSOUND_FSB_SAMPLE_HEADER_1(fd, &fs1);
            sprintf(name, "%.*s", sizeof(fs1.name), fs1.name);
            samples  = fs1.lengthsamples;
            freq     = fs1.deffreq;
            mode     = show_mode(fs1.mode, &codec, &chans, &bits);  // chans only here?
            size     = fs1.lengthcompressedbytes;

        } else if(head_ver == 2) {
            fr_FSOUND_FSB_SAMPLE_HEADER_2(fd, &fs2);
            MODEZ(fs2)

        } else if(head_ver == 3) {
            if((head_mode & FSOUND_FSB_SOURCE_BASICHEADERS) && i) {
                fr_FSOUND_FSB_SAMPLE_HEADER_BASIC(fd, &fsb, moresize);
                sprintf(name, NULLNAME, i);
                size     = fsb.lengthcompressedbytes;
                samples  = fsb.lengthsamples;
                //freq, chans, mode and moresize are the same of the first file
            } else {
                if(fh3.version == FSOUND_FSB_VERSION_3_1) {         // 3.1
                    fr_FSOUND_FSB_SAMPLE_HEADER_3_1(fd, &fs31);
                    MODEZ(fs31)
                } else {                                            // 3.0
                    fr_FSOUND_FSB_SAMPLE_HEADER_2(fd, &fs2);
                    MODEZ(fs2)
                }
            }

        } else if(head_ver == 4) {
            if((head_mode & FSOUND_FSB_SOURCE_BASICHEADERS) && i) {
                fr_FSOUND_FSB_SAMPLE_HEADER_BASIC(fd, &fsb, moresize);
                sprintf(name, NULLNAME, i);
                size     = fsb.lengthcompressedbytes;
                samples  = fsb.lengthsamples;
                //freq, chans, mode and moresize are the same of the first file
            } else {
                fr_FSOUND_FSB_SAMPLE_HEADER_3_1(fd, &fs31);
                MODEZ(fs31);
            }

        } else if(head_ver == 5) {
            moresize = 0;
            freq    = 44100;
            chans   = 1;
            bits    = 16;
            codec   = fh5.mode;
            switch(fh5.mode) {
                case FMOD_SOUND_FORMAT_PCM8:        bits = 8;   break;
                case FMOD_SOUND_FORMAT_PCM16:       bits = 16;  break;
                case FMOD_SOUND_FORMAT_PCM24:       bits = 24;  break;
                case FMOD_SOUND_FORMAT_PCM32:       bits = 32;  break;
                case FMOD_SOUND_FORMAT_PCMFLOAT:    bits = 32;  break;
                default: break;
            }
            switch(fh5.mode) {
                case FMOD_SOUND_FORMAT_PCM8:        mode = "pcm8";  break;
                case FMOD_SOUND_FORMAT_PCM16:       mode = "pcm16"; break;
                case FMOD_SOUND_FORMAT_PCM24:       mode = "pcm24"; break;
                case FMOD_SOUND_FORMAT_PCM32:       mode = "pcm32"; break;
                case FMOD_SOUND_FORMAT_PCMFLOAT:    mode = "float"; break;
                case FMOD_SOUND_FORMAT_GCADPCM:     mode = "gc";    break;
                case FMOD_SOUND_FORMAT_IMAADPCM:    mode = "ima";   break;
                case FMOD_SOUND_FORMAT_VAG:         mode = "vag";   break;
                case FMOD_SOUND_FORMAT_HEVAG:       mode = "hevag"; break;
                case FMOD_SOUND_FORMAT_XMA:         mode = "xma";   break;
                case FMOD_SOUND_FORMAT_MPEG:        mode = "mpeg";  break;
                case FMOD_SOUND_FORMAT_CELT:        mode = "celt";  break;
                case FMOD_SOUND_FORMAT_AT9:         mode = "at9";   break;
                case FMOD_SOUND_FORMAT_XWMA:        mode = "xwma";  break;
                case FMOD_SOUND_FORMAT_VORBIS:      mode = "ogg";   break;
                default:                            mode = "";      break;
            }

            offset   = fr32(fd);
            samples  = fr32(fd) >> 2;   // ???

            type     = offset & 0xff;
            offset >>= 8;
            offset  *= 0x40;

            if(type & 0x20) chans = 2;

            while(type & 1) {
                t32  = fr32(fd);
                type = t32 & 1;
                len  = (t32 & 0xffffff) >> 1;
                t32 >>= 24;
                t64 = myftell(fd);
                switch(t32) {
                    case 0x2: chans = fr8(fd);      break;
                    case 0x4: freq  = fr32(fd);     break;
                    case 0x6: fr32(fd); fr32(fd);   break;
                    case 0xc:   // xma seek
                    case 0xe:   // dsp coeff
                    case 0x14:  // xwma data
                    {
                        moresize = len;
                        if(moresize > moresize_dumpsz) {
                            moresize_dumpsz = moresize + 100;
                            moresize_dump = realloc(moresize_dump, moresize_dumpsz);
                            if(!moresize_dump) std_err();
                        }
                        frch(fd, moresize_dump, moresize);
                        break;
                    }
                    default: break;
                }
                t64 += len;
                myfseek(fd, t64, SEEK_SET);
            }

            t64 = myftell(fd);
            if(myftell(fd) < nameoff) {
                size = fr32(fd);
                if(!size) {
                    myfseek(fd, 0, SEEK_END);
                    size = myftell(fd);
                } else {
                    size >>= 8;
                    size *= 0x40;
                    size += baseoff;
                }
            } else {
                myfseek(fd, 0, SEEK_END);
                size = myftell(fd);
            }
            myfseek(fd, t64, SEEK_SET);
            fileoff = baseoff + offset;
            size -= fileoff;

            if(fh5.namesize) {
                t64 = myftell(fd);
                myfseek(fd, nameoff + (i * 4), SEEK_SET);
                myfseek(fd, nameoff + fr32(fd), SEEK_SET);
                frchs(fd, name, sizeof(name));
                myfseek(fd, t64, SEEK_SET);
            } else {
                sprintf(name, NULLNAME, i);
            }

        } else {
            printf("\nError: you must update this tool adding support for head version %d\n", head_ver);
            myexit(1);
        }

        if(verbose) printf("  fh->file_offset %08x\n", fileoff);
        if(moresize && (head_ver < 5)) {
            if(fdreb) { // the rebuild file already contains moresize, so we need to "reload" it now
                myfseek(fd, -moresize, SEEK_CUR);
            }
            if(moresize > moresize_dumpsz) {
                moresize_dumpsz = moresize + 100;
                moresize_dump = realloc(moresize_dump, moresize_dumpsz);
                if(!moresize_dump) std_err();
            }
            frch(fd, moresize_dump, moresize);
            //if(fdreb) fwch(fdreb, moresize_dump, moresize);   // moresize is already saved
            if(verbose) {
                printf("  fh->moresize    %d:\n", moresize);
                show_dump(moresize_dump, moresize, stdout);
                printf("\n");
            }
        }

        if(!name[0]) {
            //sprintf(name, NULLNAME, i);
            nullfiles++;
            printf("- NULL file skipped\n");
            continue;
        }

        printf(
            "%-32s %-10u %s %d %hu %hu\n",
            name, size, mode, freq, chans, bits);
        if(fdlist) {
            fprintf(fdlist, "%s\r\n", name);
        }

        current_offset = myftell(fd);
        if(!list) {
            if(myfseek(fd, fileoff, SEEK_SET) < 0) std_err();
            extract_file(fd, name, freq, chans, bits, size, moresize_dump, moresize, samples);
            fileoff += size;
            if(aligned) {
                if(fileoff & 31) fileoff = (fileoff + 31) & (~31);
            }
        }
        myfseek(fd, current_offset, SEEK_SET);
    }

    if(fsb_scan) {
        if(fileoff < datasize) fileoff = datasize;
        fsb_offset += fileoff;
        goto redo;
    }
quit:
    if(fd)     fclose(fd);
    if(fdreb)  fclose(fdreb);
    if(fdlist) fclose(fdlist);
    printf("\n- %d files %s\n\n", i - nullfiles, list ? "listed" : "processed");
    if(force_ima_x && addhead && !force_ima) {
        printf("\n"
            "- some of the extracted files use IMA ADPCM and this tool has assigned them\n"
            "  the codec 0x69 (Xbox ADPCM) but if you want to play them immediately with\n"
            "  VLC or mplayer you must add the -A option to fsbext so that they will be\n"
            "  created with the codec 0x01 (IMA ADPCM).\n");
    }
    myexit(0);
    return(0);
}



void add_to_reb_file(FILE *fd) {
    if(!fdreb) return;
    if(!rebsize) rebsize = ftell(fd) - reboff;
    REBBUFFCHK
    fseek(fd, reboff, SEEK_SET);
    frch(fd, rebbuff, rebsize);
    fwi32(fdreb, rebsize);              // size of the block
    fwch(fdreb, rebbuff, rebsize);      // content of the block
    // name is saved after extract_file
}



int rebuild_fsb(FILE *fd) {
    FSOUND_FSB_HEADER_FSB1          *fh1;
    FSOUND_FSB_HEADER_FSB2          *fh2;
    FSOUND_FSB_HEADER_FSB3          *fh3;
    FSOUND_FSB_HEADER_FSB4          *fh4;
    FSOUND_FSB_HEADER_FSB5          *fh5;
    FSOUND_FSB_SAMPLE_HEADER_1      *fs1;
    FSOUND_FSB_SAMPLE_HEADER_2      *fs2;
    FSOUND_FSB_SAMPLE_HEADER_3_1    *fs31;
    FSOUND_FSB_SAMPLE_HEADER_BASIC  *fsb;

    double  val1,   // double modifies lengthsamples of ima.fsb and float the one of ps2_iop.fsb, so I fix it
            val2,
            val3;
    u32     i,
            j,
            files,
            head_off,
            head_size,
            real_head_size          = 0,
            data_off,
            data_size,
            lengthsamples           = 0,
            lengthcompressedbytes   = 0,
            *already_read;
    int     //samename,
            padding,
            head_mode   = 0;
    u8      name[256 + 32]   = "",
            ver;

    fr16 = fri16;
    fr32 = fri32;

    printf(
        "- FSB rebuilding\n"
        "  note that this option is experimental and doesn't work for all the\n"
        "  versions of FSB files!!!\n");

    rebsize = fr32(fdreb);  // we consider the input file in little endian, I'm lazy and it's the format used in the 99,9% of the files
    REBBUFFCHK
    frch(fdreb, rebbuff, rebsize);
    //frchs(fdreb, name, sizeof(name)); // not here
    reboff = ftell(fdreb);
    fwch(fd, rebbuff, rebsize);

    fh1  = (FSOUND_FSB_HEADER_FSB1 *)rebbuff;
    fh2  = (FSOUND_FSB_HEADER_FSB2 *)rebbuff;
    fh3  = (FSOUND_FSB_HEADER_FSB3 *)rebbuff;
    fh4  = (FSOUND_FSB_HEADER_FSB4 *)rebbuff;
    fh5  = (FSOUND_FSB_HEADER_FSB5 *)rebbuff;
    fs1  = (FSOUND_FSB_SAMPLE_HEADER_1 *)rebbuff;
    fs2  = (FSOUND_FSB_SAMPLE_HEADER_2 *)rebbuff;
    fs31 = (FSOUND_FSB_SAMPLE_HEADER_3_1 *)rebbuff;
    fsb  = (FSOUND_FSB_SAMPLE_HEADER_BASIC *)rebbuff;

    files  = *(u32 *)(rebbuff + 4);
    ver    = rebbuff[3];

    if(ver >= '5') {
        printf("\nError: use the FMOD programs to rebuild the files of FSB versions major than 4\n");
        myexit(0);
    }

#define X0(x,y,z)   if(ver == x) {                                              \
                        printf("- FSB version %c\n", x);                        \
                        real_head_size = y;                                     \
                        head_mode      = z;                                     \
                    }
#define X1(x,y)     if(ver == x) {                                              \
                        if((head_mode & FSOUND_FSB_SOURCE_BASICHEADERS) && i) { \
                            sprintf(name, NULLNAME, i);                         \
                            lengthsamples         = fsb->lengthsamples;         \
                            lengthcompressedbytes = fsb->lengthcompressedbytes; \
                        } else {                                                \
                            memcpy(name, y->name, sizeof(y->name));             \
                            lengthsamples         = y->lengthsamples;           \
                            lengthcompressedbytes = y->lengthcompressedbytes;   \
                        }                                                       \
                    }
#define X2(x,y)     if(ver == x) {                                              \
                        if((head_mode & FSOUND_FSB_SOURCE_BASICHEADERS) && i) { \
                            fsb->lengthsamples         = lengthsamples;         \
                            fsb->lengthcompressedbytes = lengthcompressedbytes; \
                        } else {                                                \
                            y->lengthsamples           = lengthsamples;         \
                            y->lengthcompressedbytes   = lengthcompressedbytes; \
                        }                                                       \
                    }
#define X3(x,y)     if(ver == x) {                                              \
                        y->datasize = data_size;                                \
                    }

         X0('1', 0,             0)
    else X0('2', fh2->shdrsize, 0)
    else X0('3', fh3->shdrsize, fh3->mode)
    else X0('4', fh4->shdrsize, fh4->mode)
    else X0('5', fh5->shdrsize, 0)
    else {
        printf("\nError: this version of FSB (%c) is not supported yet\n", ver);
        myexit(1);
    }

    head_off  = ftell(fd);
    head_size = 0;
    data_size = 0;

    for(i = 0; i < files; i++) {
        rebsize = fr32(fdreb);
        REBBUFFCHK
        frch(fdreb, rebbuff, rebsize);
        frchs(fdreb, name, sizeof(name));
        head_size += rebsize;
    }
    if(head_size < real_head_size) head_size = real_head_size;

    data_off = head_off + head_size;

    fseek(fdreb, reboff, SEEK_SET);

    already_read = malloc(files * sizeof(u32));
    if(!already_read) std_err();

    for(i = 0; i < files; i++) {
        memset(name, 0, sizeof(name));

        rebsize = fr32(fdreb);
        REBBUFFCHK
        frch(fdreb, rebbuff, rebsize);
        // name after X

             X1('1', fs1)
        else X1('2', fs2)
        else X1('3', fs31)
        else X1('4', fs31) // correct
        else X1('5', fs31) // correct

        // avoid lots of boring problems with the truncated filenames
        frchs(fdreb, name, sizeof(name));

        val1 = lengthsamples;
        val2 = lengthcompressedbytes;
        val3 = (val2) ? (val1 / val2) : 0;

        if(!name[0]) {  // in this case if the file has no filename I handle it normally, putfile returns 0... this is the good way
            //sprintf(name, NULLNAME, i);
            nullfiles++;    // null files are something like loops inside the real files, probably used for dynamic music
            printf("- NULL file skipped\n");
            //continue;
        } else {
            printf("  %s\n", name);

            /* no longer needed because I save the filename in fdreb
            already_read[i] = char_crc(name);
            samename = 0;
            for(j = 0; j < i; j++) {
                if(already_read[i] == already_read[j]) samename++;
            }
            add_extension(name, samename, 0, 1);
            */
        }
        fseek(fd, data_off, SEEK_SET);
        lengthcompressedbytes = putfile(fd, /*i,*/ name);

        padding = (data_off + lengthcompressedbytes) & 31;
        if(padding) {
            padding = 32 - padding;
            for(j = 0; j < padding; j++) fwch(fd, "\x00", 1);
        }

        val2          = lengthcompressedbytes;
        val1          = val2 * val3;
        lengthsamples = val1;
        if(lengthsamples & 1) lengthsamples++;

        /*
        some recent FSB archives require a 32 bytes padding while others don't need it
        so for making the story easier I directly pad any file.
        the result is that any file of any FSB version will have this padding.
        not a great solution but should avoid problems in future and I don't remember
        why I used a 16 bytes padding in my previous versions so I prefer to not remove
        it.
        for other info check the "aligned" variable.
        so when you extract the files from your newly generated fsb file don't worry if
        they are bigger than the original ones because there are only zeroes at their
        end and should not give problems.
        */
        lengthcompressedbytes += padding;
        data_off  += lengthcompressedbytes;
        data_size += lengthcompressedbytes;

        if(name[0]) {   // this is the correct way, NULL files will be left as they are
                 X2('1', fs1)
            else X2('2', fs2)
            else X2('3', fs31)
            else X2('4', fs31) // correct
            else X2('5', fs31) // correct
        }

        fseek(fd, head_off, SEEK_SET);
        fwch(fd, rebbuff, rebsize);
        head_off += rebsize;
    }

    fseek(fd, 0, SEEK_SET);
    rewind(fdreb);
    rebsize = fr32(fdreb);
    REBBUFFCHK
    frch(fdreb, rebbuff, rebsize);
    frchs(fdreb, name, sizeof(name));

         X3('1', fh1)
    else X3('2', fh2)
    else X3('3', fh3)
    else X3('4', fh4)
    else X3('5', fh5)

    fwch(fd, rebbuff, rebsize);
    return(files);

#undef X0
#undef X1
#undef X2
#undef X3
}



int myfr(FILE *fd, u8 *buff, int size) {
    int     len;

    if(buff) {
        len = fread(buff, 1, size, fd);
    } else {
        for(len = 0; len < size; len++) {
            if(fgetc(fd) < 0) break;
        }
    }
    if(len > 0) {
        switch(fsb_enctype) {
            case 0: fsb_keyc = fsbdec0(buff, len, fsb_key, fsb_keyc);   break;
            case 1: fsb_keyc = fsbdec1(buff, len, fsb_key, fsb_keyc);   break;
            default: break;
        }
    }
    return(len);
}



int myfgetc(FILE *fd) {
    u8      tmp[1];

    if(myfr(fd, tmp, 1) != 1) return(-1);
    return(tmp[0]);
}



i64 myftell(FILE *fd) {
    return(ftell(fd) - fsb_offset);
}



// i64 because must be positive and negative!
int myfseek(FILE *fd, i64 pos, int type) {
    int     ret;

    if(type == SEEK_SET) pos += fsb_offset;
    ret = fseek(fd, pos, type);
    pos = myftell(fd);
    if(fsb_enctype >= 0) {
        if(fsb_keysz <= 0) {
            fsb_keyc = 0;
        } else {
            fsb_keyc = pos % fsb_keysz;
        }
    }
    return(ret);
}



void frch(FILE *fd, u8 *data, int size) {
    if(myfr(fd, data, size) != size) read_err();
}



void frchs(FILE *fd, u8 *data, int datasz) {
    int     i;
    u8      c;

    datasz--;
    for(i = 0;; i++) {
        frch(fd, &c, 1);
        if(!c) break;
        if(i < datasz) data[i] = c;
    }
    if(i < datasz) data[i] = 0;
    else           data[datasz] = 0;
}



void fwch(FILE *fd, u8 *data, int size) {
    if(fwrite(data, 1, size, fd) != size) write_err();
}



void fwchs(FILE *fd, u8 *data) {
    int     i;

    for(i = 0;; i++) {
        fwch(fd, data + i, 1);
        if(!data[i]) break;
    }
}



u8 fr8(FILE *fd) {
    int     t;

    t = myfgetc(fd);
    if(t < 0) read_err();
    return(t);
}



u16 fri16(FILE *fd) {
    int     t1,
            t2;

    t1 = myfgetc(fd);
    t2 = myfgetc(fd);
    if((t1 < 0) || (t2 < 0)) read_err();
    return(t1 | (t2 << 8));
}



u16 frb16(FILE *fd) {
    int     t1,
            t2;

    t1 = myfgetc(fd);
    t2 = myfgetc(fd);
    if((t1 < 0) || (t2 < 0)) read_err();
    return(t2 | (t1 << 8));
}



u32 fri32(FILE *fd) {
    int     t1,
            t2,
            t3,
            t4;

    t1 = myfgetc(fd);
    t2 = myfgetc(fd);
    t3 = myfgetc(fd);
    t4 = myfgetc(fd);
    if((t1 < 0) || (t2 < 0) || (t3 < 0) || (t4 < 0)) read_err();
    return(t1 | (t2 << 8) | (t3 << 16) | (t4 << 24));
}



u32 frb32(FILE *fd) {
    int     t1,
            t2,
            t3,
            t4;

    t1 = myfgetc(fd);
    t2 = myfgetc(fd);
    t3 = myfgetc(fd);
    t4 = myfgetc(fd);
    if((t1 < 0) || (t2 < 0) || (t3 < 0) || (t4 < 0)) read_err();
    return(t4 | (t3 << 8) | (t2 << 16) | (t1 << 24));
}



void fw08(FILE *fd, int num) {
    if(fputc((num      ) & 0xff, fd) < 0) write_err();
}



void fwi16(FILE *fd, int num) {
    if(fputc((num      ) & 0xff, fd) < 0) write_err();
    if(fputc((num >>  8) & 0xff, fd) < 0) write_err();
}



void fwi32(FILE *fd, int num) {
    if(fputc((num      ) & 0xff, fd) < 0) write_err();
    if(fputc((num >>  8) & 0xff, fd) < 0) write_err();
    if(fputc((num >> 16) & 0xff, fd) < 0) write_err();
    if(fputc((num >> 24) & 0xff, fd) < 0) write_err();
}



void fwb16(FILE *fd, int num) {
    if(fputc((num >>  8) & 0xff, fd) < 0) write_err();
    if(fputc((num      ) & 0xff, fd) < 0) write_err();
}



void fwb32(FILE *fd, int num) {
    if(fputc((num >> 24) & 0xff, fd) < 0) write_err();
    if(fputc((num >> 16) & 0xff, fd) < 0) write_err();
    if(fputc((num >>  8) & 0xff, fd) < 0) write_err();
    if(fputc((num      ) & 0xff, fd) < 0) write_err();
}



int check_sign_endian(FILE *fd) {
    u8      sign[4];

    myfseek(fd, 0, SEEK_SET);
    frch(fd, sign, sizeof(sign));
    myfseek(fd, 0, SEEK_SET);

    if(
         (sign[0] == 'F')
      && (sign[1] == 'S')
      && (sign[2] == 'B')) {
        fr16 = fri16;
        fr32 = fri32;
        return(sign[3]);

    } else if(
         (sign[3] == 'F')
      && (sign[2] == 'S')
      && (sign[1] == 'B')) {
        fr16 = frb16;
        fr32 = frb32;
        return(sign[0]);

    } else {
        fr16 = fri16;
        fr32 = fri32;
        return(-1);
    }
}


void fr_FSOUND_FSB_HEADER_FSB1(FILE *fd, FSOUND_FSB_HEADER_FSB1 *fh) {
    FDREB_INIT

                     frch(fd, fh->id, 4);
    fh->numsamples = fr32(fd);
    fh->datasize   = fr32(fd);
    fh->dunno_null = fr32(fd);

    rebsize = 0;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_HEADER_FSB1:\n"
            "  fh->id         %.4s\n"
            "  fh->numsamples %08x\n"
            "  fh->datasize   %08x\n"
            "  fh->dunno_null %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->id,
            fh->numsamples,
            fh->datasize,
            fh->dunno_null);
    }
}



void fr_FSOUND_FSB_HEADER_FSB2(FILE *fd, FSOUND_FSB_HEADER_FSB2 *fh) {
    FDREB_INIT

                     frch(fd, fh->id, 4);
    fh->numsamples = fr32(fd);
    fh->shdrsize   = fr32(fd);
    fh->datasize   = fr32(fd);

    rebsize = 0;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_HEADER_FSB2:\n"
            "  fh->id         %.4s\n"
            "  fh->numsamples %08x\n"
            "  fh->shdrsize   %08x\n"
            "  fh->datasize   %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->id,
            fh->numsamples,
            fh->shdrsize,
            fh->datasize);
    }
}



void fr_FSOUND_FSB_HEADER_FSB3(FILE *fd, FSOUND_FSB_HEADER_FSB3 *fh) {
    FDREB_INIT

                     frch(fd, fh->id, 4);
    fh->numsamples = fr32(fd);
    fh->shdrsize   = fr32(fd);
    fh->datasize   = fr32(fd);
    fh->version    = fr32(fd);
    fh->mode       = fr32(fd);

    rebsize = 0;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_HEADER_FSB3:\n"
            "  fh->id         %.4s\n"
            "  fh->numsamples %08x\n"
            "  fh->shdrsize   %08x\n"
            "  fh->datasize   %08x\n"
            "  fh->version    %08x\n"
            "  fh->mode       %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->id,
            fh->numsamples,
            fh->shdrsize,
            fh->datasize,
            fh->version,
            fh->mode);
    }
}



void fr_FSOUND_FSB_HEADER_FSB4(FILE *fd, FSOUND_FSB_HEADER_FSB4 *fh) {
    FDREB_INIT

                     frch(fd, fh->id, 4);
    fh->numsamples = fr32(fd);
    fh->shdrsize   = fr32(fd);
    fh->datasize   = fr32(fd);
    fh->version    = fr32(fd);
    fh->mode       = fr32(fd);
                     frch(fd, fh->zero, sizeof(fh->zero));
                     frch(fd, fh->hash, sizeof(fh->hash));

    rebsize = 0;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_HEADER_FSB4:\n"
            "  fh->id         %.4s\n"
            "  fh->numsamples %08x\n"
            "  fh->shdrsize   %08x\n"
            "  fh->datasize   %08x\n"
            "  fh->version    %08x\n"
            "  fh->mode       %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->id,
            fh->numsamples,
            fh->shdrsize,
            fh->datasize,
            fh->version,
            fh->mode);
    }
}



void fr_FSOUND_FSB_HEADER_FSB5(FILE *fd, FSOUND_FSB_HEADER_FSB5 *fh) {
    FDREB_INIT

                     frch(fd, fh->id, 4);
    fh->version    = fr32(fd);
    fh->numsamples = fr32(fd);
    fh->shdrsize   = fr32(fd);
    fh->namesize   = fr32(fd);
    fh->datasize   = fr32(fd);
    fh->mode       = fr32(fd);
                     frch(fd, fh->zero, sizeof(fh->zero));
                     frch(fd, fh->hash, sizeof(fh->hash));
                     frch(fd, fh->dummy, sizeof(fh->dummy));

    rebsize = 0;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_HEADER_FSB5:\n"
            "  fh->id         %.4s\n"
            "  fh->version    %08x\n"
            "  fh->numsamples %08x\n"
            "  fh->shdrsize   %08x\n"
            "  fh->namesize   %08x\n"
            "  fh->datasize   %08x\n"
            "  fh->mode       %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->id,
            fh->version,
            fh->numsamples,
            fh->shdrsize,
            fh->namesize,
            fh->datasize,
            fh->mode);
    }
}



void fr_FSOUND_FSB_SAMPLE_HEADER_1(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_1 *fh) {
    FDREB_INIT

                                frch(fd, fh->name, sizeof(fh->name));
    fh->lengthsamples         = fr32(fd);
    fh->lengthcompressedbytes = fr32(fd);
    fh->deffreq               = fr32(fd);
    fh->defpri                = fr16(fd);
    fh->numchannels           = fr16(fd);
    fh->defvol                = fr16(fd);
    fh->defpan                = fr16(fd);
    fh->mode                  = fr32(fd);
    fh->loopstart             = fr32(fd);
    fh->loopend               = fr32(fd);

    rebsize = 0;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_SAMPLE_HEADER_1:\n"
            "  fh->name                  %.*s\n"
            "  fh->lengthsamples         %08x\n"
            "  fh->lengthcompressedbytes %08x\n"
            "  fh->deffreq               %08x\n"
            "  fh->defpri                %04x\n"
            "  fh->numchannels           %04x\n"
            "  fh->defvol                %04x\n"
            "  fh->defpan                %04x\n"
            "  fh->mode                  %08x\n"
            "  fh->loopstart             %08x\n"
            "  fh->loopend               %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            sizeof(fh->name), fh->name,
            fh->lengthsamples,
            fh->lengthcompressedbytes,
            fh->deffreq,
            fh->defpri,
            fh->numchannels,
            fh->defvol,
            fh->defpan,
            fh->mode,
            fh->loopstart,
            fh->loopend);
    }
}



void fr_FSOUND_FSB_SAMPLE_HEADER_2(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_2 *fh) {
    FDREB_INIT

    fh->size                  = fr16(fd);
                                frch(fd, fh->name, sizeof(fh->name));
    fh->lengthsamples         = fr32(fd);
    fh->lengthcompressedbytes = fr32(fd);
    fh->loopstart             = fr32(fd);
    fh->loopend               = fr32(fd);
    fh->mode                  = fr32(fd);
    fh->deffreq               = fr32(fd);
    fh->defvol                = fr16(fd);
    fh->defpan                = fr16(fd);
    fh->defpri                = fr16(fd);
    fh->numchannels           = fr16(fd);

    rebsize = fh->size;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_SAMPLE_HEADER_2:\n"
            "  fh->size                  %04x\n"
            "  fh->name                  %.*s\n"
            "  fh->lengthsamples         %08x\n"
            "  fh->lengthcompressedbytes %08x\n"
            "  fh->loopstart             %08x\n"
            "  fh->loopend               %08x\n"
            "  fh->mode                  %08x\n"
            "  fh->deffreq               %08x\n"
            "  fh->defvol                %04x\n"
            "  fh->defpan                %04x\n"
            "  fh->defpri                %04x\n"
            "  fh->numchannels           %04x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->size,
            sizeof(fh->name), fh->name,
            fh->lengthsamples,
            fh->lengthcompressedbytes,
            fh->loopstart,
            fh->loopend,
            fh->mode,
            fh->deffreq,
            fh->defvol,
            fh->defpan,
            fh->defpri,
            fh->numchannels);
    }
}



void fr_FSOUND_FSB_SAMPLE_HEADER_3_1(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_3_1 *fh) {
    FDREB_INIT

    fh->size                  = fr16(fd);
                                frch(fd, fh->name, sizeof(fh->name));
    fh->lengthsamples         = fr32(fd);
    fh->lengthcompressedbytes = fr32(fd);
    fh->loopstart             = fr32(fd);
    fh->loopend               = fr32(fd);
    fh->mode                  = fr32(fd);
    fh->deffreq               = fr32(fd);
    fh->defvol                = fr16(fd);
    fh->defpan                = fr16(fd);
    fh->defpri                = fr16(fd);
    fh->numchannels           = fr16(fd);
    fh->mindistance           = (F_FLOAT)fr32(fd);
    fh->maxdistance           = (F_FLOAT)fr32(fd);
    fh->varfreq               = fr32(fd);
    fh->varvol                = fr16(fd);
    fh->varpan                = fr16(fd);

    rebsize = fh->size;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_SAMPLE_HEADER_3_1:\n"
            "  fh->size                  %04x\n"
            "  fh->name                  %.*s\n"
            "  fh->lengthsamples         %08x\n"
            "  fh->lengthcompressedbytes %08x\n"
            "  fh->loopstart             %08x\n"
            "  fh->loopend               %08x\n"
            "  fh->mode                  %08x\n"
            "  fh->deffreq               %08x\n"
            "  fh->defvol                %04x\n"
            "  fh->defpan                %04x\n"
            "  fh->defpri                %04x\n"
            "  fh->numchannels           %04x\n"
            "  fh->mindistance           %f\n"
            "  fh->maxdistance           %f\n"
            "  fh->varfreq               %08x\n"
            "  fh->varvol                %04x\n"
            "  fh->varpan                %04x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->size,
            sizeof(fh->name), fh->name,
            fh->lengthsamples,
            fh->lengthcompressedbytes,
            fh->loopstart,
            fh->loopend,
            fh->mode,
            fh->deffreq,
            fh->defvol,
            fh->defpan,
            fh->defpri,
            fh->numchannels,
            fh->mindistance,
            fh->maxdistance,
            fh->varfreq,
            fh->varvol,
            fh->varpan);
    }
}



void fr_FSOUND_FSB_SAMPLE_HEADER_BASIC(FILE *fd, FSOUND_FSB_SAMPLE_HEADER_BASIC *fh, int moresize) {
    FDREB_INIT

    fh->lengthsamples         = fr32(fd);
    fh->lengthcompressedbytes = fr32(fd);

    rebsize = sizeof(FSOUND_FSB_SAMPLE_HEADER_BASIC) + moresize;
    add_to_reb_file(fd);

    if(verbose) {
        printf("\n"
            "- %08x fr_FSOUND_FSB_SAMPLE_HEADER_BASIC:\n"
            "  fh->lengthsamples         %08x\n"
            "  fh->lengthcompressedbytes %08x\n",
            (u32)myftell(fd) - sizeof(*fh),
            fh->lengthsamples,
            fh->lengthcompressedbytes);
    }
}



void pcmwav_header(FILE *fd, int freq, u16 chans, u16 bits, u32 rawlen) {
    mywav_fmtchunk  fmt;

    fmt.wFormatTag       = 0x0001;
    fmt.wChannels        = chans;
    fmt.dwSamplesPerSec  = freq;
    fmt.wBitsPerSample   = bits;
    fmt.wBlockAlign      = (fmt.wBitsPerSample / 8) * fmt.wChannels;
    fmt.dwAvgBytesPerSec = fmt.dwSamplesPerSec * fmt.wBlockAlign;
    mywav_writehead(fd, &fmt, rawlen, NULL, 0);
}



void xbox_ima_header(FILE *fd, int freq, u16 chans, u32 rawlen) {
    mywav_fmtchunk  fmt;

    force_ima_x = 1;
    fmt.wFormatTag       = force_ima ? 0x0011 : 0x0069;
    fmt.wChannels        = chans;
    fmt.dwSamplesPerSec  = freq;
    fmt.wBitsPerSample   = 4;
    fmt.wBlockAlign      = 36 * fmt.wChannels;
    fmt.dwAvgBytesPerSec = (689 * fmt.wBlockAlign) + 4; // boh, not important
    //fmt.wBlockAlign      = fmt.wBitsPerSample * fmt.wChannels * 9;  // 9???
    //fmt.dwAvgBytesPerSec = 49612;                                   // 49612???
    mywav_writehead(fd, &fmt, rawlen, "\x02\x00" "\x40\x00", 4);
}



void its_header(FILE *fd, u8 *fname, u16 chans, u16 bits, u32 rawlen) {
    u8      flags;

    /* note that doesn't seem possible to know if the sample has been encoded with 2.14 or 2.15 */
    flags = 1 | 8;  // 8 for compression
    if(bits == 16)  flags |= 2;
    if(chans == 2)  flags |= 4;

    fwch(fd, "IMPS", 4);        // DWORD id
    fwch(fd, fname, 12);        // CHAR filename[12]
    fw08(fd,  0);               // BYTE zero
    fw08(fd,  128);             // BYTE gvl
    fw08(fd,  0);               // BYTE flags
    fw08(fd,  64);              // BYTE vol
    fwch(fd, fname, 26);        // CHAR name[26]
    fw08(fd,  0xff);            // BYTE cvt
    fw08(fd,  0x7f);            // BYTE dfp
    fwi32(fd, rawlen);          // DWORD length
    fwi32(fd, 0);               // DWORD loopbegin
    fwi32(fd, 0);               // DWORD loopend
    fwi32(fd, 8363);            // DWORD C5Speed
    fwi32(fd, 0);               // DWORD susloopbegin
    fwi32(fd, 0);               // DWORD susloopend
    fwi32(fd, 0);               // DWORD samplepointer
    fw08(fd,  0);               // BYTE vis
    fw08(fd,  0);               // BYTE vid
    fw08(fd,  0);               // BYTE vir
    fw08(fd,  0);               // BYTE vit
}



void genh_header(FILE *fd, int freq, u16 chans, u32 rawlen, u8 *coeff, int coeffsz) {
    static const int    genhsz  = 0x80; // in case of future additions to the format
    int     i,
            j;

    fwb32(fd, 0x47454e48);              // 0    magic
    fwi32(fd, chans);                   // 4    channel_count
    fwi32(fd, 2);                       // 8    interleave
    fwi32(fd, freq);                    // c    sample_rate
    fwi32(fd, 0xffffffff);              // 10   loop_start
    fwi32(fd, ((rawlen*14)/8)/chans);   // 14   loop_end
    fwi32(fd, 12);                      // 18   codec
    fwi32(fd, genhsz + (chans * 32));   // 1c   start_offset
    fwi32(fd, genhsz + (chans * 32));   // 20   header_size
    fwi32(fd, genhsz);                  // 24   coef[0]
    fwi32(fd, genhsz + 32);             // 28   coef[1]
    fwi32(fd, 1);                       // 2c   dsp_interleave_type
    fwi32(fd, 0);                       // 30   coef_type
    fwi32(fd, genhsz);                  // 34   coef_splitted[0]
    fwi32(fd, genhsz + 32);             // 38   coef_splitted[1]
    for(i = ftell(fd); i < genhsz; i++) fputc(0, fd);
    for(i = 0; i < chans; i++) {
        if(coeff && (coeffsz >= 0x2e)) {
            fwch(fd, coeff, 32);
            coeff   += 0x2e;
            coeffsz -= 0x2e;
        } else {
            for(j = 0; j < 16; j++) fwi16(fd, 0);
        }
    }
}



void brstm_header(FILE *fd, int freq, u16 chans, u32 rawlen, u8 *coeff, int coeffsz) {
    int     i,
            j;

    fwb32(fd, 0x5253544D);          // RSTM
    fwb32(fd, 0xFEFF0100);
    fwb32(fd, 0x40 + 0x80 + (chans * 0x38) + 0x10 + 0x20 + rawlen);
    fwb16(fd, 0);
    fwb16(fd, 0);
    fwb32(fd, 0x40);                // head offset
    fwb32(fd, 0x80 + (chans * 0x38) + 0x10);   // head length
    fwb32(fd, 0);                   // ADPC offset
    fwb32(fd, 0);                   // ADPC size
    fwb32(fd, 0x40 + 0x80 + (chans * 0x38) + 0x10); // DATA offset
    fwb32(fd, 0x20 + rawlen);       // DATA size
    for(i = 0; i < 0x18; i++) fw08(fd, 0);
    fwb32(fd, 0x48454144);          // HEAD
    for(i = 0; i < 12; i++) fw08(fd, 0);
    fwb32(fd, 0);
    fwb32(fd, 0);
    fwb32(fd, 0);
    fwb32(fd, 0x5c);                // coef_offset1
    fw08(fd,  2);                   // codec
    fw08(fd,  0);
    fw08(fd,  chans);
    fw08(fd,  0);
    fwb16(fd, freq);
    fwb16(fd, 0);
    fwb32(fd, 0);                   // loop start
    fwb32(fd, ((rawlen*14)/8)/chans);   // num_samples
    fwb32(fd, 0x40 + 0x80 + (chans * 0x38) + 0x10 + 0x20);
    fwb32(fd, 0);
    fwb32(fd, 0x2000);              // interleave block size, brstm are not good because there is no support for byte_interleaving!
    for(i = 0; i < 12; i++) fw08(fd, 0);
    fwb32(fd, 0x2000);              // interleave small block size
    for(i = 0; i < 32; i++) fw08(fd, 0);
    fwb32(fd, 0x70);                // coef_offset2
    for(i = 0; i < 16; i++) fw08(fd, 0);
    for(i = 0; i < chans; i++) {
        if(coeff && (coeffsz >= 0x2e)) {
            fwch(fd, coeff, 0x2e);
            coeff   += 0x2e;
            coeffsz -= 0x2e;
        } else {
            for(j = 0; j < 0x2e; j++) fw08(fd, 0);
        }
        fwb32(fd, 0);
        fwb32(fd, 0);
        fwb16(fd, 0);
    }
    for(i = 0; i < 16; i++) fw08(fd, 0);
    fwb32(fd, 0x44415441);
    fwb32(fd, ((rawlen*14)/8)/chans);
    fwb32(fd, 0x18);
    fwb32(fd, 0);
    for(i = 0; i < 16; i++) fw08(fd, 0);
}



void adx_header(FILE *fd, int freq, u16 chans, u32 rawlen) {
    fwb16(fd, 0x8000);  // 0x8000
    fwb16(fd, 0x1a);    // copyright offset
    fw08(fd,  3);       // encoding type
    fw08(fd,  18);      // block size
    fw08(fd,  4);       // sample bitdepth
    fw08(fd,  chans);   // channel count
    fwb32(fd, freq);    // sample rate
    fwb32(fd, ((rawlen*32)/18)/chans);  // total samples
    fwb16(fd, 500);     // highpass frequency
    fw08(fd,  0x03);    // version
    fw08(fd,  0x00);    // flags
    fwb32(fd, 0);       // unknown
    fwch(fd, "(c)CRI", 6);
}



void ss2_header(FILE *fd, int freq, u16 chans, u32 rawlen) {
    fwb32(fd, 0x53536864);
    fwi32(fd, 0x18);
    fwi32(fd, 0x10);
    fwi32(fd, freq);
    fwi32(fd, chans);
    fwi32(fd, rawlen / chans);  // seems to be the correct interleave value
    fwi32(fd, 0);
    fwi32(fd, 0xffffffff);
    fwb32(fd, 0x53536264);
    fwi32(fd, rawlen);
}



void vag_header(FILE *fd, u8 *fname, int freq, u32 rawlen) {
    int     i;

    fwch(fd, "VAGp", 4);
    fwb32(fd, 32);
    fwb32(fd, 0);
    fwb32(fd, rawlen);
    fwb32(fd, freq);
    for(i = 0; i < 12; i++) fputc(0, fd);
    fwch(fd, fname, 16);
    for(i = 0; i < 16; i++) fputc(0, fd);
}



char *show_mode(u32 mode, int *xcodec, u16 *xchans, u16 *xbits) {
    static char m[300];

    m[0] = 0;
    if(mode & FSOUND_LOOP_OFF)      strcat(m, "noloop,");
    if(mode & FSOUND_LOOP_NORMAL)   strcat(m, "loop,");
    if(mode & FSOUND_LOOP_BIDI)     strcat(m, "biloop,");
    if(mode & FSOUND_8BITS)         strcat(m, "8,");
    if(mode & FSOUND_16BITS)        strcat(m, "16,");
    if(mode & FSOUND_MONO)          strcat(m, "mono,");
    if(mode & FSOUND_STEREO)        strcat(m, "stereo,");
    if(mode & FSOUND_UNSIGNED)      strcat(m, "unsign,");
    if(mode & FSOUND_SIGNED)        strcat(m, "sign,");
    if(mode & FSOUND_DELTA)         strcat(m, "delta,");
    if(mode & FSOUND_IT214)         strcat(m, "IT_2.14,");
    if(mode & FSOUND_IT215)         strcat(m, "IT_2.15,");
    if(mode & FSOUND_HW3D)          strcat(m, "hw3d,");
    if(mode & FSOUND_2D)            strcat(m, "2d,");
    if(mode & FSOUND_STREAMABLE)    strcat(m, "stream,");
    if(mode & FSOUND_LOADMEMORY)    strcat(m, "memory,");
    if(mode & FSOUND_LOADRAW)       strcat(m, "raw,");
    if(mode & FSOUND_MPEGACCURATE)  strcat(m, "acc_mpeg,");
    if(mode & FSOUND_FORCEMONO)     strcat(m, "force_mono,");
    if(mode & FSOUND_HW2D)          strcat(m, "hw2d,");
    if(mode & FSOUND_ENABLEFX)      strcat(m, "effects,");
    if(mode & FSOUND_MPEGHALFRATE)  strcat(m, "half_mpeg,");
    if(mode & FSOUND_IMAADPCM)      strcat(m, "ima_adpcm,");
    if(mode & FSOUND_VAG)           strcat(m, "vag,");
    //if(mode & FSOUND_NONBLOCKING)   strcat(m, "non_block,");
    if(mode & FSOUND_XMA)           strcat(m, "xma,");
    if(mode & FSOUND_GCADPCM)       strcat(m, "GC_adpcm,");
    if(mode & FSOUND_MULTICHANNEL)  strcat(m, "multichan,");
    if(mode & FSOUND_USECORE0)      strcat(m, "00-23,");    // celt
    if(mode & FSOUND_USECORE1)      strcat(m, "24-47,");
    if(mode & FSOUND_LOADMEMORYIOP) strcat(m, "memory,");
    if(mode & FSOUND_IGNORETAGS)    strcat(m, "notags,");
    if(mode & FSOUND_STREAM_NET)    strcat(m, "netstream,");
    // if(mode & FSOUND_NORMAL)        strcat(m, "normal,");
    if(m[0]) m[strlen(m) - 1] = 0;

    if(xcodec) {
        *xcodec = FMOD_SOUND_FORMAT_PCM16;
        if(mode & FSOUND_DELTA)     *xcodec = FMOD_SOUND_FORMAT_MPEG;
        if(mode & FSOUND_IT214)     *xcodec = FMOD_SOUND_FORMAT_IT214;
        if(mode & FSOUND_IT215)     *xcodec = FMOD_SOUND_FORMAT_IT215;
        if(mode & FSOUND_IMAADPCM)  *xcodec = FMOD_SOUND_FORMAT_IMAADPCM;
        if(mode & FSOUND_VAG)       *xcodec = FMOD_SOUND_FORMAT_VAG;
        if(mode & FSOUND_GCADPCM)   *xcodec = FMOD_SOUND_FORMAT_GCADPCM;
        if(mode & FSOUND_XMA)       *xcodec = FMOD_SOUND_FORMAT_XMA;
        if(mode & FSOUND_USECORE0)  *xcodec = FMOD_SOUND_FORMAT_CELT;
        if(mode & FSOUND_8BITS) {
            if(*xcodec == FMOD_SOUND_FORMAT_PCM16) *xcodec = FMOD_SOUND_FORMAT_PCM8;
        }
    }
    if(xchans) {
        *xchans = 1;
        if(mode & FSOUND_MONO)      *xchans = 1;
        if(mode & FSOUND_STEREO)    *xchans = 2;
    }
    if(xbits) {
        *xbits = 16;
        if(mode & FSOUND_8BITS)     *xbits = 8;
        if(mode & FSOUND_16BITS)    *xbits = 16;
    }
    return(m);
}



void create_dir(u8 *fname) {
    u8      *p,
            *l;

    p = strchr(fname, ':');
    if(p) *p = '_';
    for(p = fname; *p && ((*p == '\\') || (*p == '/')); p++) *p = '_';

    for(p = fname; ; p = l + 1) {
        for(l = p; *l && (*l != '\\') && (*l != '/'); l++);
        if(!*l) break;
        *l = 0;

        if(!strncmp(p, "..", 2)) memcpy(p, "__", 2);

        make_dir(fname);
        *l = PATHSLASH;
    }
}



void tmp_extension(u8 *fname, u8 *ext) {
    u8      *wavext;

    wavext = strrchr(fname, '.');
    if(!wavext || (wavext && (strlen(wavext + 1) > 3))) {
        memmove(fname + strlen(fname), ext, strlen(ext) + 1);   // don't touch memmove!
    } else if(wavext && (strlen(wavext + 1) < 3)) {
        memmove(wavext, ext, strlen(ext) + 1);                  // don't touch memmove!
    }
}



void put_extension(u8 *fname, u8 *ext, int skip_known) {
    u8      oldext_buff[strlen(fname) + 1],
            *wavext;

    wavext = strrchr(fname, '.');
    if(wavext) {
        if(skip_known) {
            oldext_buff[0] = 0;
            experimental_extension_guessing(fname, oldext_buff, fname + strlen(fname));
            if(oldext_buff[0]) {
                strcpy(wavext + 1, ext);
            } else if(!stricmp(wavext + 1, ext)) {
                // do nothing
            } else {
                sprintf(fname + strlen(fname), ".%s", ext);
            }
        } else {
            strcpy(wavext + 1, ext);
        }
    } else {
        sprintf(fname + strlen(fname), ".%s", ext);
    }
}



int mylencmp(u8 *str1, u8 *str2) {
    int     equal;

    if(strlen(str1) > strlen(str2)) return(0);

    for(equal = 0; *str1; str1++, str2++) {
        if(tolower(*str1) != tolower(*str2)) break;
        equal++;
    }
    return(equal);
}



void experimental_extension_guessing(u8 *fname, u8 *oldext, u8 *end) {
    int     ext[16] = {0},
            maxnamelen = FSOUND_FSB_NAMELEN;
    u8      *wavext;

    if(head_ver == 1) maxnamelen = 32;
    if((end - fname) < maxnamelen) return;

    memset(ext, 0, sizeof(ext));

    wavext = strrchr(fname, '.');
    if(wavext) {
        wavext++;
        if(strlen(wavext) <= 3) strcpy(oldext, wavext - 1);
        if((end - fname) < maxnamelen) return;

        ext[0]  = mylencmp(wavext, "wav");
        ext[1]  = mylencmp(wavext, "wma");
        ext[2]  = mylencmp(wavext, "xma");
        ext[3]  = mylencmp(wavext, "vag");
        ext[4]  = mylencmp(wavext, "mp3");
        ext[5]  = mylencmp(wavext, "ogg");
        ext[6]  = mylencmp(wavext, "celt");
        ext[7]  = mylencmp(wavext, "it");
        ext[8]  = mylencmp(wavext, "genh");
        ext[9]  = mylencmp(wavext, "ss2");
        // these are enough for the moment
    }

         if(ext[0])  strcpy(oldext, ".wav");
    else if(ext[1])  strcpy(oldext, ".wma");
    else if(ext[2])  strcpy(oldext, ".xma");
    else if(ext[3])  strcpy(oldext, ".vag");
    else if(ext[4])  strcpy(oldext, ".mp3");
    else if(ext[5])  strcpy(oldext, ".ogg");
    else if(ext[6])  strcpy(oldext, ".celt");
    else if(ext[7])  strcpy(oldext, ".it");
    else if(ext[8])  strcpy(oldext, ".genh");
    else if(ext[9])  strcpy(oldext, ".ss2");
    else if(!oldext[0] || !wavext[0]) strcpy(oldext, ".wav");

}



void add_extension(u8 *fname, int add, int extract, int add_guess) {
    struct stat xstat;
    int     i;
    u8      oldext_buff[strlen(fname) + 1], // remember "CorporateMundo.spellcasteffor"
            *oldext,
            *end;

    end = fname + strlen(fname);

    if(add) sprintf(end, "_%u", add);

    oldext = oldext_buff;
    oldext[0] = 0;
    experimental_extension_guessing(fname, oldext, end);
    if(add_guess && oldext[0]) {
        tmp_extension(fname, oldext);
    } else {
        oldext = strrchr(fname, '.');
        if(oldext) {
            strcpy(oldext_buff, oldext);
            end = oldext;
        }
        oldext = oldext_buff;
    }

    if(!extract) return;

    for(i = (add ? add : 1); !stat(fname, &xstat); i++) {
        sprintf(end, "_%u", i);
        tmp_extension(fname, oldext);
    }
}



u32 putfile(FILE *fd, /*int idx,*/ u8 *fname) {
    static u8   *tmpname = NULL;
    FILE    *fdi;
    u32     size,
            max_size = -1;
    int     t;
    u8      buff[4096],
            *ext;

    if(!fname || !fname[0]) {
        //sprintf(fname, NULLNAME, idx);
        return(0);
    }
    t = strlen(fname) + 10 + 4 + 1;
    tmpname = realloc(tmpname, t);
    strcpy(tmpname, fname);
    fname = tmpname;

    ext = strrchr(fname, '.');
    if(!ext) ext = fname + strlen(fname);
    for(t = 0;; t++) {
        switch(t) {
            case 0:                             break;
            case 1: strcpy(ext, ".mp3");        break;
            case 2: strcpy(ext, ".it");         break;
            case 3: strcpy(ext, ".wav");        break;
            case 4: strcpy(ext, ".ss2");        break;
            case 5: strcpy(ext, ".genh");       break;
            case 6: strcpy(ext, ".xma");        break;
            case 7: strcpy(ext, ".celt");       break;
            default: std_err();                 break;
        }
        printf("- open file \"%s\"\n", fname);
        fdi = fopen(fname, "rb");
        if(fdi) break;
    }

    if(fread(buff, 1, 4, fdi) != 4) {
        memset(buff, 0, 4); // so that the comparison fails
    }
    if(!memcmp(buff, "RIFF", 4) || !memcmp(buff, "RIFX", 4) /*|| !memcmp(buff, "XMA2", 4)*/) {
        max_size = mywav_seekchunk(fdi, "data");
        if(max_size == -1) fseek(fdi, 0, SEEK_SET);

    } else if(!memcmp(buff, "IMPS", 4)) {
        if(fseek(fdi, 80, SEEK_SET)) std_err();

    } else if(!memcmp(buff, "GENH", 4)) {
        fri32(fdi);     // 4    channel_count
        fri32(fdi);     // 8    interleave
        fri32(fdi);     // c    sample_rate
        fri32(fdi);     // 10   loop_start
        fri32(fdi);     // 14   loop_end
        fri32(fdi);     // 18   codec
        t = fri32(fdi); // 1c   start_offset
        if(fseek(fdi, t, SEEK_SET)) std_err();

    /* brstm is not needed
    } else if(!memcmp(buff, "RSTM", 4)) {
        frb32(fdi);
        frb32(fdi);
        frb16(fdi);
        frb16(fdi);
        frb32(fdi);     // head offset
        frb32(fdi);     // head length
        frb32(fdi);     // ADPC offset
        frb32(fdi);     // ADPC size
        t = frb32(fdi); // DATA offset
        if(fseek(fdi, t, SEEK_SET)) std_err(); */

    // it's necessary to compare at least 4 bytes and have a good signature to avoid false positives
    //} else if(!memcmp(buff, "\x80\x00", 2)) {
        //t = (buff[2] << 8) | buff[3];
        //t -= 2;
        //if(fseek(fdi, t, SEEK_SET)) std_err();

    } else if(!memcmp(buff, "SShd", 4)) {
        if(fseek(fdi, 40, SEEK_SET)) std_err();

    } else if(!memcmp(buff, "VAGp", 4)) {
        if(fseek(fdi, 64, SEEK_SET)) std_err();

    } else {    // reset
        fseek(fdi, 0, SEEK_SET);
    }

    // size and max_size are "unsigned"
    for(size = 0; (t = fread(buff, 1, sizeof(buff), fdi)) && (size < max_size); size += t) {
        if((size + t) > max_size) t = max_size - size;
        fwch(fd, buff, t);
    }

    fclose(fdi);
    return(size);
}



int dump_file(FILE *fd, FILE *fdo, int len, int bits) {
    int     t,
            n,
            j;
    u8      buff[1024];

    if(!pcm_endian || (bits < 16) || (bits > 32)) { // normal fast copy
        for(t = sizeof(buff); len; len -= t) {
            if(t > len) t = len;
            n = myfr(fd, buff, t);
            fwch(fdo, buff, n);
            if(n != t) goto quit;
        }
    } else {    // automatically converts endianess
        for(t = bits / 8; len; len -= t) {
            if(t > len) t = len;
            n = myfr(fd, buff, t);
            for(j = t - 1; j >= 0; j--) {
                fwch(fdo, buff + j, 1);
            }
            if(n != t) goto quit;
        }
    }
    return(0);
quit:
    printf("\n- Alert: the extracted file is incomplete! I continue\n");
    return(-1);
}



void extract_file(FILE *fd, u8 *fname, int freq, u16 chans, u16 bits, u32 len, u8 *moresize_dump, int moresize, int samples) {
    FILE    *fdo;
    int     pcm = 0;

    create_dir(fname);  // mainly for security

    add_extension(fname, 0, 1, 1);
    if(addhead) {       // force the right extension, I think it's a good idea
        switch(codec) {
            case FMOD_SOUND_FORMAT_GCADPCM: put_extension(fname, "genh", 1);    break;
            case FMOD_SOUND_FORMAT_IMAADPCM:put_extension(fname, "wav",  1);    break;
            case FMOD_SOUND_FORMAT_VAG:     put_extension(fname, "ss2",  1);    break;
            case FMOD_SOUND_FORMAT_HEVAG:   put_extension(fname, "ss2",  1);    break;
            case FMOD_SOUND_FORMAT_XMA:     put_extension(fname, "xma",  1);    break;
            case FMOD_SOUND_FORMAT_MPEG:    put_extension(fname, "mp3",  1);    break;
            case FMOD_SOUND_FORMAT_CELT:    put_extension(fname, "celt", 1);    break;
            case FMOD_SOUND_FORMAT_AT9:     put_extension(fname, "at9",  1);    break;
            case FMOD_SOUND_FORMAT_XWMA:    put_extension(fname, "xwma", 1);    break;
            case FMOD_SOUND_FORMAT_VORBIS:  put_extension(fname, "ogg",  1);    break;
            case FMOD_SOUND_FORMAT_IT214:   put_extension(fname, "it",   1);    break;
            case FMOD_SOUND_FORMAT_IT215:   put_extension(fname, "it",   1);    break;
            default:                        put_extension(fname, "wav",  1);    break;
        }
    }
    add_extension(fname, 0, 1, 0);

    fdo = fopen(fname, "wb");
    if(!fdo) std_err();
    if(fdreb) fwchs(fdreb, fname);

    if(!chans) chans = 1;   // useless, should never happen

    if(addhead) {
        switch(codec) {
            case FMOD_SOUND_FORMAT_GCADPCM: genh_header(fdo, freq, chans, len, moresize_dump, moresize); break;
            case FMOD_SOUND_FORMAT_IMAADPCM:xbox_ima_header(fdo, freq, chans, len); break;
            case FMOD_SOUND_FORMAT_VAG:     ss2_header(fdo, freq, chans, len); break;
            case FMOD_SOUND_FORMAT_HEVAG:   ss2_header(fdo, freq, chans, len); break;
            case FMOD_SOUND_FORMAT_XMA:     xma2_header(fdo, freq, chans, bits, len, moresize_dump, moresize, samples); break;
            case FMOD_SOUND_FORMAT_MPEG:    /* mp3 files have no header */ break;
            case FMOD_SOUND_FORMAT_CELT:    /* no header? */ break;
            case FMOD_SOUND_FORMAT_AT9:     /* ??? */ break;
            case FMOD_SOUND_FORMAT_XWMA:    /* ??? */ break;
            case FMOD_SOUND_FORMAT_VORBIS:  /* ??? */ break;
            case FMOD_SOUND_FORMAT_IT214:   its_header(fdo, fname, chans, bits, len); break;
            case FMOD_SOUND_FORMAT_IT215:   its_header(fdo, fname, chans, bits, len); break;
            default:                        pcm = bits; pcmwav_header(fdo, freq, chans, bits, len); break;
        }
    }

    dump_file(fd, fdo, len, pcm);
    fclose(fdo);
}



void delimit(u8 *data) {
    while(*data && (*data != '\n') && (*data != '\r')) data++;
    *data = 0;
}



void xor(u8 *data, int len, u8 *key) {
    int     i;
    u8      *k;

    k = key;
    for(i = 0; i < len; i++) {
        if(!*k) k = key;
        data[i] ^= *k;
        k++;
    }
}



int fsb_file_crypt(FILE *fd, FILE *fdo) {
    static u8   *buff = NULL;
    int     len;

    if(!buff) {
        buff = malloc(4096);
        if(!buff) std_err();
    }
    myfseek(fd, 0, SEEK_SET);
    printf("- wait the encryption process\n");
    while((len = myfr(fd, buff, 4096))) {
        fwch(fdo, buff, len);
    }
    printf("- decryption finished\n");
    return(0);
}



// sort of check_sign_endian
int fsb_file_crypt_scan(FILE *fd) {
    int     type,
            len;
    u8      buff[4];

    for(type = 0;; type++) {
        len = fread(buff, 1, 4, fd);    // NOT myfr
        myfseek(fd, 0, SEEK_SET);
        switch(type) {
            case 0: fsbdec0(buff, len, fsb_key, 0); break;
            case 1: fsbdec1(buff, len, fsb_key, 0); break;
            default: {
                printf("\nError: your password seems wrong\n");
                myexit(1);
                break;
            }
        }
        if(!memcmp(buff, "FSB", 3) || !memcmp(buff + 1, "BSF", 3)) break;
    }
    return(type);
}



FILE *try_fsbdec(FILE *fd, u8 *fsb_password) {
    int     len;
    u8      hexsize[HEXSIZE];

    for(;;) {
        myfseek(fd, 0, SEEK_SET);

        if(fsb_password) {
            fsb_keysz = sprintf(fsb_key, "%.*s", sizeof(fsb_key) - 1, fsb_password);
            fsb_password = NULL;    // if it's wrong it will be reasked
            break;
        }
        printf(
            "- probably the file uses encryption, insert the needed keyword:\n"
            "  type ? for viewing the hex dump of the first %d bytes of the file because\n"
            "  it's possible to see part of the plain-text password in the encrypted file!\n"
            "  ", HEXSIZE);
        fflush(stdin);
        fgets(fsb_key, sizeof(fsb_key), stdin);
        delimit(fsb_key);
        if(strcmp(fsb_key, "?")) break;

        printf("- encryption type 1\n");
        len = fread(hexsize, 1, HEXSIZE, fd);
        show_dump(hexsize, len, stdout);
        fputc('\n', stdout);

        printf("- encryption type 2\n");
        fsbdec1(hexsize, len, "", 0);
        xor(hexsize, 4, "FSB"); // "4"
        show_dump(hexsize, len, stdout);
        fputc('\n', stdout);
    }

    fsb_keysz = strlen(fsb_key);
    printf("- use encryption type %d\n", fsb_enctype);
    fsb_enctype = fsb_file_crypt_scan(fd);
    fsb_keyc = 0;

    /* now the decryption is performed in real-time
    fdo = tmpfile();
    fsb_file_crypt(fd, fdo);
    fclose(fd);
    fd = fdo;
    fflush(fd);
    */
    return(fd);
}



u8 fsbdec(u8 t) {
    return((((((((t & 64) | (t >> 2)) >> 2) | (t & 32)) >> 2) | (t & 16)) >> 1) |
           (((((((t &  2) | (t << 2)) << 2) | (t &  4)) << 2) | (t &  8)) << 1));
}



int fsbdec0(u8 *data, int len, u8 *key, int keyc) {
    u8      *k,
            *p;

    p = data;
    for(k = key + keyc; len--; p++) {
        if(!*k) k = key;
        if(data) *p = fsbdec(*p ^ *k);
        if(*k) k++;
    }
    return(k - key);
}



int fsbdec1(u8 *data, int len, u8 *key, int keyc) {
    u8      *k,
            *p;

    p = data;
    for(k = key + keyc; len--; p++) {
        if(!*k) k = key;
        if(data) *p = fsbdec(*p) ^ *k;
        if(*k) k++;
    }
    return(k - key);
}



u32 char_crc(u8 *data) {
    static const u32   crctable[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };
    u32     crc;

    for(crc = 0xffffffff; *data; data++) {
        crc = crctable[tolower(*data) ^ (crc & 0xff)] ^ (crc >> 8);
    }
    return(~crc);
}



u8 *mystrrchrs(u8 *str, u8 *chrs) {
    int     i;
    u8      *p,
            *ret = NULL;

    if(str) {
        for(i = 0; chrs[i]; i++) {
            p = strrchr(str, chrs[i]);
            if(p) {
                str = p;
                ret = p;
            }
        }
    }
    return(ret);
}



#ifdef WIN32
char *get_file(char *title, int fsb, int multi) {
    OPENFILENAME    ofn;
    int     maxlen;
    char    *filename;

    if(multi) {
        maxlen = 32768; // 32k limit ansi, no limit unicode
    } else {
        maxlen = PATHSZ;
    }
    filename = malloc(maxlen + 1);
    if(!filename) std_err();
    filename[0] = 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize     = sizeof(ofn);
    if(fsb) {
        ofn.lpstrFilter =
            "FSB archive\0" "*.fsb;*.xen\0"
            "(*.*)\0"       "*.*\0"
            "\0"            "\0";
    } else {
        ofn.lpstrFilter =
            "(*.*)\0"       "*.*\0"
            "\0"            "\0";
    }
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = filename;
    ofn.nMaxFile        = maxlen;
    ofn.lpstrTitle      = title;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                          OFN_LONGNAMES     | OFN_EXPLORER |
                          OFN_HIDEREADONLY  | OFN_ENABLESIZING;
    if(multi) ofn.Flags |= OFN_ALLOWMULTISELECT;

    printf("- %s\n", ofn.lpstrTitle);
    if(!GetOpenFileName(&ofn)) exit(1); // terminate immediately
    return(filename);
}

char *get_folder(char *title) {
    OPENFILENAME    ofn;
    char    *p;
    char    *filename;

    filename = malloc(PATHSZ + 1);
    if(!filename) std_err();

    strcpy(filename, "enter in the output folder and press Save");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize     = sizeof(ofn);
    ofn.lpstrFilter     = "(*.*)\0" "*.*\0" "\0" "\0";
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = filename;
    ofn.nMaxFile        = PATHSZ;
    ofn.lpstrTitle      = title;
    ofn.Flags           = OFN_PATHMUSTEXIST | /*OFN_FILEMUSTEXIST |*/
                          OFN_LONGNAMES     | OFN_EXPLORER |
                          OFN_HIDEREADONLY  | OFN_ENABLESIZING;

    printf("- %s\n", ofn.lpstrTitle);
    if(!GetSaveFileName(&ofn)) exit(1); // terminate immediately
    p = mystrrchrs(filename, "\\/");
    if(p) *p = 0;
    return(filename);
}
#endif



int get_num(u8 *data) {
    int     num,
            sign    = 1;

    if(data[0] == '-') {
        sign = -1;
        data++;
    }
    if((strlen(data) > 1) && (tolower(data[1]) == 'x')) {
        sscanf(data + 2, "%x", &num);
    } else if(sign < 0) {
        if(!((data[0] >= '0') && (data[0] <= '9'))) {
            if(data[0] != 'l') {    // yeah l and 1 are very similar
                fprintf(stderr, "\n"
                    "Error: recheck your options because seems that some arguments are wrong\n"
                    "       for example \"%s\" should be a number\n", data);
                exit(1);
            }
        }
        sscanf(data, "%i", &num);
    } else {
        sscanf(data, "%u", &num);
    }
    if(sign < 0) num = -num;
    return(num);
}



void read_err(void) {
    fprintf(stderr, "\nError: the file contains unexpected data or is smaller than expected\n\n");
    myexit(1);
}



void write_err(void) {
    fprintf(stderr, "\nError: impossible to write the output file, probably your disk space is finished\n\n");
    myexit(1);
}



void std_err(void) {
    perror("\nError");
    myexit(1);
}



void myexit(int ret) {
#ifdef WIN32
    u8      ans[16];

    if(GetWindowLong(mywnd, GWL_WNDPROC)) {
        printf("\nPress RETURN to quit");
        fgets(ans, sizeof(ans), stdin);
    }
#endif
    exit(ret);
}

