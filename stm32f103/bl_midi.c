/* Bootloader for synth related work.  Same approach as the CDC ACM
   3if monitor, but replacing the virtual serial port as main
   multiplexed interface with a Midi port, using a sysex wrapper for
   the 3if protocol. */

/* See CDC ACM example in bootloader.c
   The approach now is to use mod_*.c instead of lib.a */

#define PARTITION_CONFIG_OVERLAP_PARTITION_INIT(addr, size) \
  { .max_size = size, .page_logsize = 10, .config = (void*)(addr) }
#define PARTITION_CONFIG_INIT {                                         \
        PARTITION_CONFIG_OVERLAP_PARTITION_INIT(0x08004000, 0x1C000),   \
        PARTITION_CONFIG_OVERLAP_PARTITION_INIT(0x08012000, 0x0E000),   \
}
#define GDBSTUB_MEMORY_MAP GDBSTUB_MEMORY_MAP_STM32F103CB
#define GDBSTUB_BOOT1_START(x) ((x)==0)
#define GDBSTUB_RSP_ENABLED 0
#define MONITOR_ENABLED 1
#define TRAMPOLINE_IN_BOOTLOADER
#define MANUFACTURER "Zwizwa"
#define PRODUCT "MIDI Synth"
#include "mod_trampoline.c"
#define CONFIG_DEFAULT 1
const char config_version[] = "bl_midi.c";

#if defined(STM32F1)
#include "hw_stm32f103.h"
#elif defined(STM32F4)
#include "hw_stm32f407.h"
#else
#error UNKNOWN_HW
#endif

/* libopencm USB descriptor */
#include "mod_midi_desc.c"

/* Protocol code is separate, so it can be stubbed out and tested on host. */
#include "mod_bl_midi.c"


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

#ifndef GPIOB5_HIGH
#define GPIOB5_HIGH 1
#endif


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


