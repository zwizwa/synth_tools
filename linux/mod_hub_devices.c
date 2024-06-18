#ifndef MOD_HUB_CONFIG
#define MOD_HUB_CONFIG

/* Connectivity: all midi devices will be connected to the hub.
   Filtering can be done in C code.  For outgoing we need to see what
   works best.  The output connection would only be for broadcast
   messages.  Might be useful for management sysex, e.g. addressing a
   number of usb uc boards.

   Code goes into a separate module to make some tests.

*/

/* Note that in the C code we represent midi channel 1-16 as 0-15,
   following the protocol encoding, which makes it easier to work
   with. */

/* Device names. The ids are allocated sequentially. */
#define FOR_DEV(m) \
    /* dev_name, ALSA client name */ \
    m(axiom25, "Axiom 25") \
    m(synth,   "synth") \
    m(pd_io,   "pd_io") \
    m(pixi,    "EuroPIXI") \
    m(fire,    "FL STUDIO FIRE") \

/* Selector names. The ids are allocated sequentially.  The list needs
   to be sorted in DPC order. */
#define FOR_SEL(m) \
    /* sel_name, dev_name, port, channel */ \
    m(axiom25_0_0,  axiom25, 0,  0) \
    m(axiom25_0_15, axiom25, 0, 15) \
    m(axiom25_1_0,  axiom25, 1,  0) \
    m(axiom25_2_0,  axiom25, 2,  0) \
    m(synth,        synth,   0,  0) \
    m(pd_io,        pd_io,   0,  0) \
    m(pixi,         pixi,    0,  0) \
    m(fire,         fire,    0,  0) \

/* Routing information is needed for input and output:
   - MIDI in   client:port:channel -> selector
   - MIDI out: selector -> client:port:channel

   The dev <-> client map is run-time, based on ALSA client numbers.
   All the rest can just go in static types and const data.
*/


#include "macros.h"
#include <stdint.h>

/* dev_name -> dev_id */
#define DEV_ENUM(name, str) dev_##name,
enum dev { FOR_DEV(DEV_ENUM) };

/* sel_name -> sel_id */
#define SEL_ENUM(name, d, p, c) sel_##name,
enum sel { FOR_SEL(SEL_ENUM) };



/* dev_id,port,chan -> sel */
#define DPC(D,P,C) (((((D) * 16) + (P)) * 16) + (C))
#define DPT_TO_SEL(sel_name,dev_name,port,channel) DPC(dev_##dev_name,port,channel),
const uint16_t dpc_table[] = { FOR_SEL(DPT_TO_SEL) };
#define NB_SEL (ARRAY_SIZE(dpc_table))

/* Instantiate the bisect module. */
#define NS(name) dpc##name
typedef void dpc_t; // dummy, use NULL
static inline uintptr_t dpc_size(dpc_t *r) {
    return ARRAY_SIZE(dpc_table);
}
static inline uintptr_t dpc_rank(dpc_t *r, uintptr_t index) {
    return dpc_table[index];
}
#include "ns_bisect.h"
#undef NS
static inline intptr_t dpc_to_sel(uint8_t dev_id, uint8_t port, uint8_t chan) {
    return dpc_find(NULL, DPC(dev_id, port, chan));
}


static inline uint8_t dpc_to_dev(uint16_t dpc) { return dpc >> 8; }
static inline uint8_t dpc_to_port(uint16_t dpc) { return (dpc >> 4) & 0xF; }
static inline uint8_t dpc_to_chan(uint16_t dpc) { return dpc & 0xF; }

#define DEV_NAMES(name,str) str,
const char *dev_names[] = {
    FOR_DEV(DEV_NAMES)
};
#define NB_DEV ARRAY_SIZE(dev_names)




/* Routing information is needed for input and output:
   - MIDI in   client:port:channel -> selector
   - MIDI out: selector -> client:port:channel
*/

/* The port:channel information is known at compile time.  The client
   selector is not, so we make a routing table indexed by client
   number.  ALSA only has 8 bit address space for this so that can be
   a flat table.
*/


#endif
