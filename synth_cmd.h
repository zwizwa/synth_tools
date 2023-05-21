#ifndef SYNTH_CMD_H
#define SYNTH_CMD_H


/* This macro is essentially the symbol table of interpreter
   extensions. It is used in firmware to create the index, and in
   tether app to retreive the addresses.  Much more lightweight than
   ELF. */
#define for_synth_cmd(m) \
    m(0, info) \

#endif

