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

#if defined(USBCON)

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

int USBDISP_::eventRun(void) {
	return 0;
}

uint8_t USBDISP_::getShortName(char *name)
{
	name[0] = 'H';
	name[1] = 'I';
	name[2] = 'D';
	name[3] = 'A' + pluggedInterface;
	name[4] = 'A' + pluggedEndpoint;
	return 5;
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
	return 0;
}

#endif /* if defined(USBCON) */
