/* USB Midi port
   Cobbled together from old CDC ACM code and
   https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f4/stm32f4-discovery/usb_midi/usbmidi.c
   https://www.usb.org/sites/default/files/USB%20MIDI%20v2_0.pdf
   https://datahacker.blog/index.php?option=com_dropfiles&format=&task=frontfile.download&catid=86&id=96&Itemid=1000000000000

tom@luna:/proc/asound$ cat cards
...
 3 [Synth          ]: USB-Audio - MIDI Synth
                      Zwizwa MIDI Synth at usb-0000:00:14.0-3.4.1, full speed


tom@luna:/proc/asound$ cat card3/midi0
MIDI Synth

Output 0
  Tx bytes     : 0
Input 0
  Rx bytes     : 0


  Some remarks:
  - message type determins message size
  - "whenever possible" messages should stay within 1 bulk transaction

*/


/* OpenCM3 driver code.

   On uc_tools STM32F103, the driver used is:
   hw_stm32f103.h:74:#define HW_USB_DRIVER &st_usbfs_v1_usb_driver

   Which is defined here:
   st_usbfs_v1.c:30:const struct _usbd_driver st_usbfs_v1_usb_driver = {

   The endpoint routines are

   .ep_write_packet = st_usbfs_ep_write_packet,
   .ep_read_packet = st_usbfs_ep_read_packet,

   These are defined in st_usbfs_core.c in terms of routines in st_usbfs_v1.c :
   st_usbfs_copy_to_pm()
   st_usbfs_copy_from_pm()

   The data is written 16 bits at a time to a memory mapped region.
   Note that the mapping embeds 16 bits in 32 bit words.  See
   functions above.
*/




/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2014 Daniel Thompson <daniel@redfelineninja.org.uk>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/audio.h>
#include <libopencm3/usb/midi.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

/*
 * All references in this file come from Universal Serial Bus Device Class
 * Definition for MIDI Devices, release 1.0.
 */

/*
 * Table B-1: MIDI Adapter Device Descriptor
 */
static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,    /* was 0x0110 in Table B-1 example descriptor */
    .bDeviceClass = 0,   /* device defined at interface level */
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x6666,  /* Prototype product vendor ID */
    .idProduct = 0x5119, /* dd if=/dev/random bs=2 count=1 | hexdump */
    .bcdDevice = 0x0100,
    .iManufacturer = 1,  /* index to string desc */
    .iProduct = 2,       /* index to string desc */
    .iSerialNumber = 3,  /* index to string desc */
    .bNumConfigurations = 1,
};

/*
 * Midi specific endpoint descriptors.
 */
static const struct usb_midi_endpoint_descriptor midi_bulk_endp[] = {{
	/* Table B-12: MIDI Adapter Class-specific Bulk OUT Endpoint
	 * Descriptor
	 */
	.head = {
            .bLength = sizeof(struct usb_midi_endpoint_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_ENDPOINT,
            .bDescriptorSubType = USB_MIDI_SUBTYPE_MS_GENERAL,
            .bNumEmbMIDIJack = 1,
	},
	.jack[0] = {
            .baAssocJackID = 0x01,
	},
}, {
	/* Table B-14: MIDI Adapter Class-specific Bulk IN Endpoint
	 * Descriptor
	 */
	.head = {
            .bLength = sizeof(struct usb_midi_endpoint_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_ENDPOINT,
            .bDescriptorSubType = USB_MIDI_SUBTYPE_MS_GENERAL,
            .bNumEmbMIDIJack = 1,
	},
	.jack[0] = {
            .baAssocJackID = 0x03,
	},
} };

/*
 * Standard endpoint descriptors
 */
static const struct usb_endpoint_descriptor bulk_endp[] = {{
	/* Table B-11: MIDI Adapter Standard Bulk OUT Endpoint Descriptor */
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 0x40,
	.bInterval = 0x00,

	.extra = &midi_bulk_endp[0],
	.extralen = sizeof(midi_bulk_endp[0])
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x81,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 0x40,
	.bInterval = 0x00,

	.extra = &midi_bulk_endp[1],
	.extralen = sizeof(midi_bulk_endp[1])
} };

/*
 * Table B-4: MIDI Adapter Class-specific AC Interface Descriptor
 */
static const struct {
	struct usb_audio_header_descriptor_head header_head;
	struct usb_audio_header_descriptor_body header_body;
} __attribute__((packed)) audio_control_functional_descriptors = {
	.header_head = {
            .bLength = sizeof(struct usb_audio_header_descriptor_head) +
            1 * sizeof(struct usb_audio_header_descriptor_body),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_AUDIO_TYPE_HEADER,
            .bcdADC = 0x0100,
            .wTotalLength =
                sizeof(struct usb_audio_header_descriptor_head) +
                1 * sizeof(struct usb_audio_header_descriptor_body),
            .binCollection = 1,
	},
	.header_body = {
            .baInterfaceNr = 0x01,
	},
};

/*
 * Table B-3: MIDI Adapter Standard AC Interface Descriptor
 */
static const struct usb_interface_descriptor audio_control_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_AUDIO_SUBCLASS_CONTROL,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.extra = &audio_control_functional_descriptors,
	.extralen = sizeof(audio_control_functional_descriptors)
} };

/*
 * Class-specific MIDI streaming interface descriptor
 */
static const struct {
	struct usb_midi_header_descriptor header;
	struct usb_midi_in_jack_descriptor in_embedded;
	struct usb_midi_in_jack_descriptor in_external;
	struct usb_midi_out_jack_descriptor out_embedded;
	struct usb_midi_out_jack_descriptor out_external;
} __attribute__((packed)) midi_streaming_functional_descriptors = {
	/* Table B-6: Midi Adapter Class-specific MS Interface Descriptor */
	.header = {
            .bLength = sizeof(struct usb_midi_header_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MS_HEADER,
            .bcdMSC = 0x0100,
            .wTotalLength = sizeof(midi_streaming_functional_descriptors),
	},
	/* Table B-7: MIDI Adapter MIDI IN Jack Descriptor (Embedded) */
	.in_embedded = {
            .bLength = sizeof(struct usb_midi_in_jack_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK,
            .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,
            .bJackID = 0x01,
            .iJack = 0x00,
	},
	/* Table B-8: MIDI Adapter MIDI IN Jack Descriptor (External) */
	.in_external = {
            .bLength = sizeof(struct usb_midi_in_jack_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK,
            .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL,
            .bJackID = 0x02,
            .iJack = 0x00,
	},
	/* Table B-9: MIDI Adapter MIDI OUT Jack Descriptor (Embedded) */
	.out_embedded = {
            .head = {
                .bLength = sizeof(struct usb_midi_out_jack_descriptor),
                .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK,
                .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,
                .bJackID = 0x03,
                .bNrInputPins = 1,
            },
            .source[0] = {
                .baSourceID = 0x02,
                .baSourcePin = 0x01,
            },
            .tail = {
                .iJack = 0x00,
            }
	},
	/* Table B-10: MIDI Adapter MIDI OUT Jack Descriptor (External) */
	.out_external = {
            .head = {
                .bLength = sizeof(struct usb_midi_out_jack_descriptor),
                .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK,
                .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL,
                .bJackID = 0x04,
                .bNrInputPins = 1,
            },
            .source[0] = {
                .baSourceID = 0x01,
                .baSourcePin = 0x01,
            },
            .tail = {
                .iJack = 0x00,
            },
	},
};

/*
 * Table B-5: MIDI Adapter Standard MS Interface Descriptor
 */
static const struct usb_interface_descriptor midi_streaming_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_AUDIO_SUBCLASS_MIDISTREAMING,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = bulk_endp,

	.extra = &midi_streaming_functional_descriptors,
	.extralen = sizeof(midi_streaming_functional_descriptors)
} };

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = audio_control_iface,
}, {
	.num_altsetting = 1,
	.altsetting = midi_streaming_iface,
} };

/*
 * Table B-2: MIDI Adapter Configuration Descriptor
 */
static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0, /* can be anything, it is updated automatically
			      when the usb code prepares the descriptor */
	.bNumInterfaces = 2, /* control and data */
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80, /* bus powered */
	.bMaxPower = 0x32,

	.interface = ifaces,
};

/* SysEx identity message, preformatted with correct USB framing information */
const uint8_t sysex_identity[] = {
	0x04,	/* USB Framing (3 byte SysEx) */
	0xf0,	/* SysEx start */
	0x7e,	/* non-realtime */
	0x00,	/* Channel 0 */
	0x04,	/* USB Framing (3 byte SysEx) */
	0x7d,	/* Educational/prototype manufacturer ID */
	0x66,	/* Family code (byte 1) */
	0x66,	/* Family code (byte 2) */
	0x04,	/* USB Framing (3 byte SysEx) */
	0x51,	/* Model number (byte 1) */
	0x19,	/* Model number (byte 2) */
	0x00,	/* Version number (byte 1) */
	0x04,	/* USB Framing (3 byte SysEx) */
	0x00,	/* Version number (byte 2) */
	0x01,	/* Version number (byte 3) */
	0x00,	/* Version number (byte 4) */
	0x05,	/* USB Framing (1 byte SysEx) */
	0xf7,	/* SysEx end */
	0x00,	/* Padding */
	0x00,	/* Padding */
};


/* Bundle all state into a single struct to make debugging easier.
   This has to be a global variable because libopencm3 callbacks do
   not have a context pointer.  There is only a single usb
   configuration all contained in this file, so that's ok. */
struct usb_midi_state {
    uint8_t usbd_control_buffer[128];
    const char * usb_strings[3];
    usbd_device *usbd_dev;
    char serial_hex[25];
    uint8_t tx_buf_sema;
};
struct usb_midi_state usb_midi_state;


/* Nomenclature uses "host view"

   read:  get data from application, transmit it to USB bus (USB host IN)
   write: receive data from USB bus (USB host OUT), provide it to application

   Application hooks are just global functions, since state is global
   anyway due to lack of context pointer in libopencm3 callbacks.

*/
uint32_t usb_midi_read(uint8_t *buf, uint32_t room);
void     usb_midi_write(const uint8_t *buf, uint32_t room);


/* Poll output state machine. */
static void usbmidi_poll_data_tx(struct usb_midi_state *s) {
    if (s->tx_buf_sema > 0) return; // busy
    uint8_t tx_buf[64];
    uint32_t len = usb_midi_read(tx_buf, sizeof(tx_buf));
    if (len) {
        /* Send len bytes from tx_buf */
        s->tx_buf_sema++;
        usbd_ep_write_packet(s->usbd_dev, 0x81, tx_buf, len);
    }
}
/* USB transmission complete. */
static void usbmidi_tx_cb(usbd_device *usbd_dev, uint8_t ep) {
    struct usb_midi_state *s = &usb_midi_state;
    if (usbd_dev != s->usbd_dev) return;
    s->tx_buf_sema--;
    /* Immediately check if there's more. */
    usbmidi_poll_data_tx(s);
}
/* USB has data available.  Push it to state machine + poll TX. */
static void usbmidi_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
    struct usb_midi_state *s = &usb_midi_state;
    if (usbd_dev != s->usbd_dev) return;
    uint8_t rx_buf[64];
    uint32_t len = usbd_ep_read_packet(s->usbd_dev, 0x01, rx_buf, sizeof(rx_buf));
    usb_midi_write(rx_buf, len);
    /* Immediately check if there's anything available. */
    usbmidi_poll_data_tx(s);
}
/* Called by application. */
void usb_midi_poll(void) {
    struct usb_midi_state *s = &usb_midi_state;
    usbd_poll(s->usbd_dev);
    usbmidi_poll_data_tx(s);
}


static void usbmidi_set_config(usbd_device *usbd_dev, uint16_t wValue) {
    usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbmidi_rx_cb);
    usbd_ep_setup(usbd_dev, 0x81, USB_ENDPOINT_ATTR_BULK, 64, usbmidi_tx_cb);
}


#if 0 // Framing example from orig code
	char buf[4] = { 0x08, /* USB framing: virtual cable 0, note on */
			0x80, /* MIDI command: note on, channel 1 */
			60,   /* Note 60 (middle C) */
			64,   /* "Normal" velocity */
	};
	buf[0] |= pressed;
	buf[1] |= pressed << 4;
#endif





void usb_midi_init(void) {

    struct usb_midi_state *s = &usb_midi_state;

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_AFIO);

    for (uint32_t i=0; i<12; i++) {
        // FIXME: share this code with smpacket.h
        static const uint8_t digit[] = "0123456789abcdef";
        uint8_t b = ((uint8_t*)0x1FFFF7E8)[i];
        uint8_t hi = digit[(b >> 4) & 0x0f];
        uint8_t lo = digit[(b >> 0) & 0x0f];
        s->serial_hex[2*i] = hi;
        s->serial_hex[2*i+1] = lo;
    }

    s->usb_strings[0] = flash_string(_config.manufacturer, "Zwizwa");
    s->usb_strings[1] = flash_string(_config.product,      "MIDI");
    s->usb_strings[2] = flash_string(_config.serial,       s->serial_hex);

    s->usbd_dev = usbd_init(
        HW_USB_DRIVER,
        &dev, &config,
        s->usb_strings, 3,
        s->usbd_control_buffer, sizeof(s->usbd_control_buffer));

    usbd_register_set_config_callback(s->usbd_dev, usbmidi_set_config);


}



