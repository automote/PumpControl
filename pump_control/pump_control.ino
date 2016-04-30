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

#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>
#include "SoftReset.h"
// Add SIM900 library here

/*
// SIM900 GSM module PIN and BAUD RATE settings
#define PIN_TX		3
#define PIN_RX	    2
#define PIN_PWR		9
#define BAUDRATE	9600
*/
// Pump actuation pin
#define PUMP	6

// For debugging interface
#define DEBUG	1

// Rings indentifier
#define RINGS_TURN_ON		3
#define RINGS_TURN_OFF		5
#define RINGS_STATUS		7

// Global Varables
bool reboot_flag = false;
bool auth_flag = false;
bool sms_reply_flag = false;
bool data_flag = false;
bool pump_status_flag = false;
bool inComing = false;
char password = "abcdef";  // Default password

// Create an instance of SMS and call
SMSGSM sms;
CallGSM call;

void setup() {
	// Initialize the hardware
	InitHardware();
	
	// Setup SIM Card
	SIMCardSetup();
	
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
		
		// RING
		// +CCWA: "1234657890",129,1
		if(NULL != strstr(gprsBuffer,"RING")) {
			// Put the ring handling code here
			calculateRings();
		}
		// +CMTI: "SM", 2
		else if(NULL != (s = strstr(gprsBuffer,"+CMTI: \"SM\""))) { 
			// Put the SMS handling code here
			checkSMSContent			
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
	while(!gsm.init()) {
		Serial.println(F("Initialisation error"));
		delay(1000);
		soft_restart();
	}
	
	// Sent ATE0
	// Send AT+CLIP=1
	
}

void SIMCardSetup(void) {
	// check SIM status
	gsm.checkSIMStatus();
	
	// check IMEI of device
	char imei;
	imei = getIMEI();
	
	// Generate Secret PASSWORD from IMEI
	GeneratePassword(imei);
	
	// Check for signal strength
	int rssi, ber
	// sent AT+CSQ
	//rssi = atoi(x);
	// ber = atoi(x);
	
	
	// Check for SIM registration
	int registered, networkType;
	// Send AT+CREG
	//registered = atoi(x);
	//networkType = atoi(x);
}

void GeneratePassword(char* imei) {
	// think of some mechanism
	//password[0] = sum;
	//password[1] = odd_number_sum;
	//password[2] = even_number_sum;
	//password[4] = imei[12];
	//password[5] = imei[13];
	//password[6] = imei[14];
}

void SMSServiceSetup(void) {
	// Set SMS input mode
	// send AT+CMGF=1
	
	// Set the input character set
	// Send AT+CSCS="GSM"
	
	// Set SMS mode
	// Send AT+CNMI	
}

void calculateRings(void) {
	int counter = 1;
	int state;
	int which_pump;
	
	sim900_clean_buffer(gprsBuffer,32);
	sim900_read_buffer(gprsBuffer,32,DEFAULT_TIMEOUT);
	// count the number of rings till you get RELEASE
	while(strstr(gprsBuffer,"RELEASE")) != NULL) {
		sim900_clean_buffer(gprsBuffer,32);
		sim900_read_buffer(gprsBuffer,32,DEFAULT_TIMEOUT);
		if(NULL != strstr(gprsBuffer,"RING")) {
			counter++;
		}
	}
	
	// Check the number of rings
	switch (counter){
		case RINGS_TURN_ON:
			which_pump = PUMP;
			state = 1;
			UpdateResource(which_pump, state);
			break;
			
		case RINGS_TURN_OFF:
			which_pump = PUMP;
			state = 0;
			UpdateResource(which_pump, state);
			break;
			
		case RINGS_STATUS:
			which_pump = PUMP;
			state = -2;
			UpdateResource(which_pump,state);
			break;
			
		default:
			Serial.println(F("invalid RINGS"));
			break;
	}
	
}

void UpdateResource(int which_pump, int state) {
	if(state >= 0) {
		digitalWrite(which_pump, state);
	}
	
	else if(state == -2) {
		if(sms_reply_flag) {
			String response = "STATUS";
			response += (digitalRead(which_pump) > 0) ? "ON" : "OFF";
			// Send SMS
			Serial.println(F("Sending STATUS SMS"));
			sim900.sendSMS(number, response.c_str());
		}
	}	
}


