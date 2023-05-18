/* Previous version used slipstub.
   All USB protocol support has been removed.  Use the 3if */


#include <stdint.h>
#include "fixedpoint.h"
#include "base.h"

#include "gdbstub_api.h"
#include <string.h>

#include "pbuf.h"
#include "cbuf.h"
#include "run_3if.h"


/* CONFIGURATION */

#include "mod_synth.c"


/* COMMUNICATION */
struct app {
    struct cbuf out; uint8_t out_buf[128];
};

void handle_tag(struct app *app, uint16_t tag, const struct pbuf *p) {
    //infof("tag %d\n", tag);
    switch(tag) {
    case TAG_U32: {
        /* name ! {send_u32, [101, 1000000000, 1,2,3]}. */
        int rv = tag_u32_dispatch(
            synth_handle_tag_u32,
            NULL, //send_reply_tag_u32,
            NULL, p->buf, p->count);
        if (rv) { infof("tag_u32_dispatch returned %d\n", rv); }
        break;
    }
    default:
        infof("unknown tag 0x%x\n", tag);
    }
}



/* STARTUP, HOST */

#define LED GPIOC,13


struct app app_;

void start(void) {
    hw_app_init();
    /* FIXME: This assumes it's GPIOA */
    rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB | RCC_AFIO);

    /* App struct init */
    CBUF_INIT(app_.out);

    /* Turn on the LED to indicate we have started. */
    hw_gpio_config(LED,HW_GPIO_CONFIG_OUTPUT);
    hw_gpio_low(LED);

    /* Init the synth time stack. */
    synth_init(&app_.out);

}
void main_loop(gdbstub_fn_poll bl_poll_fn) {
    for(;;) {
        bl_poll_fn();
    }
}
void stop(void) {
    hw_app_stop();
    _service.reset();
}

#ifndef VERSION
#define VERSION "current"
#endif

const char config_manufacturer[] CONFIG_DATA_SECTION = "Zwizwa";
const char config_product[]      CONFIG_DATA_SECTION = "PDM CV Synth";
const char config_firmware[]     CONFIG_DATA_SECTION = FIRMWARE;
const char config_protocol[]     CONFIG_DATA_SECTION = "{driver,pdm,slip}";


extern uint8_t _firmware_endx;
extern struct info_buf_hdr info_buf;
struct gdbstub_control control CONTROL_SECTION;
struct gdbstub_config config CONFIG_HEADER_SECTION = {
    .manufacturer    = config_manufacturer,
    .product         = config_product,
    .firmware        = config_firmware,
    .version         = config_version,
    .protocol        = config_protocol,
    .start           = start,
    .stop            = stop,
    .switch_protocol = NULL,
    .flash_start     = (const void*)&config,
    .flash_endx      = (void*)&_firmware_endx,
    .control         = &control,
    .fwtag           = 0, // must be 0, used to recognize ecrypted firmware
    .info_buf        = &info_buf,
    .loop            = main_loop,
};

