#include "gdbstub_api.h"
#include "hw_stm32f103.h"
#include "cbuf.h"
#include "pbuf.h"

/* APP */
struct app {
    void *next;
    struct cbuf console_out; uint8_t console_out_buf[128];
    struct pbuf console_in;  uint8_t console_in_buf[128];
    struct {
        uint32_t echo:1;
    } config;
};

/* Protothread: app_put() is inverted to APP_GET() suspend. */
#define APP_GET_(app,label)                               \
    do {                                                  \
	(app)->next = &&label;                            \
	return; /* suspend machine */                     \
      label:{}                                            \
    } while(0)
#define APP_GET(s)                              \
    APP_GET_(s,GENSYM(label_))

void app_out(struct app *app, uint8_t out) {
    cbuf_write(&app->console_out, &out, 1);
}
void app_word(struct app *app) {
    if (app->next) { goto *app->next; }
    for (;;) {
        APP_GET(app);
        struct pbuf *p = &app->console_in;
        for(uint32_t i=0; i<p->count; i++) {
            app_out(app, p->buf[i]);
        }
        app_out(app, '\n');
        app_out(app, '\r');
    }
}

static void app_write(struct app *app, const uint8_t *buf, uint32_t len) {
    for (uint32_t i=0; i<len; i++) {
        uint8_t in = buf[i];
        if (in == 127) {
            if (app->console_in.count > 0) {
                const uint8_t backspace[] = {8,' ',8};
                cbuf_write(&app->console_out, backspace, sizeof(backspace));
                app->console_in.count--;
            }
        }
        else {
            app_out(app, in);
            if (in == '\r') {
                app_out(app, '\n');
                app_word(app);
                pbuf_clear(&app->console_in);
            }
            else {
                pbuf_write(&app->console_in, &in, 1);
            }
        }
    }
}
static uint32_t app_read(struct app *app, uint8_t *buf, uint32_t room) {
    uint32_t n = cbuf_elements(&app->console_out);
    if (n > room) n = room;
    cbuf_read(&app->console_out, buf, room);
    return n;
}
void app_init(struct app *app) {
    CBUF_INIT(app->console_out);
    PBUF_INIT(app->console_in);
    app->config.echo = 1;
    app_word(app); // get sequencing started
}





/* GLUE */
struct app app_;
static uint32_t app_read_(uint8_t *buf, uint32_t room) {
    return app_read(&app_, buf, room);
}
static void app_write_(const uint8_t *buf, uint32_t len) {
    app_write(&app_, buf, len);
}
const struct gdbstub_io app_io = {
    .read  = app_read_,
    .write = app_write_,
};
void slipstub_switch_protocol(const uint8_t *buf, uint32_t size) {
    *_service.io = (struct gdbstub_io *)(&app_io);
    (*_service.io)->write(buf, size);
}
#define LED GPIOC,13
void start() {
    hw_app_init();
    rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB | RCC_AFIO);
    hw_gpio_config(LED,HW_GPIO_CONFIG_OUTPUT);
    hw_gpio_low(LED);
    app_init(&app_);
}
void main_loop(gdbstub_fn_poll bl_poll_fn) {
    for(;;) {
        bl_poll_fn();
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
const char config_product[]      CONFIG_DATA_SECTION = "Console test";
const char config_firmware[]     CONFIG_DATA_SECTION = FIRMWARE;
const char config_protocol[]     CONFIG_DATA_SECTION = "{driver,pdm,line}";
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
    .switch_protocol = slipstub_switch_protocol,
    .flash_start     = (const void*)&config,
    .flash_endx      = (void*)&_firmware_endx,
    .control         = &control,
    .fwtag           = 0, // must be 0, used to recognize ecrypted firmware
    .info_buf        = &info_buf,
    //.loop            = main_loop,
};

