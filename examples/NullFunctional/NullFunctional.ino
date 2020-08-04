/*
  NullFunctional.ino

  This code is a null functional example,
  will only enumerate a USB Display Device.

  Note:
    The USBDISP().begin(bool reverse, bool usermode) function has two parameters.
    In our demo, the default setting is USBDISP().begin(true)
    If you want to parse drawing function in usermode-sdk or python-demo(seeed-linux-usbdisp),
    you need to set it as USBDISP().begin(true, true)
*/

#define SERHD SERIAL_PORT_HARDWARE
#include "USBDISP.h"

void setup() {
	// Construct the singleton object
	(void)USBDISP();

	// open the serial port
	SERHD.begin(115200);
	SERHD.println("WIO Termianl USB Display");

	SerialUSB.begin(115200);
	/*
	while (!SerialUSB) {
		delay(1);
	}
	SerialUSB.println("WIO Termianl USB Display");
	*/

	// Should after SerialUSB ready.
	// Please see the note above.
	USBDISP().begin(true);
}

void loop() {
	// SERHD.print(".");
	// SerialUSB.print(".");
	USBDISP().eventRun();
}
