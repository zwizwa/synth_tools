/**********************************************************************
* Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
**********************************************************************/

/* Adapeded from MAX11300.cpp by Tom Schouten

   This takes MAX11300Hex.h (in standard form, as generated by the
   config tool), and uses an adapted version of MAX11300.cpp to
   generate a data structure abstracting SPI register writes and
   delays. */


#include "MAX11300Hex.h" // Generated by config tool
#include "macros.h"
#include <stdint.h>

/* Converted from C++ */
#define MODE_BITMASK_PROCESS_1 0x047A
#define MODE_BITMASK_PROCESS_2 0x0380
#define MODE_BITMASK_PROCESS_3 0x1804
enum MAX11300_Port_Modes {
    ///HIGH_Z
    MODE_0,
    ///Digital input with programmable threshold, GPI
    MODE_1,
    ///Bidirectional level translator terminal
    MODE_2,
    ///Register-driven digital output with DAC-controlled level, GPO
    MODE_3,
    ///Unidirectional path output with DAC-controlled level, GPO
    MODE_4,
    ///Analog output for DAC
    MODE_5,
    ///Analog output for DAC with ADC monitoring
    MODE_6,
    ///Positive analog input to single-ended ADC
    MODE_7,
    ///Positive analog input to differential ADC
    MODE_8,
    ///Negative analog input to differential ADC
    MODE_9,
    ///Analog output for DAC and negative analog input to differential ADC
    MODE_10,
    ///Terminal to GPI-controlled analog switch
    MODE_11,
    ///Terminal to register-controlled analog switch
    MODE_12
};


static const uint16_t port_config_design_vals[20] = {
    port_cfg_00_DESIGNVALUE,
    port_cfg_01_DESIGNVALUE,
    port_cfg_02_DESIGNVALUE,
    port_cfg_03_DESIGNVALUE,
    port_cfg_04_DESIGNVALUE,
    port_cfg_05_DESIGNVALUE,
    port_cfg_06_DESIGNVALUE,
    port_cfg_07_DESIGNVALUE,
    port_cfg_08_DESIGNVALUE,
    port_cfg_09_DESIGNVALUE,
    port_cfg_10_DESIGNVALUE,
    port_cfg_11_DESIGNVALUE,
    port_cfg_12_DESIGNVALUE,
    port_cfg_13_DESIGNVALUE,
    port_cfg_14_DESIGNVALUE,
    port_cfg_15_DESIGNVALUE,
    port_cfg_16_DESIGNVALUE,
    port_cfg_17_DESIGNVALUE,
    port_cfg_18_DESIGNVALUE,
    port_cfg_19_DESIGNVALUE};

#define INDENT "    "
#define W(...) printf(__VA_ARGS__)
void write_register(MAX11300RegAddress_t reg, uint16_t data) {
    W(INDENT "write(0x%02x, 0x%04x) \\\n", reg, data);
}
void block_write(MAX11300RegAddress_t reg, uint16_t * data, uint8_t num_reg) {
    // Implement block_write in terms of write_register, so we only
    // need to implement one.
    for(uint8_t i=0; i<num_reg; i++) {
        write_register(reg, data[i]);
        reg++;
    }
}
void delayMicroseconds(uint32_t us) {
    if (us > 0) {
        W(INDENT "delay(%d) \\\n", us);
    }
}

void config_process_1(uint16_t *device_control_local) {
    uint8_t idx;
    uint16_t port_mode;
    uint16_t dac_data_array[20] = {
        dac_data_port_00_DESIGNVALUE,
        dac_data_port_01_DESIGNVALUE,
        dac_data_port_02_DESIGNVALUE,
        dac_data_port_03_DESIGNVALUE,
        dac_data_port_04_DESIGNVALUE,
        dac_data_port_05_DESIGNVALUE,
        dac_data_port_06_DESIGNVALUE,
        dac_data_port_07_DESIGNVALUE,
        dac_data_port_08_DESIGNVALUE,
        dac_data_port_09_DESIGNVALUE,
        dac_data_port_10_DESIGNVALUE,
        dac_data_port_11_DESIGNVALUE,
        dac_data_port_12_DESIGNVALUE,
        dac_data_port_13_DESIGNVALUE,
        dac_data_port_14_DESIGNVALUE,
        dac_data_port_15_DESIGNVALUE,
        dac_data_port_16_DESIGNVALUE,
        dac_data_port_17_DESIGNVALUE,
        dac_data_port_18_DESIGNVALUE,
        dac_data_port_19_DESIGNVALUE};

    *device_control_local |= (device_control_DESIGNVALUE & (device_control_DACREF | device_control_DACCTL));
    write_register(device_control, *device_control_local);

    delayMicroseconds(200);

    //Is DACCTL = 2 or 3
    if(((device_control_DESIGNVALUE & device_control_DACCTL) == 2) ||
       ((device_control_DESIGNVALUE & device_control_DACCTL) == 3)) {
        //yes
        write_register(dac_preset_data_1, dac_preset_data_1_DESIGNVALUE);
        write_register(dac_preset_data_2, dac_preset_data_2_DESIGNVALUE);
    }
    else {
        //no
        for(idx = 0; idx < 20; idx++) {
            port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
            if((port_mode == MODE_1) || (port_mode == MODE_3) ||
               (port_mode == MODE_4) || (port_mode == MODE_5) ||
               (port_mode == MODE_6) || (port_mode == MODE_10)) {
                write_register(dac_data_port_00 + idx, dac_data_array[idx]);
            }
        }
    }

    //Config FUNCID[i], FUNCPRM[i] for ports in mode 1
    uint8_t num_ports_mode_1 = 0;
    for(idx = 0; idx < 20; idx++) {
        port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
        if(port_mode == MODE_1) {
            write_register(port_cfg_00 + idx, port_config_design_vals[idx]);
            num_ports_mode_1++;
        }
    }

    delayMicroseconds(200 * num_ports_mode_1);

    //Config GPODAT[i] for ports in mode 3
    write_register(gpo_data_15_to_0, gpo_data_15_to_0_DESIGNVALUE);
    write_register(gpo_data_19_to_16, gpo_data_19_to_16_DESIGNVALUE);

    //Config FUNCID[i], FUNCPRM[i] for ports in mode 3, 4, 5, 6, or 10
    for(idx = 0; idx < 20; idx++) {
        port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
        if((port_mode == MODE_3) | (port_mode == MODE_4) |
           (port_mode == MODE_5) | (port_mode == MODE_6) |
           (port_mode == MODE_10)) {
            write_register(port_cfg_00 + idx, port_config_design_vals[idx]);
            delayMicroseconds(1000);
        }
    }

    //Config GPIMD[i] for ports in mode 1
    write_register(gpi_irqmode_7_to_0, gpi_irqmode_7_to_0_DESIGNVALUE);
    write_register(gpi_irqmode_15_to_8, gpi_irqmode_15_to_8_DESIGNVALUE);
    write_register(gpi_irqmode_19_to_16, gpi_irqmode_19_to_16_DESIGNVALUE);
}

//*********************************************************************
void config_process_2(uint16_t *device_control_local)
{
    uint8_t idx;
    uint16_t port_mode;

    for(idx = 0; idx < 20; idx++) {
        port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
        if(port_mode == MODE_9)
        {
            write_register(port_cfg_00 + idx, port_config_design_vals[idx]);
            delayMicroseconds(100);
        }
    }

    for(idx = 0; idx < 20; idx++) {
        port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
        if((port_mode == MODE_7) || (port_mode == MODE_8))
        {
            write_register(port_cfg_00 + idx, port_config_design_vals[idx]);
            delayMicroseconds(100);
        }
    }

    *device_control_local |= (device_control_DESIGNVALUE & device_control_ADCCTL);
    write_register(device_control, *device_control_local);
}

//*********************************************************************
void config_process_3(void)
{
    uint8_t idx;
    uint16_t port_mode;

    for(idx = 0; idx < 20; idx++) {
        port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
        if((port_mode == MODE_2) || (port_mode == MODE_11) || (port_mode == MODE_12)) {
            write_register(port_cfg_00 + idx, port_config_design_vals[idx]);
        }
    }
}




void init() {

    //see datasheet 19-7318; Rev 3; 4/16; page 49
    //https://datasheets.maximintegrated.com/en/ds/MAX11300.pdf
    //for description of configuration process

    uint8_t idx;
    uint8_t port_mode;
    uint16_t mode_bit_mask = 0;

    //figure out port modes used
    for(idx = 0; idx < 20; idx++) {
        port_mode = ((port_config_design_vals[idx] & 0xf000) >> 12);
        if(port_mode > 0) {
            mode_bit_mask |= (1 << port_mode);
        }
    }

    //STEP 1: configure BRST, THSHDN, ADCCONV
    uint16_t device_control_local = (device_control_DESIGNVALUE & (device_control_BRST | device_control_THSHDN | device_control_ADCCONV));
    write_register(device_control, device_control_local);

    //STEP 2: If any port is configured for modes 1,3,4,5,6, or 10
    if(mode_bit_mask & MODE_BITMASK_PROCESS_1) {
        config_process_1(&device_control_local);
    }

    //STEP 3: If any port is configured for modes 7,8, or 9
    if(mode_bit_mask & MODE_BITMASK_PROCESS_2) {
        config_process_2(&device_control_local);
    }

    //STEP 4: If any port is configured for modes 2,11, or 12
    if(mode_bit_mask & MODE_BITMASK_PROCESS_3) {
        config_process_3();
    }

    //STEP 5: Are Temperature sensors used?
    if(device_control_DESIGNVALUE & (device_control_TMPCTLEXT1 | device_control_TMPCTLEXT0 | device_control_TMPCTLINT)) {
        device_control_local |= (device_control_DESIGNVALUE & (device_control_TMPPER | device_control_RS_CANCEL));
        write_register(device_control, device_control_local);

        uint16_t temp_thresholds [6] = {
            tmp_mon_int_hi_thresh_DESIGNVALUE,
            tmp_mon_int_lo_thresh_DESIGNVALUE,
            tmp_mon_ext1_hi_thresh_DESIGNVALUE,
            tmp_mon_ext1_lo_thresh_DESIGNVALUE,
            tmp_mon_ext2_hi_thresh_DESIGNVALUE,
            tmp_mon_ext2_lo_thresh_DESIGNVALUE};
        block_write(tmp_mon_int_hi_thresh, temp_thresholds, 6);

        device_control_local |= (device_control_DESIGNVALUE & (device_control_TMPCTLEXT1 | device_control_TMPCTLEXT0 | device_control_TMPCTLINT));
        write_register(device_control, device_control_local);
    }

    //STEP 6: Configure interrupt masks
    write_register(interrupt_mask, interrupt_mask_DESIGNVALUE);
}



int main(int argc, char **argv) {
    W("// generated from MAX11300Hex.h by gen_max11300.c\n");
    W("#define MAX11300_INIT(write, delay) \\\n");
    W("\n");
    init();
}
