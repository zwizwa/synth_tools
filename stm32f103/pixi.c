/* Previous version used slipstub.
   All USB protocol support has been removed.  Use the 3if */

// TODO
// - SPI mode
// - Reg init
// - Sawtooth test signal
// - Test on target

#include <stdint.h>
#include "fixedpoint.h"
#include "base.h"

#include "gdbstub_api.h"
#include <string.h>

#include "pbuf.h"
#include "cbuf.h"
#include "cmd_3if.h"

#include "pixi_v1_max11300.h"

/* STATE */
#define NB_DAC 12
#define NB_ADC 6
struct app {
    void *next;
    uint16_t dac_vals[NB_DAC];
    uint16_t adc_vals[NB_ADC];
    uint16_t devid;
    struct cbuf out; uint8_t out_buf[128];
};
struct app app_;




/* CONFIGURATION */

/* Some design considerations.

   - There doesn't seem to be a straightforward way to sync to the
     PIXI's DAC update clock, so just use a timer to feed it at a
     constant rate and live with the jitter that that produces.  One
     way to work around this is to use one of the DAC ports as a
     digital signal and have that feed back to an STM edge detection
     interrupt.  Doesn't seem to be worth the hassle.  See git history
     before this message for original DMA code.

   - The DAC cycle is (* 40 12) 480 uS, or (/ 1.0 0.480) 2.083 kHz.
     Use a 2kHz timer to drive the DAC.

   - PIXI clock MAX is 20MHz.  DIV_4 is 18MHz

   - The packets are short, the 18MHz SPI rate is relatively high, so
     don't bother with DMA, just busy-wait on the SPI inside the
     interrupt.

*/

const struct hw_spi_nodma hw_spi2_nodma_master_0rw = {
//  rcc_gpio   rcc_spi   spi   rst       gpio   out in  sck  mode        div
// --------------------------------------------------------------------------------------------------
    RCC_GPIOB, RCC_SPI2, SPI2, RST_SPI2, GPIOB, 15, 14, 13,  HW_SPI_0RW, SPI_CR1_BAUDRATE_FPCLK_DIV_2
};

#define C_SPI hw_spi2_nodma_master_0rw

#include "cmd_3if.h"

void cmd_3if_info(struct cmd_3if *s) {
    uint8_t buf[256] = {};
    uint8_t nb = info_read(buf+2, sizeof(buf)-2);
    buf[0] = nb + 1;
    /* Empty replies are always discarded, so we send one extra byte
       which later could be a status code. */
    buf[1] = 0; // FIXME: more avalaible = 1
    cbuf_write(s->out, buf, nb + 2);
}

#include "synth_cmd.h"
#define CMD_ARRAY_ENTRY(id,name) [id] = cmd_3if_##name,
const cmd_3if cmd_3if_list[] = { for_synth_cmd(CMD_ARRAY_ENTRY) };

/* Timebase is derived from PWM signal. */
#define PWM_HZ 2000
#define PWM_DIV (72000000 / PWM_HZ)

/* GPIOs corresponding to output compare channels. */
#define TIM3_GPIOS                                  \
    {  RCC_GPIOA, GPIOA, 6  }, /* TIM_OC1 */        \
    {/*RCC_GPIOA, GPIOA, 7*/}, /* TIM_OC2 */        \
    {/*RCC_GPIOB, GPIOB, 0*/}, /* TIM_OC3 */        \
    {/*RCC_GPIOB, GPIOB, 1*/}  /* TIM_OC4 */

/* Cloned from PDM firmware.  On PIXI circuit, A6 and A7 are exposed
   on a header.  TIM_OC3 = B0 is connected to PIXI INT so cannot be
   used, and TOM_OC4 = B1 is not exposed.  The PWM output is currently
   not used for anything, so only OC1 is exposed as a fixed 50% duty
   cycle to serve as a debug clock signal. */

const struct hw_multi_pwm hw_pwm_config = {
//  rcc_tim   rcc_gpio   gpio           gpio_config,               div       duty        irq (optional)
//---------------------------------------------------------------------------------------------------------------
    RCC_TIM3, TIM3,      {TIM3_GPIOS},  HW_GPIO_CONFIG_ALTFN_2MHZ, PWM_DIV,  PWM_DIV/2,  NVIC_TIM3_IRQ,
};
#define C_PWM hw_pwm_config

#define TIM_PWM 3

// see mod_oled.c
#define SPI_CS GPIOB,12
#define NOINLINE __attribute__((__noinline__))
NOINLINE void hw_spi_cs(int val) {
    //hw_busywait_us(10);
    hw_gpio_write(SPI_CS, val);
    //hw_busywait_us(10);
    if (val) {
        hw_busywait(10);
    }
}
static inline void hw_spi_cs_init(void) {
    hw_spi_cs(1);
    hw_gpio_config(SPI_CS, HW_GPIO_CONFIG_OUTPUT);

}

#define PIXI_REG_DEVID    0x00
#define PIXI_REG_ADC_DATA 0x40
#define PIXI_REG_DAC_DATA 0x60

void spi_wr_regs(uint8_t reg, const uint16_t *vals, uint32_t nb) {
    hw_spi_cs(0);
    hw_spi_nodma_wr(C_SPI, reg << 1);
    for (uint32_t i=0; i<nb; i++) {
        uint16_t val = *vals++;
        hw_spi_nodma_wr(C_SPI, (val >> 8));
        hw_spi_nodma_wr(C_SPI, val);
    }
    hw_spi_nodma_end(C_SPI);
    hw_spi_cs(1);
}
void spi_wr_reg(uint8_t reg, uint16_t val) {
    spi_wr_regs(reg, &val, 1);
}
void spi_rd_regs(uint8_t reg, uint16_t *vals, uint32_t nb) {
    /* Note that read flush is necessary because we do not perform a
       read in spi_wr_regs(). */
    hw_spi_nodma_rd_no_wait(C_SPI);

    hw_spi_cs(0);
    hw_spi_nodma_rdwr(C_SPI, 1 | (reg << 1));
    for (uint32_t i=0; i<nb; i++) {
        uint8_t hi_val = hw_spi_nodma_rdwr(C_SPI, 0);
        uint8_t lo_val = hw_spi_nodma_rdwr(C_SPI, 0);
        *vals++ = (hi_val << 8) | lo_val;
    }
    hw_spi_nodma_end(C_SPI);
    hw_spi_cs(1);
}
uint16_t spi_rd_reg(uint8_t reg) {
    uint16_t val;
    spi_rd_regs(reg, &val, 1);
    return val;
}


/* Convert the MAX11300_INIT macro to a const data structure. */
struct max11300_init_cmd { uint16_t cmd, val; };
#define WAIT_US 0x100
#define MAX11300_WRITE(reg,val) {reg,     val},
#define MAX11300_DELAY(val)     {WAIT_US, val},
const struct max11300_init_cmd max_11300_init_cmds[] = {
    MAX11300_INIT(MAX11300_WRITE,MAX11300_DELAY)
};
NOINLINE uint16_t pixi_devid(void) {
    app_.devid = spi_rd_reg(PIXI_REG_DEVID);
    return app_.devid;
}
NOINLINE void pixi_init(void) {
    for(uint32_t i=0; i<ARRAY_SIZE(max_11300_init_cmds); i++) {
        const struct max11300_init_cmd *c = &max_11300_init_cmds[i];
        if (c->cmd == WAIT_US) {
            hw_busywait_us(c->val);
        }
        else {
            spi_wr_reg(c->cmd, c->val);
        }
    }
    pixi_devid();
}




/* PDM TIMER INTERRUPT */
void HW_TIM_ISR(TIM_PWM)(void) {
    hw_multi_pwm_ack(C_PWM);
    spi_rd_regs(PIXI_REG_ADC_DATA + 12, app_.adc_vals, NB_ADC);
#if 1
    uint16_t inc = app_.adc_vals[0] >> 5;
#else
    uint16_t inc = 1;
#endif
    for (uint32_t i=0; i<NB_DAC; i++) {
        app_.dac_vals[i] += inc;
        app_.dac_vals[i] &= 0xFFF;
    }
    spi_wr_regs(PIXI_REG_DAC_DATA + 0,  app_.dac_vals, NB_DAC);
}


NOINLINE void pwm_start(void) {
    infof("start\n");
    hw_multi_pwm_init(C_PWM);
    hw_multi_pwm_start(C_PWM);
}
NOINLINE void pwm_stop(void) {
    hw_multi_pwm_stop(C_PWM);
}




/* COMMUNICATION */

void midi_write(const uint8_t *buf, uint32_t len) {
}
uint32_t midi_read(uint8_t *buf, uint32_t room) {
    return 0;
}




/* STARTUP, HOST */

#define LED GPIOC,13

void pixi_task(struct app *app) {
    if (app->next) { goto *app->next; }
    /* This part of init runs inside the ISR, to be able to reuse the
       SPI state machine to send out the PIXI config. */
}



void start(void) {
    hw_app_init();
    rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB | RCC_AFIO);

    /* App struct init */
    CBUF_INIT(app_.out);

    /* Turn on the LED to indicate we have started. */
    hw_gpio_config(LED,HW_GPIO_CONFIG_OUTPUT);
    hw_gpio_low(LED);


    hw_gpio_config(GPIOA,0,HW_GPIO_CONFIG_OUTPUT);
    hw_gpio_high(GPIOA,0);

    // synth_init(&app_.out);  // FIXME: review this
    hw_spi_cs_init();
    hw_spi_nodma_init(C_SPI);
    // hw_spi_start(C_SPI, app_.spi.buf, app_.spi.count);

    pixi_init();

    pwm_start();
}

#ifndef VERSION
#define VERSION "current"
#endif

const char config_manufacturer[] CONFIG_DATA_SECTION = "Zwizwa";
const char config_product[]      CONFIG_DATA_SECTION = "EuroPIXI";
const char config_firmware[]     CONFIG_DATA_SECTION = FIRMWARE;
const char config_protocol[]     CONFIG_DATA_SECTION = "{driver,pixi,midi}";

const struct gdbstub_io midi_io = {
    .read  = midi_read,
    .write = midi_write,
};

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
    .switch_protocol = NULL, // we use .io
    .flash_start     = (const void*)&config,
    .flash_endx      = (void*)&_firmware_endx,
    .control         = &control,
    .fwtag           = 0, // must be 0, used to recognize ecrypted firmware
    .info_buf        = &info_buf,
    .io              = &midi_io,
    .cmd_3if         = cmd_3if_list,
};

