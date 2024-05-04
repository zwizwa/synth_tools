// Adapted by Tom Schouten, from alsa-tools/envy24control
// This can also be done using alsactl:
// http://linux-audio.com/ice1712multi.html

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
    unsigned int subvendor;  /* PCI[2c-2f] */
    unsigned char size;      /* size of EEPROM image in bytes */
    unsigned char version;   /* must be 1 */
    unsigned char codec;     /* codec configuration PCI[60] */
    unsigned char aclink;    /* ACLink configuration PCI[61] */
    unsigned char i2sID;     /* PCI[62] */
    unsigned char spdif;     /* S/PDIF configuration PCI[63] */
    unsigned char gpiomask;  /* GPIO initial mask, 0 = write, 1 = don't */
    unsigned char gpiostate; /* GPIO initial state */
    unsigned char gpiodir;   /* GPIO direction state */
    unsigned short ac97main;
    unsigned short ac97pcm;
    unsigned short ac97rec;
    unsigned char ac97recsrc;
    unsigned char dacID[4];  /* I2S IDs for DACs */
    unsigned char adcID[4];  /* I2S IDs for ADCs */
    unsigned char extra[4];
} ice1712_eeprom_t;


static void master_clock_word_select(
    snd_ctl_t *ctl,
    snd_ctl_elem_value_t *word_clock_sync,
    int on)
{
    int err;
    /* Only works for ICE1712_SUBDEVICE_DELTA1010 and
       ICE1712_SUBDEVICE_DELTA1010LT. This tool does not read the
       EEPROM to determine device type, see original code. */
    snd_ctl_elem_value_set_boolean(word_clock_sync, 0, on ? 1 : 0);
    if ((err = snd_ctl_elem_write(ctl, word_clock_sync)) < 0) {
        ERROR("Unable to write word clock sync selection: %s\n", snd_strerror(err));
    }
}
static void internal_clock_set(
    snd_ctl_t *ctl,
    snd_ctl_elem_value_t *word_clock_sync,
    snd_ctl_elem_value_t *internal_clock,
    int xrate)
{
    int err;
    master_clock_word_select(ctl, word_clock_sync, 0);
    snd_ctl_elem_value_set_enumerated(internal_clock, 0, xrate);
    if ((err = snd_ctl_elem_write(ctl, internal_clock)) < 0) {
        ERROR("Unable to write internal clock rate: %s\n", snd_strerror(err));
    }
}

void setup(snd_ctl_t *ctl, int syncval) {

    //ice1712_eeprom_t card_eeprom;

    static snd_ctl_elem_value_t *internal_clock;
    static snd_ctl_elem_value_t *word_clock_sync;


    /* FIXME: It seems possible to change the default state.
       See original envy24control source. */

    /* Mixer elements are accessed by name.  See kernel source:

       ~/git/linux/sound$ grep -re "Multi Track Internal Clock" *
       pci/ice1712/ice1712.c: .name = "Multi Track Internal Clock",

       ~/git/linux/sound/pci$ grep -re 'Word Clock Sync' *
       ice1712/delta.c:ICE1712_GPIO(SNDRV_CTL_ELEM_IFACE_MIXER, "Word Clock Sync", 0, ICE1712_DELTA_WORD_CLOCK_SELECT, 1, 0);
    */

    int err;

    ASSERT(0 == snd_ctl_elem_value_malloc(&internal_clock));
    snd_ctl_elem_value_set_interface(internal_clock, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(internal_clock, "Multi Track Internal Clock");
    if ((err = snd_ctl_elem_read(ctl, internal_clock)) < 0) {
        ERROR("Unable to read Internal Clock state: %s\n", snd_strerror(err));
    }
    LOG("internal_clock, was %d, setting %d\n",
        snd_ctl_elem_value_get_enumerated(internal_clock, 0),
        syncval);

    ASSERT(0 == snd_ctl_elem_value_malloc(&word_clock_sync));
    snd_ctl_elem_value_set_interface(word_clock_sync, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(word_clock_sync, "Word Clock Sync");

    if (1) {
        /* Default in my setup seems to be 8 (44.1kHz internal).
           I want it to be 13 (S/PDIF in). */
        internal_clock_set(ctl, word_clock_sync, internal_clock, syncval);
    }
}

int main(int argc, char **argv) {

    if (argc != 3) {
        ERROR("usage: %s <card> <syncval>\n", argv[0]);
    }
    char cardname[8 + strlen(argv[1])];
    strcpy(cardname, argv[1]);

    int syncval = atoi(argv[2]);

    snd_ctl_card_info_t *hw_info;
    snd_ctl_card_info_alloca(&hw_info);

    int index;
    if ((index = snd_card_get_index(cardname)) >= 0) {
        LOG("%s is hw:%d\n", cardname, index);
        sprintf(cardname, "hw:%d", index);
    }

    snd_ctl_t *ctl;
    if (snd_ctl_open(&ctl, cardname, 0) < 0) {
        ERROR("can't open %s\n", cardname);
    }
    if (snd_ctl_card_info(ctl, hw_info) < 0 ||
        strcmp(snd_ctl_card_info_get_driver(hw_info), "ICE1712")) {
        snd_ctl_close(ctl);
        ERROR("%s is not an ICE1712\n", cardname);
    }
    /* found */
    LOG("opened ICE1712 %s\n", cardname);
    setup(ctl, syncval);
    return 0;
}
