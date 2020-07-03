/*
   Copyright (c) 2020, Seeed Studio

   Permission to use, copy, modify, and/or distribute this software for
   any purpose with or without fee is hereby granted, provided that the
   above copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
   BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
   OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
   ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
   SOFTWARE.
 */

#include "USBDISP.h"
#include "rpusbdisp_protocol.h"

#include "SPI.h"
#include "TFT_eSPI.h"

// Use hardware SPI
TFT_eSPI tft = TFT_eSPI();

#if defined(USBCON)

static rpusbdisp_status_normal_packet_t usbdisp_status[1] = {
	0x00,  // Packet Type
	RPUSBDISP_DISPLAY_STATUS_DIRTY_FLAG,
	RPUSBDISP_TOUCH_STATUS_NO_TOUCH,
	0x0U,
	0x0U,
};

USBDISP_& USBDISP()
{
	static USBDISP_ obj;
	return obj;
}

int USBDISP_::getInterface(uint8_t* interfaceCount)
{
	*interfaceCount += 1; // uses 1
	USBDISPDescriptor dispInterface = {
		D_INTERFACE(pluggedInterface, 2, USB_INTERFACE_DISP_CLASS, USB_INTERFACE_DISP_SUBCLASS, USB_INTERFACE_PROTOCOL_NONE),
		D_ENDPOINT(USB_ENDPOINT_OUT(pluggedEndpoint),    USB_ENDPOINT_TYPE_BULK,      EPX_SIZE, 0x01),
		D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint + 1), USB_ENDPOINT_TYPE_INTERRUPT, 32,       0x05),
	};
	return USBDevice.sendControl(&dispInterface, sizeof(dispInterface));
}

int USBDISP_::getDescriptor(USBSetup& setup)
{
	// Check if this is a USBDISP Class Descriptor request
	if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) { return 0; }

	// In a USBDISP Class Descriptor wIndex cointains the interface number
	if (setup.wIndex != pluggedInterface) { return 0; }

	int total = 0;
	/*
	USBDevice.packMessages(true);
	int res = USBDevice.sendControl(thisDesc, thisDescLength);
	if (res == -1)
		return -1;
	total += res;
	USBDevice.packMessages(false);
	*/
	return total;
}

#define USE_FRAME_BUFF 1
#if USE_FRAME_BUFF
static uint8_t frame_buff[ TFT_WIDTH * TFT_HEIGHT * 2 ];
static int frame_sz = 0;
#endif
static union {
	#define LINEBUF_SZ (TFT_HEIGHT << 1)
	uint8_t epbuf[LINEBUF_SZ];
	rpusbdisp_disp_packet_header_t   hdr;
	rpusbdisp_disp_fill_packet_t     fill;
	rpusbdisp_disp_bitblt_packet_t   bblt;
	rpusbdisp_disp_fillrect_packet_t rect;
	rpusbdisp_disp_copyarea_packet_t copy;
} ucmd[1];

static uint8_t* const bulkbuf = &ucmd->epbuf[0];
static int bulkpos = 0;

static int parse_bitblt(int ep) {
	static rpusbdisp_disp_bitblt_packet_t bb[1];
	unsigned load;

	*bb = ucmd->bblt;

	tft.setAddrWindow(bb->x, bb->y, bb->width, bb->height);
	tft.startWrite();

	int sz;

	#if USE_FRAME_BUFF
	frame_sz = 0;
	#endif

	load = bb->width * bb->height * 2/*RGB565*/;
	if (bulkpos > sizeof ucmd->bblt) {
		sz = bulkpos - sizeof ucmd->bblt;
		#if USE_FRAME_BUFF
		memcpy(&frame_buff[frame_sz], &bulkbuf[sizeof ucmd->bblt], sz);
		frame_sz += sz;
		#else
		tft.pushColors(&bulkbuf[sizeof ucmd->bblt], sz);
		#endif
		load -= sz;
	}

	for (; load;) {
		if ((sz = USBDevice.available(ep)) == 0) {
			continue;
		}

		sz = min(sz, EPX_SIZE);
		sz = USBDevice.recv(ep, bulkbuf, sz);
		if (*bulkbuf != RPUSBDISP_DISPCMD_BITBLT) {
			printf("BITBLT data sync error 0\r\n");
			break;
		}
		// skip header
		--sz;
		#if USE_FRAME_BUFF
		memcpy(&frame_buff[frame_sz], &bulkbuf[1], sz);
		frame_sz += sz;
		#else
		tft.pushColors(&bulkbuf[1], sz);
		#endif

		load -= sz;
	}

	#if USE_FRAME_BUFF
	for (sz = 0; sz < frame_sz; sz += bb->width) {
		tft.pushColors(&frame_buff[sz], bb->width);
		if (USBDevice.available(ep)) {
			// This ignore remain colors
			// make USB Host more stable
			// break;
		}
	}
	#endif
	tft.endWrite();

	if (bb->header.cmd_flag & RPUSBDISP_CMD_FLAG_CLEARDITY) {
		usbdisp_status->display_status &= ~RPUSBDISP_DISPLAY_STATUS_DIRTY_FLAG;
	}
	return 0;
}

int USBDISP_::eventRun(void) {
	uint32_t av;

	while ((av = USBDevice.available(pluggedEndpoint))) {
		av = min(av, EPX_SIZE);
		USBDevice.recv(pluggedEndpoint, bulkbuf + bulkpos, av);
		bulkpos += av;

		if (!(*bulkbuf & RPUSBDISP_CMD_FLAG_START)) {
			bulkpos = 0;
			printf("Parse error cmd start\r\n");
		}

		switch (*bulkbuf & RPUSBDISP_CMD_MASK) {
		case RPUSBDISP_DISPCMD_NOPE:
		case RPUSBDISP_DISPCMD_FILL:
			break;

		case RPUSBDISP_DISPCMD_BITBLT:
			if (bulkpos < sizeof ucmd->bblt) continue;
			parse_bitblt(pluggedEndpoint);
			break;

		case RPUSBDISP_DISPCMD_RECT:
		case RPUSBDISP_DISPCMD_COPY_AREA:
		case RPUSBDISP_DISPCMD_BITBLT_RLE:
			break;

		default:
			printf("Parse error cmd start 2\r\n");
			bulkpos = 0;
			break;
		}
	}
	return 0;
}

bool USBDISP_::setup(USBSetup& setup)
{
	if (pluggedInterface != setup.wIndex) {
		return false;
	}

	uint8_t request = setup.bRequest;
	uint8_t requestType = setup.bmRequestType;

	if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
	{
	}

	if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE)
	{
	}

	return false;
}

USBDISP_::USBDISP_(void) : PluggableUSBModule(2, 1, epType), idle(1)
{
	epType[0] = USB_ENDPOINT_TYPE_BULK      | USB_ENDPOINT_OUT(0);
	epType[1] = USB_ENDPOINT_TYPE_INTERRUPT | USB_ENDPOINT_IN(0);
	PluggableUSB().plug(this);
}

int USBDISP_::begin(void)
{
	// Initial notify
	// Screen image are out of sync from power up
	USBDevice.send(pluggedEndpoint + 1, usbdisp_status, sizeof usbdisp_status);

	tft.init();
	// Plot with 90 deg. clockwise rotation
	// Required a 320x240 screen, not a 240x320.
	tft.setRotation(1);
	return 0;
}

#endif /* if defined(USBCON) */
