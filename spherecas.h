//
//  spherecas.h
//
//  Copyright (c) Ben Zotto 2022
//  See LICENSE for licensing information.
//
//  This module is a small library for parsing cassette data recorded for/by the
//  Sphere 1 and other Sphere microcomputers. Create a state object,
//  then call begin_read to set it up, then call read_byte(s) until all of the
//  input data is exhausted. The library will invoke your callback function
//  at the completion of every block in the cassette data.
//
//  Format details:
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

#ifndef SPHERECAS_H
#define SPHERECAS_H

#include <stdio.h>
#include <stdint.h>

enum spherecas_error {
    SPHERECAS_ERROR_NONE,
    SPHERECAS_ERROR_TRAILER = 1,
    SPHERECAS_ERROR_CHECKSUM
};

struct spherecas_state {
    int       read_state;
    char      block_name[2];
    uint16_t  data_count_expected;
    int       data_count_read;
    uint8_t   data[0x10000];  
    uint8_t   checksum;
    void *    context;
};

// Begin reading
void spherecas_begin_read(struct spherecas_state * state);

// Read a single character
void spherecas_read_byte(struct spherecas_state * state, uint8_t byte);

// Read `count` characters from `data`
void spherecas_read_bytes(struct spherecas_state * restrict state,
                          const uint8_t * restrict data,
                          int count);

// Callback - this must be provided by the user of the library.
// The arguments are as follows:
//      state           - Pointer to the spherecas_state structure
//      block_name      - Two characters indicating the "name" of this block
//      data            - Pointer to the start of the data payload
//      length          - Length of data payload
//      error           - Success (0) or an error code
//
// Note that while the interpreted record is passed entirely as arguments,
// the raw data is available in the `spherecas_state` structure, which includes the
// address and checksum as part of data.
extern void spherecas_data_read(struct spherecas_state * state ,
                                char block_name[],
                                uint8_t *data,
                                int length,
                                enum spherecas_error error);

#endif /* SPHERECAS_H */
