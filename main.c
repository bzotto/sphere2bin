//
//  sphere2bin
//
//  Utility to convert raw cassette tape data for the Sphere 1 (and related)
//  early microcomputer systems into data-only binary file(s) of the cassette
//  block(s).
//
//  The Sphere cassette physical format uses the 300bps Kansas City/Byte standard
//  for raw byte stream audio. The logical data format consists of one or more
//  named "blocks" of data stored in a binary format. The format of a block is as
//  follows:
//     - 3x sync bytes (0x16)
//     - One escape marker (0x1B)
//     - Two bytes data length (stored big endian)
//     - Two bytes block "name" (typically ASCII)
//     - Binary data bytes (count of bytes equal to length given above, *plus one*)
//     - End of transmission marker (0x17)
//     - Checksum byte (see below)
//     - Three additional trailer bytes (typically same value as checksum)
//   The checksum is computed as a running sum of the values in the data portion
//   (only). An 8-bit counter is used for the sum which just rolls over as needed.
//
//   A cassette can have more than one of these blocks of data present, which
//   are differentiated by the user by "name". Unlike some other tape formats,
//   there is no information about load address stored on the tape; the user is
//   expected to provide the load address (and request the block by name).
//
//   This utility will accept a raw Sphere cassette dump (ie, after conversion
//   from an audio signal) and emit its consitutent blocks as individual binary
//   files.
//
//  Copyright (c) Ben Zotto 2022.
//  See LICENSE for licensing information.
//

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include "spherecas.h"

// Global information accessed by the read callback function.
static int ListOnly = 0;
static int CurrentBlockIndex = 0;
static char * FilenameBase = NULL;

// Fwd declaration
static char * remove_path_extension(const char * str);

void print_usage(const char * name) {
    printf("usage: %s [-l] input_file\n", name);
    printf("\t-l (--list): Only list the blocks found in input (ignores other options).\n");
}

int main(int argc, char **argv) {
    
    if (argc == 1) {
        print_usage(argv[0]);
        return -1;
    }

    const char * input_file_name = NULL;
    
    // Parse command line options.
    for (;;) {
        static struct option long_options[] = {
            {"list", no_argument, 0, 'l'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        int c = getopt_long (argc, argv, "l", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 'l':
                ListOnly = 1;
                break;
            default:
            case '?':
                print_usage(argv[0]);
                break;
        }
    }

    // Require exactly one input file name
    if (optind != argc - 1) {
        print_usage(argv[0]);
        return -1;
    }
    
    input_file_name = argv[optind];
    FilenameBase = remove_path_extension(input_file_name);
    
    // Open the input file.
    FILE * file = fopen(input_file_name, "rb");
    if (!file) {
        printf("Unable to open %s\n", input_file_name);
        return -1;
    }
    
    if (fseek(file, 0 , SEEK_END) != 0) {
        printf("Error reading %s\n", input_file_name);
        return -1;
    }

    long file_size = ftell(file);

    unsigned char * data = malloc(file_size);
    if (data == NULL) {
        printf("File too large to allocate work buffer");
        fclose(file);
        return -1;
    }
    
    if (fseek(file, 0 , SEEK_SET) != 0) {
        printf("Error reading %s\n", input_file_name);
        free(data);
        fclose(file);
        return -1;
    }
    
    unsigned long bytes_read = fread(data, 1, file_size, file);
    fclose(file);
    if (bytes_read != file_size) {
        printf("Error reading %s\n", input_file_name);
        free(data);
        return -1;
    }
        
    printf("\n%-10s%-10s%-10s%-10s%-10s\n", "BLOCK", "NAME", "LENGTH", "TYPE", "ERROR");
    printf("-----     ----      ------    ----      -----\n");
    
    // Set up the input parsing state machine and run the input through it.
    struct spherecas_state read_state;
    spherecas_begin_read(&read_state);
    spherecas_read_bytes(&read_state, data, (int)bytes_read);
    
    printf("\nDone. %d block(s) found.\n", CurrentBlockIndex);
    free(data);
    free(FilenameBase);
}

// This is the callback function for the data reader
void spherecas_block_read(struct spherecas_state * state ,
                         char block_name[],
                         uint8_t * data,
                         int length,
                         enum spherecas_blocktype type,
                         enum spherecas_error error)
{
    const char * type_str = (type == SPHERECAS_BLOCKTYPE_TEXT ? "Text" : "Obj");
    char * error_str = "";
    if (error == SPHERECAS_ERROR_TRAILER) {
        error_str = "Trailer";
    } else if (error == SPHERECAS_ERROR_CHECKSUM) {
        error_str = "Checksum";
    }
    printf("%-10d%c%c        %-10d%-10s%-10s\n", CurrentBlockIndex+1, block_name[0], block_name[1], length, type_str, error_str);

    if (!ListOnly) {
        char * output_name = malloc(strlen(FilenameBase) + 12);
        sprintf(output_name, "%s-%c%c_%d.bin", FilenameBase, block_name[0], block_name[1], CurrentBlockIndex + 1);
        FILE * outfile = fopen(output_name, "wb");
        if (!outfile) {
            printf("\tFailed to open output file for writing %s\n\n", output_name);
            return;
        }
        fwrite(data, 1, length, outfile);
        fclose(outfile);
        printf("\t--> Block written to file %s\n\n", output_name);
        free(output_name);
    }

    CurrentBlockIndex++;
}


// remove_path_extension is adapted from
//  https://stackoverflow.com/a/2736841/73297
// CC BY-SA 4.0
static char * remove_path_extension(const char * str)
{
    char *retstr, *lastext, *lastpath;

    if (str == NULL) return NULL;
    if ((retstr = malloc(strlen(str) + 1)) == NULL) return NULL;

    strcpy(retstr, str);
    lastext = strrchr(retstr, '.');
    lastpath = strrchr(retstr, '/');

    // If it has an extension separator.
    if (lastext != NULL) {
        // and it's to the right of the path separator.
        if (lastpath != NULL) {
            if (lastpath < lastext) {
                // then remove it.
                *lastext = '\0';
            }
        } else {
            // Has extension separator with no path separator.
            *lastext = '\0';
        }
    }

    return retstr;
}

