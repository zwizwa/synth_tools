/* USB Midi port
   Cobbled together from old CDC ACM code and:
   https://duckduckgo.com/?q=USB_MIDI_SUBTYPE_MIDI_OUT_JACK+opencm3&t=newext&atb=v376-6&ia=web
*/

#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/audio.h>
#include <libopencm3/usb/midi.h>

#include "generic.h"
#include "hw.h"




static const struct usb_device_descriptor descriptor = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_AUDIO,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength = 
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1, 
	 }
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors)
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes cdcacm_control_request(
    usbd_device *usbd_dev,
    struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
    usbd_control_complete_callback *complete)
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch(req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return 1;
		}
	case USB_CDC_REQ_SET_LINE_CODING: 
		if(*len < sizeof(struct usb_cdc_line_coding))
			return 0;

		return 1;
	}
	return 0;
}


void midi_set_config_with_callbacks(usbd_device *usbd_dev, void *rx_cb, void *tx_cb) {
	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, tx_cb);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);


}
/* Serial data I/O */
uint8_t rx_buf[64];
uint8_t tx_buf[64];
uint32_t tx_buf_sema;

usbd_device *usb_midi;
static const char * usb_strings[3];
char serial_hex[25];



/* Send len bytes from tx_buf */
static void data_tx(uint32_t len) {
    tx_buf_sema++;
    usbd_ep_write_packet(usb_midi, 0x82, tx_buf, len);
}
/* Poll output state machine. */
static void poll_data_tx(void) {
    if (tx_buf_sema > 0) return; // busy
    uint32_t len = io->read(tx_buf, sizeof(tx_buf));
    if (len) data_tx(len);
}
/* USB transmission complete. */
static void data_tx_cb(usbd_device *usbd_dev, uint8_t ep) {
    if (usbd_dev != usb_midi) return;
    tx_buf_sema--;
    /* Immediately check if there's more. */
    poll_data_tx();
}
/* Receive bytes into rx_buf, return number. */
static uint32_t data_rx(void) {
    return usbd_ep_read_packet(usb_midi, 0x01, rx_buf, sizeof(rx_buf));
}
/* USB has data available.  Push it to state machine + poll TX. */
static void data_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
    if (usbd_dev != usb_midi) return;
    uint32_t len = data_rx();
    io->write(rx_buf, len);
    /* Immediately check if there's anything available. */
    poll_data_tx();
}
/* Stored in Flash, fixed location.  See stm32f1.ld */
// extern const struct gdbstub_service service;

// Instantiate the service struct.  This pulls in all dependencies.
uint32_t bl_read(uint8_t *buf, uint32_t room) {
    return 0;
}
void bl_write(const uint8_t *buf, uint32_t room) {
}
extern unsigned _ebss, _stack;

const struct gdbstub_service service SERVICE_SECTION = {
    .rsp_io = { .read = bl_read, .write = bl_write },
    .io = &io,
    .stack_lo = &_ebss,
    .stack_hi = &_stack,
};


const struct gdbstub_io *io = &service.rsp_io;
void bootloader_io_reset(void) {
    //FIXME: linker dep issues
    //io = &service.rsp_io;
}

static void midi_set_config(usbd_device *usbd_dev, uint16_t wValue) {
    (void)wValue;
    midi_set_config_with_callbacks(usbd_dev, data_rx_cb, data_tx_cb);
    usbd_register_reset_callback(usbd_dev, bootloader_io_reset);
}
usbd_device *usb_midi_init(void *set_config,
                           const char * const *usb_strings) {
	usbd_device *usbd_dev = 
		usbd_init(HW_USB_DRIVER, &descriptor, &config,
                          (const char **)usb_strings,
			  3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, set_config);
	return usbd_dev;
}

void usb_init(void) {
    for (uint32_t i=0; i<12; i++) {
        // FIXME: share this code with smpacket.h
        static const uint8_t digit[] = "0123456789abcdef";
        uint8_t b = ((uint8_t*)0x1FFFF7E8)[i];
        uint8_t hi = digit[(b >> 4) & 0x0f];
        uint8_t lo = digit[(b >> 0) & 0x0f];
        serial_hex[2*i] = hi;
        serial_hex[2*i+1] = lo;
    }
    rcc_periph_clock_enable(RCC_GPIOA);
#ifdef RCC_AFIO
    rcc_periph_clock_enable(RCC_AFIO);
#endif
    usb_strings[0] = flash_string(_config.manufacturer, "Zwizwa");
    usb_strings[1] = flash_string(_config.product,      "Bootloader");
    usb_strings[2] = flash_string(_config.serial,       serial_hex);

    usb_midi = usb_midi_init(midi_set_config, usb_strings);
}
void usb_poll(void) {
}
