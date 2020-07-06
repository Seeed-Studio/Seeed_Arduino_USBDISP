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
static __attribute__((__aligned__(4))) uint8_t frame_buff[ TFT_WIDTH * TFT_HEIGHT * 2 ] ;
#endif
static int frame_sz = 0; // in bytes

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

static int rle_len, rle_pos;

static int bitblt_append_data(int rle, uint8_t* dptr, int sz) {
	static int rle_comn = 0;
	static uint16_t the_pixel;
	int i, r = 0;

	if (!rle) {
		#if USE_FRAME_BUFF
		memcpy(&frame_buff[frame_sz], dptr, sz);
		frame_sz += sz;
		#else

		for (i = 0; i < sz; i++) {
			if (++frame_sz & 0x1U) {
				the_pixel  = dptr[i] << 0;
			} else {
				the_pixel |= dptr[i] << 8;
				tft.pushColor(the_pixel);
			}
		}

		#endif
		return sz;
	}

	for (i = 0; i < sz; i++) {
		if (rle_pos >= rle_len) {
			// rle header char
			rle_len = ((dptr[i] & RPUSBDISP_RLE_BLOCKFLAG_SIZE_BIT) + 1) << 1;
			rle_comn = !!(dptr[i] & RPUSBDISP_RLE_BLOCKFLAG_COMMON_BIT);

			rle_pos = 0;
			if (rle_comn) {
				// common section only have a single color
				// 2 Bytes (RGB565)
				rle_pos = rle_len - 2;
			}
			continue;
		}

		// rle content char
		++rle_pos;

		if (rle_comn) {
			if (rle_pos >= rle_len) {
				// upper color part
				the_pixel |= dptr[i] << 8;

				for (int k = 0; k < rle_len >> 1; k++) {
					#if USE_FRAME_BUFF
					frame_buff[frame_sz++] = (uint8_t)(the_pixel >> 0);
					frame_buff[frame_sz++] = (uint8_t)(the_pixel >> 8);
					#else
					tft.pushColor(the_pixel);
					#endif
					r += rle_len;
				}
			} else {
				// lower color part
				the_pixel = dptr[i] << 0;
			}
			continue;
		}

		// normal color part
		#if USE_FRAME_BUFF
		frame_buff[frame_sz++] = dptr[i];
		#else
		tft.pushColors(&dptr[i], 1);
		#endif
		r++;
	}
	return r;
}

static int parse_bitblt(int ep, int rle) {
	static rpusbdisp_disp_bitblt_packet_t bb[1];
	unsigned load;
	int sz;

	*bb = ucmd->bblt;

	tft.setAddrWindow(bb->x, bb->y, bb->width, bb->height);
	tft.startWrite();

	frame_sz = 0;

	rle_len = rle_pos = 0;

	load = bb->width * bb->height * 2/*RGB565*/;

	if (bulkpos > sizeof ucmd->bblt) {
		sz = bulkpos - sizeof ucmd->bblt;
		sz = bitblt_append_data(rle, &bulkbuf[sizeof ucmd->bblt], sz);
		load -= sz;
	}

	for (; load;) {
		if ((sz = USBDevice.available(ep)) == 0) {
			continue;
		}

		sz = min(sz, EPX_SIZE - bulkpos);
		bulkpos += USBDevice.recv(ep, bulkbuf + bulkpos, sz);
		if (load > bulkpos && bulkpos < EPX_SIZE) {
			continue;
		}
		bulkpos = 0;

		if (!rle && *bulkbuf != RPUSBDISP_DISPCMD_BITBLT
		||   rle && *bulkbuf != RPUSBDISP_DISPCMD_BITBLT_RLE
		) {
			printf("BB SYNC ERR #0\r\n");
			break;
		}

		// skip header
		--sz;
		sz = bitblt_append_data(rle, &bulkbuf[1], sz);

		load -= sz;
	}

	#if USE_FRAME_BUFF
	for (sz = 0; sz < frame_sz; sz += bb->width << 1) {
		/*
		 * pushColors argument swap = true is ineffective.
		 */
		for (int i = 0; i < (bb->width << 1); i += 2) {
			uint8_t* ptr = &frame_buff[sz + i];
			uint8_t pix = *ptr;
			*ptr = ptr[1];
			ptr[1] = pix;
		}
		tft.pushColors((uint16_t*)&frame_buff[sz], bb->width, false);
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

	// Screen image are out of sync from power up
	USBDevice.send(ep + 1, usbdisp_status, sizeof usbdisp_status);
	return 0;
}

int USBDISP_::eventRun(void) {
	uint32_t av;
	int mode_rle;

	while ((av = USBDevice.available(pluggedEndpoint))) {
		av = min(av, EPX_SIZE);
		USBDevice.recv(pluggedEndpoint, bulkbuf + bulkpos, av);
		bulkpos += av;

		if (!(*bulkbuf & RPUSBDISP_CMD_FLAG_START)) {
			bulkpos = 0;
			printf("Parse ERR#1\r\n");
		}

		mode_rle = 0;
		switch (*bulkbuf & RPUSBDISP_CMD_MASK) {
		case RPUSBDISP_DISPCMD_NOPE:
		case RPUSBDISP_DISPCMD_FILL:
			break;

		case RPUSBDISP_DISPCMD_BITBLT_RLE:
			mode_rle = 1;
			/* intentional fall-through */
		case RPUSBDISP_DISPCMD_BITBLT:
			if (bulkpos < sizeof ucmd->bblt) continue;
			parse_bitblt(pluggedEndpoint, mode_rle);
			break;

		case RPUSBDISP_DISPCMD_RECT:
		case RPUSBDISP_DISPCMD_COPY_AREA:
			break;

		default:
			printf("Parse ERR#2\r\n");
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
	// Initial notify, not works yet
	// Screen image are out of sync from power up
	USBDevice.send(pluggedEndpoint + 1, usbdisp_status, sizeof usbdisp_status);

	tft.init();
	// Plot with 90 deg. clockwise rotation
	// Required a 320x240 screen, not a 240x320.
	tft.setRotation(1);
	return 0;
}

#endif /* if defined(USBCON) */
