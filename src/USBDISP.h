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

#if defined(USBCON)

// DISP 'Driver'
#define USB_INTERFACE_DISP_CLASS     0xFF	/* Vendor Specific */
#define USB_INTERFACE_DISP_SUBCLASS  0xFF	/* Vendor Specific */
#define USB_INTERFACE_PROTOCOL_NONE  0

typedef struct 
{
  InterfaceDescriptor if_desc;
  EndpointDescriptor  ep_dp;	/* Display Endpoint */
  EndpointDescriptor  ep_st;	/* Status  Endpoint */
} USBDISPDescriptor;

class USBDISP_ : public PluggableUSBModule
{
public:
  USBDISP_(void);
  int begin(bool reverse = false);
  int eventRun(void);
protected:
  // Implementation of the PluggableUSBModule
  int getInterface(uint8_t* interfaceCount);
  int getDescriptor(USBSetup& setup);
  bool setup(USBSetup& setup);

private:
  uint32_t epType[2];

  uint8_t idle;
};

// Replacement for global singleton.
// This function prevents static-initialization-order-fiasco
// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
USBDISP_& USBDISP();

#endif // USBCON

#endif // __USBDISP_H__
