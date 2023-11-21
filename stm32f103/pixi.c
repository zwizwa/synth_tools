/* Euro PIXI firmware.

   Currently this is just a playground to further develop the "state
   machine OS" architecture.  Some ideas:

   - A stream based text console with transport over RTT or Sysex
     connections, optimized for high latency operation.

   - Control rate tasks implemented as state machines running in a
     CSP-like scheduler.

   - Software timer.

   - Run from interrupt.

   - Forth-like language to implement blocking tasks, with host-side
     tethering support.

   - Garbage-collected cell store (per task?).

*/


#include <stdint.h>
#include "fixedpoint.h"
#include "base.h"

#include "gdbstub_api.h"
#include <string.h>

#include "pbuf.h"
#include "cbuf.h"
#include "cmd_3if.h"

#include "pixi_v1_max11300.h"

#include "registers_stm32f103.h"

#include "mod_sequencer.c"

#define NOINLINE __attribute__((__noinline__))


/* STATE */

/* RTT communication */
#include "rtt.h"

/* Note: OpenOCD RTT implementation doesn't have backpressure.
   Anything that is sent to the TCP socket immediately gets propagated
   to the RTT buffer and *any excess data is silently dropped*.  Pick
   a buffer size large enough.  This is the first data definition to
   reduce the change that it will move in memory between
   recompilations, which makes it possible to keep RTT alive across
   reboots. */
#define RTT_BUF_SIZE 256

uint8_t rtt_up[RTT_BUF_SIZE];
uint8_t rtt_down[RTT_BUF_SIZE];
struct rtt_1_1 rtt = RTT_1_1_INIT(rtt_up, rtt_down);


#define NB_DAC 12
#define NB_ADC 6
#define NB_CV 2
typedef uint16_t tick_counter_t;
struct ticks {
    tick_counter_t pwm, swt, swi, event, midi_clock, midi[3];
};
struct app {
    void *next;
    volatile uint32_t swi_from_mainloop;
    volatile uint32_t swi_from_timer;
    volatile uint32_t cv[NB_CV]; // Atomic, so use machine word size
    struct cbuf out; uint8_t out_buf[128];
    struct ticks ticks;
    uint16_t dac_vals[NB_DAC];
    uint16_t adc_vals[NB_ADC];
    uint16_t pixi_devid;
    uint8_t started:1;
    struct sequencer sequencer;
};
struct app app_;

// Map a struct's field to the struct's pointer.
#define DEF_FIELD_TO_PARENT(function_name, parent_type, field_type, field_name) \
    static inline parent_type *function_name(field_type *field_ptr) {   \
        uint8_t *u8 = ((uint8_t *)field_ptr) - OFFSETOF(parent_type,field_name); \
        return (parent_type *)u8;                                       \
    }
// Field names and struct names are the same, so this can be shortened.
#define DEF_FIELD_TO_APP(field) \
    DEF_FIELD_TO_PARENT(field##_to_app, struct app, struct field, field)

DEF_FIELD_TO_APP(sequencer)



/* CONFIGURATION */

/* Some design considerations.

   - There doesn't seem to be a straightforward way to sync to the
     PIXI's DAC update clock, so just use a timer to feed it at a
     constant rate and live with the jitter that that produces.

     One potential way to work around this is to use one of the DAC
     ports as a digital signal and have that feed back to an STM edge
     detection interrupt.  Doesn't seem to be worth the hassle.

   - The DAC cycle is (* 40 12) 480 uS, or (/ 1.0 0.480) 2.083 kHz.
     Use a 2kHz timer to drive the DAC.

   - PIXI clock MAX is 20MHz.  DIV_2 is 18MHz

   - The packets are short, the 18MHz SPI rate is relatively high, so
     don't bother with DMA, just busy-wait on the SPI inside the
     interrupt.  See git history before this message for original DMA
     code.


*/


const struct hw_swi swi = HW_SWI_0;

/* The SWI is unidirectional (i.e. no RPC).
   The queue depth is 1: assuming messages are handled immediately. */
KEEP NOINLINE void swi_from_mainloop(struct app *app, uint32_t msg) {
    app->swi_from_mainloop = msg;
    hw_swi_trigger(swi);
}
KEEP NOINLINE void swi_from_timer(struct app *app, uint32_t msg) {
    app->swi_from_timer = msg;
    hw_swi_trigger(swi);
}



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
   not used for anything other than debugging, e.g. OC1 is exposed as
   a fixed 50% duty cycle to serve as a scope sync. */

const struct hw_multi_pwm hw_tim_pwm = {
//  rcc_tim   rcc_gpio   gpio           gpio_config,               div       duty        irq (optional)
//---------------------------------------------------------------------------------------------------------------
    RCC_TIM3, TIM3,      {TIM3_GPIOS},  HW_GPIO_CONFIG_ALTFN_2MHZ, PWM_DIV,  PWM_DIV/2,  NVIC_TIM3_IRQ,
};
#define C_PWM hw_tim_pwm

#define TIM_PWM 3

// see mod_oled.c
#define SPI_CS GPIOB,12
NOINLINE void hw_spi_cs(int val) {
    hw_gpio_write(SPI_CS, val);
#if 0
    if (val) {
        hw_busywait(10);
    }
#endif
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
    app_.pixi_devid = spi_rd_reg(PIXI_REG_DEVID);
    return app_.pixi_devid;
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

static inline void dac_adc_update(struct app *app) {
    spi_rd_regs(PIXI_REG_ADC_DATA + 12, app->adc_vals, NB_ADC);
    /* Use first knob to set the LFO speed. */
    uint16_t inc = app->adc_vals[0] >> 5;

    /* Fill with demo LFO */
    for (uint32_t i=0; i<NB_DAC; i++) {
        app->dac_vals[i] += inc;
        app->dac_vals[i] &= 0xFFF;
    }

    /* Override with CV sequencer channels + duplicates. */
    for (uint32_t i=0; i<NB_CV; i++) {
        app->dac_vals[i] = app->cv[i];
        app->dac_vals[i+NB_DAC/2] = app->cv[i];
    }
    app->ticks.pwm++;
}
void HW_TIM_ISR(TIM_PWM)(void) {
    /* Platform-dependent and global variable access go into this
       function... */
    hw_multi_pwm_ack(C_PWM);
    struct app *app = &app_;

    /* ... such that platform-independent code can be separated for
       re-use in tests or other code. */
    dac_adc_update(app);

    spi_wr_regs(PIXI_REG_DAC_DATA + 0,  app->dac_vals, NB_DAC);

    // (/ 2000.0 256)
#if 0
    /* FIXME: proper divider. */
    if ((app->ticks.pwm & 0xFF) == 0) {
        /* Midi clock */
        swi_from_timer(app, 0xF8);
    }
#endif
}


NOINLINE void pwm_start(void) {
    infof("pwm: start\n");
    hw_multi_pwm_init(C_PWM);
    hw_multi_pwm_start(C_PWM);
}
NOINLINE void pwm_stop(void) {
    infof("pwm: stop\n");
    hw_multi_pwm_stop(C_PWM);
}



/* EVENT LEVEL INTERRUPTS */

// Event level logging
void isr_log(const void *buf, uint32_t nb) {
}
#define ISR_LOG(...) \
    do { uint32_t info_data[] = { __VA_ARGS__ }; \
        isr_log(info_data, sizeof(info_data)); } while(0)
#define ORE_TOKEN 1


/* Events are encoded in uint32_t as follows
   UART rx    0x0000PEDD  P=port, E=error, DD=data
   UART other 0x0001Pxxx  P=port,
   other void 0x0002xxxx
*/
#define ISR_EVENT_TX_DONE  0x010000
#define ISR_EVENT_TX_READY 0x010001

#define ISR_EVENT_TIMER    0x020000
#define ISR_EVENT_SWI      0x020001

#define ISR_EVENT_CONSOLE  0x030000

void pattern_start(struct sequencer *);
NOINLINE void isr_event(struct app *app, uint32_t event) {
    app->ticks.event++;
    switch(event) {
    case 0xF8:
        /* MIDI clock on port 0. */
        app->ticks.midi_clock++;
        if (app->started) {
            /* For all tracks/sequences, play the current event and
               advance time/period. */
            sequencer_tick(&app->sequencer);
        }
        break;
    case 0xFA:
        /* Start */
        app->started = 1;
        pattern_start(&app->sequencer);
       break;
    case 0xFB:
        /* Continue */
        /* Enable playback. */
        app->started = 1;
        break;
    case 0xFC:
        /* Stop */
        /* Disable playback. */
        app->started = 0;
        break;
    }
}

static inline void swi_handle(struct app *app, volatile uint32_t *pevt) {
    uint32_t evt = *pevt;
    if (evt) {
        *pevt = 0;
        isr_event(app, evt);
    }
}

void exti0_isr(void) {
    hw_swi_ack(swi);
    struct app *app = &app_;
    app->ticks.swi++;
    /* Software interrupts can come from higher and lower priority
       tasks.  It is assumed that SWI handler can service these before
       the next one arrives, so no queues are used.  This is always
       the case for _from_mainloop as ISR will handle before main loop
       task can continue.  For _from_timer it is less clear if this is
       a good idea, but for now let's assume that the timer interrupt
       doesn't generate events too quickly, e.g. midi clock or some
       edge detector on ADC data.  Always handle timer interrupts
       first. */
    swi_handle(app, &app->swi_from_timer);
    swi_handle(app, &app->swi_from_mainloop);
}





#define HW_CPU_MHZ 72

// Software timer
const struct hw_delay hw_tim_swt = {
// rcc       irq            tim   psc
// -----------------------------------------------------------------
   RCC_TIM4, NVIC_TIM4_IRQ, TIM4, HW_CPU_MHZ,
};
// Note that TIM2 seems to interact badly with Flash programming.
// Could be a silicon problem but there is nothing about this in the
// errata.  The supposedly identical timer TIM4 works fine.
#define TIM_SWT 4
#define C_TIM_SWT hw_tim_swt


void HW_TIM_ISR(TIM_SWT)(void) {
    hw_delay_ack(C_TIM_SWT);
    struct app *app = &app_;
    app->ticks.swt++;
    isr_event(app, ISR_EVENT_TIMER);
    // FIXME: plug in the software timer.
    uint16_t diff_us = 1000;
    hw_delay_arm(C_TIM_SWT, diff_us);
    hw_delay_trigger(C_TIM_SWT);
}


struct midi_port {
    /* USART register window */
    struct map_usart *usart;
};

/* 5V tolerant RX:
   UART1 TX=A9  RX=A10
   UART3 TX=B10 RX=B11

   3V3 only RX:
   UART2 TX=A2 RX=A3 is only 3v3

   On Euro PIXI board, UART1 is exposed on the back.

*/

struct midi_port midi_port[3] = {
    [0] = { .usart = (struct map_usart*)USART1 },
    [1] = { .usart = (struct map_usart*)USART2 },
    [2] = { .usart = (struct map_usart*)USART3 },
};
INLINE void usart_isr(struct app *app, uint32_t port_nb) {
    app->ticks.midi[port_nb]++;
    struct map_usart *usart = midi_port[port_nb].usart;
    uint32_t sr = usart->sr;
    uint32_t cr1 = usart->cr1;
    uint32_t sr_and_cr1 = sr & cr1;

    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
	/* Read data register not empty. */
	/* Reading DR will clear RXNE, ORE */
	uint32_t dr = usart->dr & USART_DR_MASK;
	uint16_t token = ((sr & 0xF) << 8) | (dr & 0xFF);
        if(sr & USART_SR_ORE) ISR_LOG(ORE_TOKEN, token);
	isr_event(app, token | (port_nb << 12));
	return;
    }

    /* For TXE and TC we disable the corresponding interrupt.
       Downstream code will need to explicitly enable again. */
    if (sr_and_cr1 & USART_SR_TXE) {
	usart->cr1 &= ~USART_CR1_TXEIE;
	isr_event(app, ISR_EVENT_TX_READY | (port_nb << 12));
	return;
    }
    if (sr_and_cr1 & USART_SR_TC) {
	usart->cr1 &= ~USART_CR1_TCIE;
	isr_event(app, ISR_EVENT_TX_DONE | (port_nb << 12));
	return;
    }
}

struct hw_midi_port {
    uint32_t rcc;
    uint32_t irq;
    uint32_t usart;
    uint32_t gpio;
    uint32_t tx,rx;
    uint32_t div;
};
#define MIDI_BAUD_DIV(MHZ)  (((MHZ)*1000000) / 31250)

// config is indexed by uart number.
// for mab_div we need to use bus frequences. UART1 is on a different bus from UART2,UART3
const struct hw_midi_port hw_midi_port[] = {
    //    rcc          irq              usart   gpio   tx  rx  div,
    [0] = {RCC_USART1, NVIC_USART1_IRQ, USART1, GPIOA,  9, 10, MIDI_BAUD_DIV(72)},
    [1] = {RCC_USART2, NVIC_USART2_IRQ, USART2, GPIOA,  2,  3, MIDI_BAUD_DIV(36)},
    [2] = {RCC_USART3, NVIC_USART3_IRQ, USART3, GPIOB, 10, 11, MIDI_BAUD_DIV(36)},
};
void HW_USART_ISR(1)(void) { usart_isr(&app_, 0); }
void HW_USART_ISR(2)(void) { usart_isr(&app_, 1); }
void HW_USART_ISR(3)(void) { usart_isr(&app_, 2); }

INLINE void hw_midi_port_init(struct hw_midi_port p) {

    rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB | RCC_AFIO);
    rcc_periph_clock_enable(p.rcc);
    nvic_enable_irq(p.irq);
    hw_gpio_config(p.gpio, p.tx, HW_GPIO_CONFIG_ALTFN);

    // enable pullup for RX in to avoid junk when transceiver is not
    // connected.
    hw_gpio_high(p.gpio, p.rx); // pull direction
    hw_gpio_config(p.gpio, p.rx, HW_GPIO_CONFIG_INPUT_PULL);

    hw_usart_disable(p.usart);
    hw_usart_set_databits(p.usart, 8);
    hw_usart_set_stopbits(p.usart, USART_STOPBITS_1);
    hw_usart_set_mode(p.usart, USART_MODE_TX_RX);
    hw_usart_set_parity(p.usart, USART_PARITY_NONE);
    hw_usart_set_flow_control(p.usart, USART_FLOWCONTROL_NONE);
    USART_BRR(p.usart) = p.div;

    hw_usart_enable_rx_interrupt(p.usart);
    hw_usart_enable(p.usart);
}

/* COMMUNICATION */

void midi_write(const uint8_t *buf, uint32_t len) {
    /* Currently only doing single byte events.  Multi-byte events
       could be parsed here then sent in one go, but it might be
       simpler to just do one interrupt per byte and put the parser in
       the isr since it is needed for uart anyway. */
    struct app *app = &app_;
    for (uint32_t i=0; i<len; i++) {
        uint8_t byte = buf[i];
        // LOG("usb midi %02x\n", byte);
        switch(buf[i]) {
        case 0xF8:
        case 0xFA:
        case 0xFB:
        case 0xFC:
            swi_from_mainloop(app, byte);
            break;
        }
    }
}
uint32_t midi_read(uint8_t *buf, uint32_t room) {
    return 0;
}

void rtt_info_poll(struct app *app) {
    uint32_t n = info_bytes();
    if (n > 0) {
	uint8_t buf[n];
        /* Payload. */
        uint32_t nb = info_read(buf, n);
	rtt_target_up_write(&rtt.hdr, 0, buf, nb);
    }
}
void rtt_command_poll(struct app *app) {
    struct rtt_buf *down = rtt_down_buf(&rtt.hdr, 0);
    uint32_t n = rtt_buf_elements(down);
    if (n > 0) {
        uint8_t buf[n];
        uint32_t nb = rtt_buf_read(down, buf, n);
        infof("received %d\n", nb);
        swi_from_mainloop(app, ISR_EVENT_CONSOLE + nb);
    }
}





/* STARTUP, HOST */

#define LED GPIOC,13

void vm_next(struct app *app) {
    if (app->next) { goto *app->next; }
}

void pixi_poll(void) {
    /* Poll non-event driven code. */
    struct app *app = &app_;
    rtt_command_poll(app);
    rtt_info_poll(app);

    /* Enter the VM. */
    vm_next(app);
}

void pat_dispatch(struct sequencer *seq, const struct pattern_step *step) {
    infof("pat_dispatch %d %d\n", step->event.as.u8[0], step->delay);
    struct app *app = sequencer_to_app(seq);
    // ASSERT(step->event[0] == 0);  // not midi but 16 bit CV/gate channel
    uint32_t chan = step->event.as.u16[0];
    uint16_t val  = step->event.as.u16[1];
    //uint32_t val  = step->event & 0xFFF;
    //uint32_t chan = (step->event >> 12) & 0xF;
    if (chan < NB_CV) {
        app->cv[chan] = val;
    }
}
void pattern_init(struct sequencer *s) {
    sequencer_init(s, pat_dispatch);
    // FIXME: This has changed. Look at test_sequencer.c
    sequencer_start(s);
}
void pattern_start(struct sequencer *s) {
    /* Reset counters, enable playback. */
    // pat.next_step = 0; // FIXME: still needed?
    sequencer_restart(s);
}


void start(void) {
    hw_app_init();
    infof("pixi: start\n");

    // rcc_periph_clock_enable(RCC_GPIOA | RCC_GPIOB | RCC_GPIOC | RCC_AFIO);

    rcc_periph_clock_enable(RCC_GPIOC);

    /* App struct init */
    CBUF_INIT(app_.out);
    pattern_init(&app_.sequencer);
    app_.started = 1;

    /* Turn on the LED to indicate we have started. */
    hw_gpio_config(LED,HW_GPIO_CONFIG_OUTPUT);
    hw_gpio_low(LED);

    // FIXME: What is this for?
    hw_gpio_config(GPIOA,0,HW_GPIO_CONFIG_OUTPUT);
    hw_gpio_high(GPIOA,0);

    /* Software interrupt used to implement system calls from other
       priority levels into ISR event level. */
    hw_swi_init(swi);

    /* UART init */
    hw_midi_port_init(hw_midi_port[0]);
    //hw_midi_port_init(hw_midi_port[1]);
    //hw_midi_port_init(hw_midi_port[2]);

    /* SPI, PIXI init. */
    hw_spi_cs_init();
    hw_spi_nodma_init(C_SPI);
    pixi_init();

    /* Start the high priority ADC/DAC/PWM timer. */
    pwm_start();

    /* Start the low priority event timer. */
    hw_delay_init(C_TIM_SWT, 0xFFFF, 1 /*enable interrupt*/);
    hw_delay_arm(C_TIM_SWT, 1);
    hw_delay_trigger(C_TIM_SWT);

    /* Priorities of UART, SWI and event TIMER need to be the same.
       This is essential because they all update the ISR state machine
       and thus are not allowed to pre-empt each other. */
    uint32_t lo_pri = 16;
    NVIC_IPR(swi.irq) = lo_pri;
    NVIC_IPR(hw_tim_swt.irq) = lo_pri;
    NVIC_IPR(NVIC_USART1_IRQ) = lo_pri;
    NVIC_IPR(NVIC_USART2_IRQ) = lo_pri;
    NVIC_IPR(NVIC_USART3_IRQ) = lo_pri;
    /* The PWM/ADC/DAC interrupt needs to pre-empt those. */
    uint32_t hi_pri = 0;
    NVIC_IPR(hw_tim_pwm.irq) = hi_pri;

    /* Main loop background. */
    _service.add(pixi_poll);

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

