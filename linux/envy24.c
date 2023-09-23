// Adapted by Tom Schouten, from alsa-tools/envy24control

/*****************************************************************************
   hardware.c - Hardware Settings
   Copyright (C) 2000 by Jaroslav Kysela <perex@perex.cz>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include <alsa/asoundlib.h>
#include "macros.h"

/* MidiMan */
#define ICE1712_SUBDEVICE_DELTA1010	0x121430d6
#define ICE1712_SUBDEVICE_DELTADIO2496	0x121431d6
#define ICE1712_SUBDEVICE_DELTA66	0x121432d6
#define ICE1712_SUBDEVICE_DELTA44	0x121433d6
#define ICE1712_SUBDEVICE_AUDIOPHILE    0x121434d6
#define ICE1712_SUBDEVICE_DELTA410      0x121438d6
#define ICE1712_SUBDEVICE_DELTA1010LT   0x12143bd6

/* Terratec */
#define ICE1712_SUBDEVICE_EWX2496       0x3b153011
#define ICE1712_SUBDEVICE_EWS88MT       0x3b151511
#define ICE1712_SUBDEVICE_EWS88D        0x3b152b11
#define ICE1712_SUBDEVICE_DMX6FIRE      0x3b153811

typedef struct {
    unsigned int subvendor;	/* PCI[2c-2f] */
    unsigned char size;	/* size of EEPROM image in bytes */
    unsigned char version;	/* must be 1 */
    unsigned char codec;	/* codec configuration PCI[60] */
    unsigned char aclink;	/* ACLink configuration PCI[61] */
    unsigned char i2sID;	/* PCI[62] */
    unsigned char spdif;	/* S/PDIF configuration PCI[63] */
    unsigned char gpiomask;	/* GPIO initial mask, 0 = write, 1 = don't */
    unsigned char gpiostate; /* GPIO initial state */
    unsigned char gpiodir;	/* GPIO direction state */
    unsigned short ac97main;
    unsigned short ac97pcm;
    unsigned short ac97rec;
    unsigned char ac97recsrc;
    unsigned char dacID[4];	/* I2S IDs for DACs */
    unsigned char adcID[4];	/* I2S IDs for ADCs */
    unsigned char extra[4];
} ice1712_eeprom_t;
ice1712_eeprom_t card_eeprom;
snd_ctl_t *ctl;

static snd_ctl_elem_value_t *internal_clock;
static snd_ctl_elem_value_t *word_clock_sync;
int card_number;


static void master_clock_word_select(int on) {
    int err;
    /* Only works for ICE1712_SUBDEVICE_DELTA1010 and
       ICE1712_SUBDEVICE_DELTA1010LT. This tool does not read the
       EEPROM to determine device type, see original code. */
    snd_ctl_elem_value_set_boolean(word_clock_sync, 0, on ? 1 : 0);
    if ((err = snd_ctl_elem_write(ctl, word_clock_sync)) < 0) {
        ERROR("Unable to write word clock sync selection: %s\n", snd_strerror(err));
    }
}
static void internal_clock_set(int xrate) {
    int err;
    master_clock_word_select(0);
    snd_ctl_elem_value_set_enumerated(internal_clock, 0, xrate);
    if ((err = snd_ctl_elem_write(ctl, internal_clock)) < 0) {
        ERROR("Unable to write internal clock rate: %s\n", snd_strerror(err));
    }
}
int main(int argc, char **argv) {
    /* probe cards */
    int err;
    static char cardname[8];
    char *name = NULL; // probe
    snd_ctl_card_info_t *hw_info;
    snd_ctl_card_info_alloca(&hw_info);

    /* FIXME: hardcoded max number of cards */
    for (card_number = 0; card_number < 8; card_number++) {
        sprintf(cardname, "hw:%d", card_number);
        if (snd_ctl_open(&ctl, cardname, 0) < 0)
            continue;
        if (snd_ctl_card_info(ctl, hw_info) < 0 ||
            strcmp(snd_ctl_card_info_get_driver(hw_info), "ICE1712")) {
            snd_ctl_close(ctl);
            continue;
        }
        /* found */
        name = cardname;
        LOG("found %s\n", cardname);
        break;
    }
    if (! name) {
        ERROR("No ICE1712 cards found\n");
    }
    /* FIXME: It seems possible to change the default state.
       See original source. */

    ASSERT(0 == snd_ctl_elem_value_malloc(&internal_clock));
    snd_ctl_elem_value_set_interface(internal_clock, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(internal_clock, "Multi Track Internal Clock");
    if ((err = snd_ctl_elem_read(ctl, internal_clock)) < 0) {
        ERROR("Unable to read Internal Clock state: %s\n", snd_strerror(err));
    }
    LOG("internal_clock = %d\n", snd_ctl_elem_value_get_enumerated(internal_clock, 0));

    ASSERT(0 == snd_ctl_elem_value_malloc(&word_clock_sync));
    snd_ctl_elem_value_set_interface(word_clock_sync, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(word_clock_sync, "Word Clock Sync");

    if (1) {
        /* Default in my setup seems to be 8 (44.1kHz internal).
           I want it to be 13 (S/PDIF in). */
        internal_clock_set(13);
    }
    return 0;
}
