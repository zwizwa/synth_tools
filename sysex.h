/* Encoding/decoding routines for sysex. */

#ifndef SYSEX_H
#define SYSEX_H

#include "slice.h"
#include "cbuf.h"

/* Save a single chunk of up to 7 bytes in a space spanning 4 midi
   messages.  Basic ideas:

   - This is for encoding 8-bit streams that are self-delimited, so
     the sysex chunking can be arbitrary.

   - Only one 7 to 8 encoding per sysex message to keep it simple.

   It's relatively inefficient (7 bytes of payload per 16 bytes of USB
   data) but that is probably ok considering this is USB.

*/

/* Encode up to 7 bytes.  The out buffer has enough room to contain
   the encoded data.  */
static inline uint32_t sysex_encode_8bit_to_7bit(uint8_t *out, const_slice_uint8_t *in) {
    uint32_t offset = 0;

    while(in->len > 0) {
        uint32_t n = (in->len > 7) ? 7 : in->len;

        uint8_t msbs = 0;
        uint8_t *lsbs = &out[offset + 1];

        for (uint32_t i=0; i<n; i++) {
            uint8_t byte = in->buf[i];
            if (byte & 0x80) { msbs |= (1 << i); }
            lsbs[i] = byte & 0x7f;
        }

        out[offset] = msbs;
        skip_const_slice_uint8_t(in, n);
        offset += n + 1;
    }
    return offset;
}

/* Number of bytes needed for encoding. */
static inline uint32_t sysex_encode_8bit_to_7bit_needed(uint32_t nb_bytes) {
    uint32_t div = nb_bytes / 7;
    uint32_t rem = nb_bytes % 7;
    if (rem == 0) {
        return div * 8;
    }
    else {
        return div * 8 + 1 + rem;
    }
}
/* Number of payload bytes that can be accomodated in nb_sysex_bytes */
static inline uint32_t sysex_encode_8bit_to_7bit_payload_available(uint32_t nb_sysex_bytes) {
    uint32_t div = nb_sysex_bytes / 8;
    uint32_t rem = nb_sysex_bytes % 8;
    if (rem > 0) {
        return div * 7 + rem - 1;
    }
    else {
        return div * 7;
    }
}

static inline void sysex_to_ump(slice_uint8_t *out, const_slice_uint8_t *sysex) {
    while (sysex->len > 0) {
        uint32_t chunk_size = sysex->len > 3 ? 3 : sysex->len;
        LOG("chunk_size = %d, sysex->len=%d\n", chunk_size, sysex->len);
        const uint8_t chunk_size_to_group[] = { -1, 0x5, 0x6, 0x4 };
        uint8_t chunk[4] = { chunk_size_to_group[chunk_size], 0, 0, 0 };
        memcpy(chunk + 1, sysex->buf, chunk_size);
        skip_const_slice_uint8_t(sysex, chunk_size);
        write_slice_uint8_t(out, chunk, 4);
    }
}


/* This is a royal pain to express.  Since data size is small, do it
   in a couple of passes using C stack buffers. */

void sysex_stream_from_cbuf(uint8_t tag, slice_uint8_t *out, struct cbuf *in) {
    uint32_t nb_el = cbuf_elements(in);
    if (nb_el == 0) {
        //LOG("no input data\n");
        return;
    }

    /* Number of available slots. */
    uint32_t nb_slots = out->len / 4;
    if (nb_slots < 2) {
        //LOG("no room for payload\n");
        return;
    }

    /* Total number of sysex bytes that can fit in nb_slots */
    uint32_t nb_sysex_available = nb_slots * 3;

    /* Number of 8bit payload bytes that can fit in the net avialable
       sysex payload */
    uint32_t nb_8bit_payload_available =
        sysex_encode_8bit_to_7bit_payload_available(
            nb_sysex_available
            - 3 /* framing */);
    //LOG("nb_8bit_payload_available = %d\n", nb_8bit_payload_available);


    /* Get 8 to 7 bit encoded payload. */
    uint8_t payload[nb_8bit_payload_available];
    uint32_t nb_payload_bytes = cbuf_read(in, payload, sizeof(payload));
    // LOG("nb_payload_bytes = %d\n", nb_payload_bytes);
    const_slice_uint8_t payload_slice = { .buf = payload, .len = nb_payload_bytes };
    uint8_t sysex[nb_sysex_available];
    uint32_t nb_sysex_data = sysex_encode_8bit_to_7bit(sysex + 2, &payload_slice);
    // LOG("nb_sysex_data = %d\n", nb_sysex_data);

    /* Add sysex framing */
    sysex[0] = 0xF0;
    sysex[1] = tag;
    sysex[2 + nb_sysex_data] = 0xF7;

    /* Chunk it out. */
    const_slice_uint8_t sysex_slice = { .buf = sysex, .len = nb_sysex_data + 3 };
    sysex_to_ump(out, &sysex_slice);

}

#endif
