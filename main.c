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
static int CurrentBlockNumber = 0;

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
    
    // Set up the input parsing state machine and run the input through it.
    struct spherecas_state read_state;
    spherecas_begin_read(&read_state);
    spherecas_read_bytes(&read_state, data, (int)bytes_read);
    
    if (CurrentBlockNumber == 0) {
        printf("Done. No valid blocks found.\n");
    } else {
        printf("Done. %d block(s) examined.\n", CurrentBlockNumber);
    }
    
    free(data);
}

// This is the callback function for the data reader
void spherecas_data_read(struct spherecas_state * state ,
                        char block_name[],
                        uint8_t *data,
                        int length,
                        enum spherecas_error error)
{
    printf("Saw block %c%c (%d bytes)\n", block_name[0], block_name[1], length);
    if (error == SPHERECAS_ERROR_TRAILER) {
        printf("** Trailer error\n");
    } else if (error == SPHERECAS_ERROR_CHECKSUM) {
        printf("** Checksum error\n");
    }

    CurrentBlockNumber++;

    if (ListOnly) {
        return;
    }

    char outputname[4 + 1 + 5 + 1 + 2 + 1 + 3];  // 0_block_NA.bin
    sprintf(outputname, "%d_block_%c%c.bin", CurrentBlockNumber-1, block_name[0], block_name[1]);
    FILE * outfile = fopen(outputname, "wb");
    if (!outfile) {
        printf("Failed to open output file for writing %s\n", outputname);
        return;
    }
    fwrite(data, 1, length, outfile);
    fclose(outfile);
    printf("--> Block written out to file %s\n", outputname);
}
