#ifndef MOD_BL_BP
#define MOD_BL_BP



/* Bootloader for synth related work.  Similar to the CDC ACM 3if
   monitor, but replacing the virtual serial port as main multiplexed
   interface with a Midi port, using a sysex wrapper for the 3if
   protocol.  A new gdbstub_io pointer has been added to
   gdbstub_config to dispatch MIDI data. */

/* See CDC ACM example in bootloader.c
   The approach now is to use mod_*.c instead of lib.a */

#define GDBSTUB_BOOT1_START(x) ((x)==0)
#define MANUFACTURER "Zwizwa"
#define PRODUCT "MIDI Synth"

#define MEM_WRITE_PARTITIONS_START 0x08002800

const char config_version[] = "bl_midi_bp.c";

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

#include "gdbstub.h"
#include "gdbstub_api.h"


/* Config is stored in a separate Flash block and overwritten when we
   flash the application code.  To make the code more robust, the case
   of an empty (all 0xFF) flash block is handled. */
struct gdbstub_config _config_default __attribute__ ((section (".config_header"))) = {
    .bottom = 0x8002800  // allow config overwrite
};


/* Only one _service.add() poll routine is supported. */
void (*app_poll)(void);
void bl_poll_add(void (*poll)(void)) {
    app_poll = poll;
}
const struct gdbstub_service service SERVICE_SECTION = {
    .add = bl_poll_add,
};

void bl_poll() {
    usb_midi_poll();
    if (app_poll) app_poll();
}

#define USB_DP GPIOA,12

int main(void) {
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

#if USB_PULLUP_B5
    /* For modded board: 1k5 between A12 and B5 with the original R10
       pullup removed.  We set B5 high here to assert the pullup and
       signal the host we are a full speed device.  This does two
       things: on reset the pin will be de-asserted, signalling the
       host we are disconnected.  Additionally it places the pullup
       under program control. */
    rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB | RCC_AFIO);
    hw_gpio_high(GPIOB,5);
    hw_gpio_config(GPIOB,5,HW_GPIO_CONFIG_OUTPUT);
#else
    /* Non-modded board: use the USB reset hack from
       http://amitesh-singh.github.io/stm32/2017/05/27/Overcoming-wrong-pullup-in-blue-pill.html
       Note that it is important not to enable RCC_AFIO yet */
    rcc_periph_clock_enable(RCC_GPIOA);
    hw_gpio_low(USB_DP);
    hw_gpio_config(USB_DP, HW_GPIO_CONFIG_OUTPUT_2MHZ);
    hw_busywait_us(5000);
    rcc_periph_clock_enable(RCC_GPIOB | RCC_AFIO);
#endif


    usb_midi_init();
    monitor_init();

    /* When BOOT0==0 (boot from flash), BOOT1 is ignored by the STM
       boot ROM, so we can use it as an application start toggle. */
    /* FIXME: Find out why this is not 100% reliable. */
    uint32_t boot1 = hw_gpio_read(GPIOB,2);
    if (GDBSTUB_BOOT1_START(boot1) && !flash_null(_config.start)) {
        bl_ensure_started(&bl_state);
    }

    /* Note that running without _config.loop does not seem to work
       any more.  Application will take over the main loop.  It is
       passed the bootloader poll routine.  The poll_app() mechanism
       is not supported. */
    for (;;) {
        bl_poll();
    }
    return 0;
}


#endif

