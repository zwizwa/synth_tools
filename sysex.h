/* Encoding/decoding routines for sysex. */

#ifndef SYSEX_H
#define SYSEX_H

#include "slice.h"

/* Save a single chunk of up to 7 bytes in a space spanning 4 midi
   messages.  Basic ideas:

   - This is for encoding 8-bit streams that are self-delimited, so
     the sysex chunking can be arbitrary.

   - Only one 7 to 8 encoding per sysex message to keep it simple.

   It's relatively inefficient (7 bytes of payload per 16 bytes of USB
   data) but that is probably ok considering this is USB.

*/

/* Encode up to 7 bytes.  The out buffer has room for 8 bytes. */
static inline uintptr_t sysex_encode_8bit_to_7bit(uint8_t *out, const_slice_uint8_t *in) {
    uint32_t n = (in->len > 7) ? 7 : in->len;

    uint8_t msbs = 0;
    uint8_t *lsbs = &out[1];

    for (uint32_t i=0; i<n; i++) {
        uint8_t byte = in->buf[i];
        if (byte & 0x80) { msbs |= (1 << i); }
        lsbs[i] = byte & 0x7f;
    }

    out[0] = msbs;
    skip_const_slice_uint8_t(in, n);
    return n+1;
}



#endif
