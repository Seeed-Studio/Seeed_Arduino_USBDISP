/*
 * WioTerminal_USBDisplayAndMouseControl.ino
 *
 * A demo for Wio Terminal to enumerate a USB Display Device and simulate mouse by buttons.
 * It is used as a raspberry PI monitor to display the desktop and mouse control.
 * Such as Mouse Up, Mouse Down, Mouse Left, Mouse Right,
 * Click the left mouse button, Click the right mouse button,
 * Up roll, Down roll and etc.
 *
 * Copyright (c) 2020 seeed technology co., ltd.
 * Author      : weihong.cai (weihong.cai@seeed.cc)
 * Create Time : July 2020
 * Change Log  :
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software istm
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS INcommInterface
 * THE SOFTWARE.
 *
 * Mouse Usage(in Wio Terminal):
 *    Press the WIO_5S_UP -----------------------> Mouse Up
 *    Press the WIO_5S_DOWN ---------------------> Mouse Down
 *    Press the WIO_5S_LEFT ---------------------> Mouse Left
 *    Press the WIO_5S_RIGHT --------------------> Mouse Right
 *    Press the WIO_5S_PRESS --------------------> Click the left mouse button
 *    Press the BUTTON_3 ------------------------> Click the right mouse button
 *    Press the BUTTON_2 ------------------------> Switch the speed of mouse moving
 *    Press the BUTTON_1 and WIO_5S_UP ----------> Up roll
 *    Press the BUTTON_1 and WIO_5S_DOWN --------> Down roll
 *
 * Some tips:
 * 1. If your PC unables to recognize USB device leading the Wio Terminal can’t work.
 *    You can solve this problem through updating your ArduinoCore.
 *    Please follow this: https://forum.seeedstudio.com/t/seeeduino-xiao-cant-simulate-keys-pressed/252819/6?u=weihong.cai
 *
 * You can know more about the Wio Terminal from: https://wiki.seeedstudio.com/Wio-Terminal-Getting-Started/
 * If you have any questions, you can leave a message on the forum: https://forum.seeedstudio.com
 */

#define SERHD SERIAL_PORT_HARDWARE
#include "USBDISP.h"
#include "Mouse.h"

/*----------------Define the button pins---------------------------*/
const int upButton          = WIO_5S_UP;
const int downButton        = WIO_5S_DOWN;
const int leftButton        = WIO_5S_LEFT;
const int rightButton       = WIO_5S_RIGHT;
const int mouseBttonLeft    = WIO_5S_PRESS;
const int mouseBttonRight   = BUTTON_3;
const int switchRangeButton = BUTTON_2;
const int mouseWheel        = BUTTON_1;

// Output range of X or Y movement; affects movement speed
int range = 6;

// The time record paramas
unsigned long _currentMillis;
unsigned long _previousMillis;
unsigned long timeDifference;
char          flag;

#define TIME_100_Ms 100
#define TIME_500_Ms 500

void setup() {
    // Initialize the buttons' inputs:
    pinMode(upButton,          INPUT);
    pinMode(downButton,        INPUT);
    pinMode(leftButton,        INPUT);
    pinMode(rightButton,       INPUT);
    pinMode(mouseWheel,        INPUT);
    pinMode(mouseBttonLeft,    INPUT);
    pinMode(mouseBttonRight,   INPUT);
    pinMode(switchRangeButton, INPUT);

    // Initialize mouse control:
    Mouse.begin();

    // Construct the singleton object
    (void)USBDISP();

    // Open the serial port
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
    // Read the button state:
    int upState                     = digitalRead(upButton);
    int downState                   = digitalRead(downButton);
    int rightState                  = digitalRead(rightButton);
    int leftState                   = digitalRead(leftButton);
    int clickState_mouseWheel       = digitalRead(mouseWheel);
    int clickState_mouseButtonLeft  = digitalRead(mouseBttonLeft);
    int clickState_mouseButtonRight = digitalRead(mouseBttonRight);
    int switchRangeButtonState      = digitalRead(switchRangeButton);

    // Calculate the movement distance based on the button states:
    int xDistance = leftState - rightState;
    int yDistance = upState   - downState;

/*------------------Mouse Move-------------------------------------*/
    // if X or Y is non-zero, move:
    if ((xDistance != 0) || (yDistance != 0)) {
        // use millis() to record current time(ms)
        _currentMillis = millis();
        timeDifference = _currentMillis - _previousMillis;
        if (timeDifference >= TIME_500_Ms) {
            flag = 2;
        }
        else if (timeDifference >= TIME_100_Ms) {
            flag = 1;
        }
        else {
            flag = 0;
        }

        switch (flag) {
            case 0:  Mouse.move(xDistance,        yDistance,        0); break;  // low speed
            case 1:  Mouse.move(0,                0,                0); break;  // stop
            case 2:  Mouse.move(xDistance*range,  yDistance*range,  0); break;  // high speed
            default: Mouse.move(0,                0,                0); break;  // stop
        }
    }
    else {
        _previousMillis = millis();
        flag = 0;
    }

/*-------------Mouse Button Left Click-----------------------------*/
    // if the mouse button left is pressed:
    if (clickState_mouseButtonLeft == LOW) {
        // if the mouse is not pressed, press it:
        if (!Mouse.isPressed(MOUSE_LEFT)) {
            Mouse.press(MOUSE_LEFT);
        }
    }
    // else the mouse button left is not pressed:
    else {
        // if the mouse is pressed, release it:
        if (Mouse.isPressed(MOUSE_LEFT)) {
            Mouse.release(MOUSE_LEFT);
        }
    }

/*-------------Mouse Button Right Click----------------------------*/
    // if the mouse button right is pressed:
    if (clickState_mouseButtonRight == LOW) {
        // if the mouse is not pressed, press it:
        if (!Mouse.isPressed(MOUSE_RIGHT)) {
            Mouse.press(MOUSE_RIGHT);
        }
    }
    // else the mouse button right is not pressed:
    else {
        // if the mouse is pressed, release it:
        if (Mouse.isPressed(MOUSE_RIGHT)) {
            Mouse.release(MOUSE_RIGHT);
        }
    }

/*------------------Up roll----------------------------------------*/
    if ((upState == LOW) && (clickState_mouseWheel == LOW)) {
        Mouse.move(0, 0, 1);
    }

/*------------------Down roll--------------------------------------*/
    if ((downState == LOW) && (clickState_mouseWheel == LOW)) {
        Mouse.move(0, 0, -1);
    }

/*------------------Switch Range-----------------------------------*/
    if (switchRangeButtonState == LOW) {
        range += 2;
        if(range > 10){
            range = 2;
        }
    }

/*--------------------Run USB Diaplay------------------------------*/
    // SERHD.print(".");
    // SerialUSB.print(".");
    USBDISP().eventRun();
}