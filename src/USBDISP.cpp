/*
   Copyright (c) 2020, Seeed Studio.  All right reserved.

   Arduino Setting:
      CPU Speed: 200 MHz
      Optimize : Small (-Os)
      USB Stack: Arduino
      Debug    : On // printf message will output to RPI compatible UART

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
#include "SPI.h"

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

uint32_t USBDISP_::usbBackRead(void *data, uint32_t len) {
	uint8_t& ep = pluggedEndpoint;
	uint8_t* dptr = (uint8_t*)data;
	unsigned sz;
	int i = 0;

	if ((sz = backbuf.available())) {
		for (; i < len && i < sz; i++) {
			dptr[i] = backbuf.read_char();
		}
	}

	/*
	sz = USBDevice.available(ep);
	if (sz > len - i)
		sz = len - i;
	*/
	sz = USBDevice.recv(ep, &dptr[i], len - i);
	return sz + i;
}

uint32_t USBDISP_::usbBackPeek(void) {
	uint8_t& ep = pluggedEndpoint;
	// #.2 Receive USB DATA when screen drawing,
	// or else will missing USB DATA.
	uint32_t av = USBDevice.available(ep);
	for (int i = 0; i < av; i++) {
		uint8_t c = USBDevice.recv(ep);
		backbuf.store_char(c);
	}
	return av;
}

unsigned USBDISP_::usbBackAvail(void) {
	uint8_t& ep = pluggedEndpoint;
	return backbuf.available() + USBDevice.available(ep);
}

// return pixel data bytes processed,
// unprocessed/next-action data will save into backbuf.
int USBDISP_::bitbltAppendData(int rle, uint8_t* dptr, int sz) {
	static int rle_comn = 0;
	static uint16_t the_pixel;
	int i, rsz; /* required size */

	if (!rle) {
		rsz = (frame_pos + sz > frame_sz)? frame_sz - frame_pos: sz;

		#if USE_FRAME_BUFF
		memcpy(&frame_buff[frame_pos], dptr, rsz);
		frame_pos += rsz;
		i = rsz;
		#else

		for (i = 0; i < rsz; i++) {
			if (++frame_pos & 0x1U) {
				the_pixel  = dptr[i] << 0;
			} else {
				the_pixel |= dptr[i] << 8;
				tft.pushColor(the_pixel);
			}
		}
		#endif
		goto _remain;
	}

	rsz = 0;
	// decompress RLE DATA
	for (i = 0; i < sz; i++) {
		if (frame_pos >= frame_sz) {
			break;
		}

		if (rle_pos >= rle_len) {
			// rle header char
			rle_len = ((dptr[i] & RPUSBDISP_RLE_BLOCKFLAG_SIZE_BIT) + 1) << 1;
			rle_comn = !!(dptr[i] & RPUSBDISP_RLE_BLOCKFLAG_COMMON_BIT);

			rle_pos = 0;
			if (rle_comn) {
				// common section only have a single color
				// 2 Bytes (BGR565)
				rle_pos = rle_len - 2;
			}
			continue;
		}

		// rle content char
		++rle_pos;

		if (rle_comn) {
			if (rle_pos >= rle_len) {
				// compressed upper part color
				the_pixel |= dptr[i] << 8;

				#if USE_FRAME_BUFF
				for (int k = 0; k < rle_len >> 1; k++) {
					*(uint16_t*)&frame_buff[frame_pos] = the_pixel;
					frame_pos += 2;
				}
				#else
				tft.pushColor(the_pixel, rle_len >> 1);
				frame_pos += rle_len;
				#endif
				rsz += rle_len;
			} else {
				// compressed lower part color
				the_pixel = dptr[i] << 0;
			}
			continue;
		}

		// normal color part
		#if USE_FRAME_BUFF
		frame_buff[frame_pos++] = dptr[i];
		#else
		if (++frame_pos & 0x1U) {
			the_pixel  = dptr[i] << 0;
		} else {
			the_pixel |= dptr[i] << 8;
			tft.pushColor(the_pixel);
		}
		#endif
		rsz++;
	}

_remain:
	/* remain chars unprocessed */
	if (i < sz) {
		printf("M%d\r\n", sz - i);

		for (; i < sz; i++) {
			backbuf.store_char(dptr[i]);
		}
	}
	return rsz;
}

int USBDISP_::parseBitblt(int rle) {
	static rpusbdisp_disp_bitblt_packet_t bb[1];
	unsigned load;
	uint8_t& ep = pluggedEndpoint;
	/*
	#define TIMEOUT_MAX 10000000
	volatile unsigned timeout = TIMEOUT_MAX;
	*/
	int sz;

	*bb = ucmd->bblt;

	load = bb->width * bb->height * 2/*RGB565*/;
	frame_sz = load;
	frame_pos = 0;

	rle_len = rle_pos = 0;

	tft.setAddrWindow(bb->x, bb->y, bb->width, bb->height);
	tft.startWrite();

	if (bulkpos > sizeof ucmd->bblt) {
		sz = bulkpos - sizeof ucmd->bblt;
		sz = bitbltAppendData(rle, &bulkbuf[sizeof ucmd->bblt], sz);
		load -= sz;
	}

	for (bulkpos = 0; load;) {
		if ((sz = usbBackAvail()) == 0) {
			continue;
		}

		sz = min(sz, EPX_SIZE - bulkpos);
		bulkpos += usbBackRead(bulkbuf + bulkpos, sz);
		/*
		// Bug, will block the USB receiving
		if (--timeout != 0 && bulkpos < EPX_SIZE) {
			continue;
		}
		timeout = TIMEOUT_MAX;
		*/
		bulkpos = 0;

		if (!rle && *bulkbuf != RPUSBDISP_DISPCMD_BITBLT
		||   rle && *bulkbuf != RPUSBDISP_DISPCMD_BITBLT_RLE
		) {
			printf("BBE#0\r\n");
			break;
		}

		// skip header
		--sz;
		sz = bitbltAppendData(rle, &bulkbuf[1], sz);

		load -= sz;
	}

	#if USE_FRAME_BUFF
	for (sz = 0; sz < frame_pos; sz += bb->width << 1) {
		int i;
		/*
		 * pushColors 16 bit pixel MSB first,
		 * and argument swap = true is ineffective.
		 */
		for (i = 0; i < (bb->width << 1); i += 2) {
			uint8_t* ptr = &frame_buff[sz + i];
			uint8_t pix = *ptr;
			*ptr = ptr[1];
			ptr[1] = pix;
		}

		tft.pushColors((uint16_t*)&frame_buff[sz], bb->width, false);
		usbBackPeek();
	}
	#endif
	tft.endWrite();

	if (bb->header.cmd_flag & RPUSBDISP_CMD_FLAG_CLEARDITY) {
		usbdisp_status->display_status &= ~RPUSBDISP_DISPLAY_STATUS_DIRTY_FLAG;
	}

	// report display status
	USBDevice.send(ep + 1, usbdisp_status, sizeof usbdisp_status);
	return 0;
}

int USBDISP_::parseFill(void) {
	static rpusbdisp_disp_fill_packet_t Fill[1];
	uint8_t& ep = pluggedEndpoint;

	*Fill = ucmd->fill;

	tft.startWrite();
	tft.fillScreen(Fill->color_565);
	tft.endWrite();

	if (Fill->header.cmd_flag & RPUSBDISP_CMD_FLAG_CLEARDITY) {
		usbdisp_status->display_status &= ~RPUSBDISP_DISPLAY_STATUS_DIRTY_FLAG;
	}

	// report display status
	USBDevice.send(ep + 1, usbdisp_status, sizeof usbdisp_status);
	return 0;
}

int USBDISP_::parseFillRect(void) {
	static rpusbdisp_disp_fillrect_packet_t Rect[1];
	uint8_t& ep = pluggedEndpoint;

	*Rect = ucmd->rect;

	tft.startWrite();
	tft.fillRect(Rect->left, Rect->top, Rect->right - Rect->left, Rect->bottom - Rect->top, Rect->color_565);
	tft.endWrite();

	if (Rect->header.cmd_flag & RPUSBDISP_CMD_FLAG_CLEARDITY) {
		usbdisp_status->display_status &= ~RPUSBDISP_DISPLAY_STATUS_DIRTY_FLAG;
	}

	// report display status
	USBDevice.send(ep + 1, usbdisp_status, sizeof usbdisp_status);
	return 0;
}

int USBDISP_::parseCopyArea(void) {
	static rpusbdisp_disp_copyarea_packet_t CopyArea[1];
	uint8_t& ep = pluggedEndpoint;

	uint16_t *data;

	*CopyArea = ucmd->copy;

	data = (uint16_t *) malloc(CopyArea->width * CopyArea->height * sizeof(uint16_t));

	// Read a block of pixels to a data buffer, buffer is 16 bit and the array size must be at least w * h
    tft.readRect(CopyArea->sx, CopyArea->sy, CopyArea->width, CopyArea->height, (uint16_t *)data);

	tft.startWrite();
    // Write a block of pixels to the screen
    tft.pushRect(CopyArea->dx, CopyArea->dy, CopyArea->width, CopyArea->height, (uint16_t *)data);
	tft.endWrite();

	if (CopyArea->header.cmd_flag & RPUSBDISP_CMD_FLAG_CLEARDITY) {
		usbdisp_status->display_status &= ~RPUSBDISP_DISPLAY_STATUS_DIRTY_FLAG;
	}

	// report display status
	USBDevice.send(ep + 1, usbdisp_status, sizeof usbdisp_status);

	free(data);

	return 0;
}

int USBDISP_::eventRun(void) {
	uint32_t av;
	int mode_rle;

	/*
	 * #.3 cooperate with #.4, drop packet only has a broken header.
	 */
	/*
	if (bulkpos != 0)
		printf("bulkpos = %d\r\n",  bulkpos);
	*/
	bulkpos = 0;
_repeat:
	if /*while*/ ((av = usbBackAvail())) {
		av = min(av, EPX_SIZE - bulkpos);
		av = usbBackRead(bulkbuf + bulkpos, av);
		bulkpos += av;
		__DMB();

		if (!(*bulkbuf & RPUSBDISP_CMD_FLAG_START)) {
			printf("PE#1:%d\r\n", bulkpos);
			bulkpos = 0;
			goto _repeat;
		}
		mode_rle = 0;
		switch (*bulkbuf & RPUSBDISP_CMD_MASK) {
			case RPUSBDISP_DISPCMD_NOPE:
				break;
			case RPUSBDISP_DISPCMD_FILL:
				parseFill();
				break;
			case RPUSBDISP_DISPCMD_BITBLT_RLE:
				mode_rle = 1;
				/* intentional fall-through */
			case RPUSBDISP_DISPCMD_BITBLT:
				/* #.4 */
				if (bulkpos < sizeof ucmd->bblt) {
					printf("<%d\r\n", bulkpos);
					goto _repeat;
				}
				parseBitblt(mode_rle);
				break;
			case RPUSBDISP_DISPCMD_RECT:
				parseFillRect();
				break;
			case RPUSBDISP_DISPCMD_COPY_AREA:
				parseCopyArea();
				break;
			default:
				printf("PE#2:%d\r\n", *bulkbuf);
				break;
			}
		bulkpos = 0;
	}

	// Prevent missing USB DATA in the process
	// for other things doing.
	usbBackPeek();
	return 0;
}

bool USBDISP_::setup(USBSetup& setup)
{
	if (pluggedInterface != setup.wIndex) {
		return false;
	}
	return false;
}

USBDISP_::USBDISP_(void) : PluggableUSBModule(2, 1, epType),
                           bulkbuf(&ucmd->epbuf[0]), bulkpos(0)
{
	epType[0] = USB_ENDPOINT_TYPE_BULK      | USB_ENDPOINT_OUT(0);
	epType[1] = USB_ENDPOINT_TYPE_INTERRUPT | USB_ENDPOINT_IN(0);
	PluggableUSB().plug(this);
}

int USBDISP_::begin(bool reverse)
{
	// Initial notify, not works yet
	// Screen image are out of sync from power up
	USBDevice.send(pluggedEndpoint + 1, usbdisp_status, sizeof usbdisp_status);

	tft.init();
	// Plot with 90 deg. clockwise rotation
	// Required a 320x240 screen, not a 240x320.
	tft.setRotation(reverse? 3: 1);
	return 0;
}

#endif /* if defined(USBCON) */
