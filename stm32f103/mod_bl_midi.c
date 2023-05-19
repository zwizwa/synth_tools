/* Bootloader for synth related work.  Same approach as the CDC ACM
   3if monitor, but replacing the virtual serial port as main
   multiplexed interface with a Midi port, using a sysex wrapper for
   the 3if protocol. */

#ifndef GDBSTUB_BOOT1_START
#error NEED GDBSTUB_BOOT1_START
#endif

#include "gdbstub.h"
#include "gdbstub_api.h"

const char gdbstub_memory_map[] = GDBSTUB_MEMORY_MAP_STM32F103C8;
const uint32_t flash_page_size_log = 10; // 1k

/* Config is stored in a separate Flash block and overwritten when we
   flash the application code.  To make the code more robust, the case
   of an empty (all 0xFF) flash block is handled. */
#ifndef CONFIG_DEFAULT
struct gdbstub_config _config_default __attribute__ ((section (".config_header"))) = {
    .bottom = 0x8002800  // allow config overwrite
};
#endif

void ensure_started(struct gdbstub_ctrl *stub_ctrl);

#include "mod_monitor.c"
// BOOTLOADER_SERVICE(monitor_read, monitor_write, NULL)

// FIXME: Put the Midi descriptor in a different mod

/* See CDC ACM example in bootloader.c
   The approach now is to use mod_*.c instead of lib.a */

#if defined(STM32F1)
#include "hw_stm32f103.h"
#elif defined(STM32F4)
#include "hw_stm32f407.h"
#else
#error UNKNOWN_HW
#endif

#include "mod_midi_desc.c"


#ifndef GPIOB5_HIGH
#define GPIOB5_HIGH 1
#endif

struct bl_state {
    void *next;
    struct {
        uint32_t nb_rx;
        uint32_t nb_rx_3if;
        uint32_t nb_rx_sysex;
    } stats;
    /* Decoder buffer for sysex.  The encoding used is: first byte
       contains high bits of subsequent 7 bytes (LSB is bit of first
       byte), then followed by up to 7 bytes containing the low 7
       bits.  That's simplest to decode. */
    uint8_t sysex_high_bits;
    uint8_t sysex_count;

};
struct bl_state bl_state;


uint32_t __attribute__((noinline)) usb_midi_read(uint8_t *buf, uint32_t room) {
    return 0;
}


/* Here we need to distinguish between bootloader and app data, so
   parser goes here and should be as generic as possible. */
const uint8_t message_type_to_size[] = {
    /* Table 3-1 Packet Sizes based on Message Types
       The length is in multiples of 32 bit. */

    [0x0] = 1,  // Utility Messages
    [0x1] = 1,  // System Real Time and System Common Messdages (except System Exclusive)
    [0x2] = 1,  // MIDI 1.0 Channel Voice Messages
    [0x3] = 2,  // Data Messages (including System Exclusive)
    [0x4] = 2,  // MIDI 2.0 Channel Voice Messages
    [0x5] = 3,  // Data Messages

    /* All the rest is Reserved */
    [0x6] = 1,
    [0x7] = 1,
    [0x8] = 2,
    [0x9] = 2,
    [0xa] = 1,
    [0xb] = 3,
    [0xc] = 3,
    [0xd] = 4,
    [0xe] = 4,
    [0xf] = 4,
};

/* How to map this?  I will probably want to have both UART midi and
   this USB midi.  So not sure how to split it up.  What I want:

   - The other end should receive complete messaages

   - Sysex should use a stream interface.  Conceptually this is a
     separate channel.

   Note that USB MIDI uses Universal MIDI Packet format (UMP), which
   is defined elsewhere.

*/

/* MIDI 1.0 was simple. MIDI 2.0 is not. I don't feel like reading all
   the documentation, so I just test by writing to /dev/midiX and see
   what arrives here.  The "old school" approach of treating midi as 2
   different streams (low-priority sysex and everything else
   pre-empting that) seems to be ok, so that is what we provide to the
   app. */

/* The first byte after 0xF0 is a dispatch code.  I'm going to use
   0x12 which seems to be a defunct company. Change it later if
   needed.

   https://www.midi.org/specifications/midi-reference-tables/manufacturer-sysex-id-numbers
*/

#define BL_MIDI_SYSEX_MANUFACTURER 0x12

void bl_3if_push(struct bl_state *s, uint8_t byte) {
    s->stats.nb_rx_3if++;
}
void bl_app_push(struct bl_state *s, uint8_t byte) {
}

#define BL_MIDI_SYSEX_NEXT_LABEL(s,label) {              \
        (s)->next = &&label; return; label:{}            \
    }
#define BL_MIDI_SYSEX_NEXT(s)                   \
    BL_MIDI_SYSEX_NEXT_LABEL(s,GENSYM(label_))


void bl_midi_sysex_push(struct bl_state *s, uint8_t byte) {
    if (byte == 0xF0) goto packet_start;
    if (unlikely(!s->next)) return;
    goto *s->next;

  packet:
    BL_MIDI_SYSEX_NEXT(s);
    if (byte != 0xF0) {
        /* Ignore everything up to the sysex start byte. */
        goto packet;
    }
  packet_start:
    /* byte == 0x0F0 sysex start */
    /* First byte after start byte is manufacturer. */
    BL_MIDI_SYSEX_NEXT(s);
    if (byte == BL_MIDI_SYSEX_MANUFACTURER) {
        /* 3IF is addressed.  Perform conversion to 8-bit data and
           push it into the interpreter. */
        for (;;) {
            BL_MIDI_SYSEX_NEXT(s);
            if (byte == 0xF7) goto packet;
            s->sysex_high_bits = byte;
            for (s->sysex_count = 0;
                 s->sysex_count < 7;
                 s->sysex_count++) {
                BL_MIDI_SYSEX_NEXT(s);
                if (byte == 0xF7) goto packet;
                if (s->sysex_high_bits & (1 << s->sysex_count)) {
                    byte |= 0x80;
                }
                bl_3if_push(s, byte);
            }
        }
    }
    /* Everything else goes to the application. */
    bl_app_push(s, 0xF0);
    for(;;) {
        bl_app_push(s, byte);
        if (byte == 0xF7) goto packet;
        BL_MIDI_SYSEX_NEXT(s);
    }
}


void bl_midi_write_sysex(struct bl_state *s, const uint8_t *buf, uint32_t len) {
    for (uint32_t i=0; i< len; i++) {
        bl_midi_sysex_push(s, buf[i]);
    }
}

void bl_midi_write(struct bl_state *s, const uint8_t *buf, uint32_t len) {
    s->stats.nb_rx += len;
    while(len > 0) {
        /* See UMP spec appendix G, All defined messages. */
        uint8_t msg0 = buf[0];
        uint8_t message_type = (msg0 >> 4) & 0xF;
        uint8_t group = msg0 & 0xF;
        uint8_t message_size = 4 * message_type_to_size[message_type];
        switch(group) {
        case 0x4: bl_midi_write_sysex(s, &buf[1], 3); break;
        case 0x5: bl_midi_write_sysex(s, &buf[1], 1); break;
        case 0x6: bl_midi_write_sysex(s, &buf[1], 2); break;
        case 0x8: // Note off
        case 0x9: // Note on
            break;
        }
        buf += message_size;
        len -= message_size;
    }
}
void __attribute__((noinline)) usb_midi_write(const uint8_t *buf, uint32_t len) {
    bl_midi_write(&bl_state, buf, len);
}


int main(void) {
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    rcc_periph_clock_enable(RCC_GPIOC);

    /* For modded board: 1k5 between A12 and B5 with the original R10
       pullup removed.  We set B5 high here to assert the pullup and
       signal the host we are a full speed device.  This does two
       things: on reset the pin will be de-asserted, signalling the
       host we are disconnected.  Additionally it places the pullup
       under program control. */
    rcc_periph_clock_enable(RCC_GPIOB | RCC_AFIO);
#if GPIOB5_HIGH
    hw_gpio_high(GPIOB,5);
    hw_gpio_config(GPIOB,5,HW_GPIO_CONFIG_OUTPUT);
#endif

    usb_midi_init();
    monitor_init();

    /* When BOOT0==0 (boot from flash), BOOT1 is ignored by the STM
       boot ROM, so we can use it as an application start toggle. */
    /* FIXME: Find out why this is not 100% reliable. */
    uint32_t boot1 = hw_gpio_read(GPIOB,2);
    if (GDBSTUB_BOOT1_START(boot1) && !flash_null(_config.start)) {
        ensure_started(&bootloader_stub_ctrl);
    }

    /* Note that running without _config.loop does not seem to work
       any more.  Application will take over the main loop.  It is
       passed the bootloader poll routine.  The poll_app() mechanism
       is not supported. */
    for (;;) {
        if ((bootloader_stub_ctrl.flags & GDBSTUB_FLAG_STARTED) && (_config.loop)) {
            bootloader_stub_ctrl.flags |= GDBSTUB_FLAG_LOOP;
            _config.loop(&usb_midi_poll);
            /* If .loop() is implemented correctly this should not
               return.  However, in case it does, we fall through to
               bootloader_poll() below. */
        }
        /* By default, poll USB in the main loop. */
        usb_midi_poll();
    }
    return 0;
}
