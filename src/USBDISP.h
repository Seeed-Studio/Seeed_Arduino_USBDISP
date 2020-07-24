/*
  Copyright (c) 2015, Arduino LLC
  Original code (pre-library): Copyright (c) 2011, Peter Barrett

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

#ifndef __USBDISP_H__
#define __USBDISP_H__

#include <stdint.h>
#include <Arduino.h>
#include "USB/PluggableUSB.h"
#include "rpusbdisp_protocol.h"
#include "TFT_eSPI.h"

#if defined(USBCON)

// DISP 'Driver'
#define USB_INTERFACE_DISP_CLASS	0xFF	/* Vendor Specific */
#define USB_INTERFACE_DISP_SUBCLASS	0xFF	/* Vendor Specific */
#define USB_INTERFACE_PROTOCOL_NONE	0

typedef struct 
{
	InterfaceDescriptor if_desc;
	EndpointDescriptor	ep_dp;	/* Display Endpoint */
	EndpointDescriptor	ep_st;	/* Status	Endpoint */
} USBDISPDescriptor;

class USBDISP_ : public PluggableUSBModule
{
public:
	static bool parseDrawFunction;
	
	USBDISP_(void);
	int begin(bool reverse = false, bool usermode = false);
	int eventRun(void);
protected:
	// Implementation of the PluggableUSBModule
	int getInterface(uint8_t* interfaceCount);
	int getDescriptor(USBSetup& setup);
	bool setup(USBSetup& setup);

	// Save remain unprocessed data.
	unsigned usbBackAvail(void);
	uint32_t usbBackPeek(void);
	uint32_t usbBackRead(void *data, uint32_t len);

	int parseBitblt(int rle);
	int bitbltAppendData(int rle, uint8_t* dptr, int sz);

	int parseFill(void);
	int parseFillRect(void);
	int parseCopyArea(void);
private:
	uint32_t epType[2];

	/*
	 * #.1 USE_FRAME_BUFF=1 not stable.
	 *     RAM left for backbuf are too small.
	 */
	#define USE_FRAME_BUFF 0
	#if USE_FRAME_BUFF
	__attribute__((__aligned__(4))) uint8_t frame_buff[ TFT_WIDTH * TFT_HEIGHT * 2 ];
	#endif
	volatile int frame_pos; // in bytes
	int frame_sz; // in bytes

	union {
		#define LINEBUF_SZ (TFT_HEIGHT << 1)
		uint8_t epbuf[LINEBUF_SZ];
		rpusbdisp_disp_packet_header_t   hdr;
		rpusbdisp_disp_fill_packet_t     fill;
		rpusbdisp_disp_bitblt_packet_t   bblt;
		rpusbdisp_disp_fillrect_packet_t rect;
		rpusbdisp_disp_copyarea_packet_t copy;
	} ucmd[1];
	uint8_t* const bulkbuf;
	volatile int bulkpos;

	RingBufferN<32768> backbuf;

	volatile int rle_len, rle_pos;
};

// Replacement for global singleton.
// This function prevents static-initialization-order-fiasco
// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
USBDISP_& USBDISP();

#endif // USBCON

#endif // __USBDISP_H__
