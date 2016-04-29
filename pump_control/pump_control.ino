/*
	Pump Control sketch for Arduino using SIM900
	Lovelesh, thingTronics
	
The MIT License (MIT)

Copyright (c) 2015 thingTronics Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

version 0.1
*/

#include <GPRS_Shield_Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>
#include <SoftReset.h>

// SIM900 GSM module PIN and BAUD RATE settings
#define PIN_TX    3
#define PIN_RX    2
#define PIN_PWR	  9
#define BAUDRATE  9600

// Pump actuation pin
#define PUMP 6

// For debugging interface
#define DEBUG 1

// Global Varables
bool reboot_flag = false;
bool auth_flag = false;
bool sms_reply_flag = false;
bool data_flag = false;
bool pump_status_flag = false;
bool inComing = false;
String password = "abcdefgh";  // Default password

// Create an instance of GPRS shield
GPRS sim900(PIN_TX,PIN_RX,BAUDRATE);

void setup() {
	// Initialize the hardware
	InitHardware();
	
	// Check if SIM900 is working correctly
	reboot_flag = !GSMIsPoweredOn();
	
	// Reboot if any problem with GSM device
	if (reboot_flag)
		soft_restart();
	
	// Setup SIM Card
	SIMCardSetup();
	
	// Generate Secret PASSWORD
	GeneratePassword();
	
	// Setup SMS Service
	SMSServiceSetup();  
}

void loop() {
	// Wait for SMS or call
	if({SMS or call available}) {
		inComing = true;
	}
	else
		delay(100);
		
	// if SMS or call arrives
	if(inComing) {	
		sim900_read_buffer(gprsBuffer,32,DEFAULT_TIMEOUT);
		Serial.print(gprsBuffer);
      
		if(NULL != strstr(gprsBuffer,"RING")) {
			// Put the ring handling code here
		}
		else if(NULL != (s = strstr(gprsBuffer,"+CMTI: \"SM\""))) { 
			// Put the SMS handling code here
		}
		sim900_clean_buffer(gprsBuffer,32);
		inComing = false;
	}
	
	if (reboot_flag) {
		Serial.println(F("Rebooting device"));
		delay(10000);
		soft_restart();
	}
}

void InitHardware(void) {
	// Start Serial editor
	Serial.begin(9600);
	
	// Specify EEPROM Block
	// Find a better saving mechanism than EEPROM
	
	Serial.println(F("Setting up the hardware"));
	
	// prepare the PUMP control GPIO
	pinMode(PUMP, OUTPUT);
	digitalWrite(PUMP, LOW);
	
	// POWER ON the GSM module
	while(!sim900.init()) {
		Serial.println(F("Initialisation error"));
		delay(1000);
		soft_restart();
	}
	
	
	
}