/*
  NullFunctional.ino

  This code is a null functional example,
  will only enumerate a USB Display Device.

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
	USBDISP().begin(true);
}

void loop() {
	// SERHD.print(".");
	// SerialUSB.print(".");
	USBDISP().eventRun();
}
