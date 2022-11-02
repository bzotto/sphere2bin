//
//  spherecas.c
//
//  Copyright (c) Ben Zotto 2022
//  See LICENSE for licensing information.
//

#include "spherecas.h"

#define HEADER_SYNC     0x16
#define HEADER_ESC      0x1B
#define HEADER_ETB      0x17

enum spherecas_read_state {
    READ_SYNC = 0,
    READ_HEADER_START,
    READ_DATA_LENGTH_HIGH,
    READ_DATA_LENGTH_LOW,
    READ_BLOCK_NAME_1,
    READ_BLOCK_NAME_2,
    READ_DATA,
    READ_ETB,
    READ_CHECKSUM
};

void spherecas_begin_read(struct spherecas_state * state)
{
    state->read_state = READ_SYNC;
    state->data_count_expected = 0;
    state->data_count_read = 0;
    state->checksum = 0;
    state->block_type = SPHERECAS_BLOCKTYPE_TEXT;
}

void spherecas_read_byte(struct spherecas_state * state, uint8_t byte)
{
    switch (state->read_state) {
        case READ_SYNC:
        {
            if (byte == HEADER_SYNC) {
                state->read_state = READ_HEADER_START;
            }
            break;
        }
        case READ_HEADER_START:
        {
            if (byte == HEADER_ESC) {
                state->read_state = READ_DATA_LENGTH_HIGH;
            } else if (byte == HEADER_SYNC) {
                // Do nothing, remain in this state.
                ;
            } else {
                // Resync
                state->read_state = READ_SYNC;
            }
            break;
        }
        case READ_DATA_LENGTH_HIGH:
        {
            state->data_count_expected = (byte << 8);
            state->read_state++;
            break;
        }
        case READ_DATA_LENGTH_LOW:
        {
            state->data_count_expected |= byte;
            state->data_count_expected++;       // Actual block data count is length+1
            state->read_state++;
            break;
        }
        case READ_BLOCK_NAME_1:
        {
            state->block_name[0] = byte;
            state->read_state++;
            break;
        }
        case READ_BLOCK_NAME_2:
        {
            state->block_name[1] = byte;
            state->read_state++;
            break;
        }
        case READ_DATA:
        {
            state->data[state->data_count_read++] = byte;
            state->checksum += byte;
            if (state->data_count_read == state->data_count_expected) {
                state->read_state++;
            }
            // If any values in the block have their high bit set, the block
            // is likely to contain object code. If all bytes are 7-bit ASCII
            // then the block is likely to be text or source (the default).
            // This is the heuristic used by Programma's Tape Directory program.
            if (byte & 0x80) {
                state->block_type = SPHERECAS_BLOCKTYPE_OBJECT;
            }
            break;
        }
        case READ_ETB:
        {
            if (byte == HEADER_ETB) {
                state->read_state = READ_CHECKSUM;
            } else {
                // Report out an error with this block.
                spherecas_block_read(state, state->block_name, state->data, state->data_count_read, state->block_type, SPHERECAS_ERROR_TRAILER);
                // Go back to sync here.
                spherecas_begin_read(state);
            }
            break;
        }
        case READ_CHECKSUM:
        {
            enum spherecas_error error = SPHERECAS_ERROR_NONE;
            if (byte != state->checksum) {
                error = SPHERECAS_ERROR_CHECKSUM;
            }
            spherecas_block_read(state, state->block_name, state->data, state->data_count_read, state->block_type, error);
            spherecas_begin_read(state);
            break;
        }
    }
}

void spherecas_read_bytes(struct spherecas_state * restrict state,
                          const uint8_t * restrict data,
                          int count)
{
    while (count > 0) {
        spherecas_read_byte(state, *data++);
        --count;
    }
}
