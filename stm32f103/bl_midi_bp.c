/* Config stub for mod_bl_bp.c */

/* This needs to be configured properly, e.g. board with pullup
   configured without will not reset USB.
   1: Board has the B5 -> A12 (USB+) pullup.  Use that to reset USB.
   0: No such pullup.  Use the A12 gpio hack to reset USB. */
#define USB_PULLUP_B5 1

#include "mod_bl_midi_bp.c"
