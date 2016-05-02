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
//bool auth_flag = false;
bool sms_reply_flag = false;
bool data_flag = false;
//bool pump_status_flag = false;
bool inComing = false;
String password = "abcdef";  // Default password

// Create an instance of SMS and call
//SMSGSM sms;
//CallGSM call;

// Function Declaration
void InitHardware(void);
void SIMCardSetup(void);
void SMSServiceSetup(void);
void calculateRings(void);
void checkSMSContent(void);
void checkAuthentication(void);

// Main function starts here

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
			if(checkIfNumberAuthorized(gprsBuffers) {
				calculateRings();
			}
		}
		// +CMTI: "SM", 2
		else if(NULL != (s = strstr(gprsBuffer,"+CMTI: \"SM\""))) { 
			// Put the SMS handling code here
			checkSMSContent();
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
	EEPROM.begin(512);
	delay(10);
	
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
	
	// Set the communication settings for SIM900
	// Sent ATE0
	// Send AT+CLIP=1
	
}

void SIMCardSetup(void) {
	// check SIM status
	Serial.println(F("Checking for SIM status"));
	gsm.checkSIMStatus();
	
	// check IMEI of device
	char imei;
	imei = getIMEI();
	Serial.print(F("Received IMEI"));
	Serial.println(imei);
	
	// Generate Secret PASSWORD from IMEI
	Serial.println(F("Generating password"));
	GeneratePassword(imei);
	Serial.println(password);
	
	// Check for signal strength
	int rssi, ber
	// sent AT+CSQ
	//rssi = atoi(x);
	// ber = atoi(x);
	
	
	// Check for SIM registration
	int registered, networkType;
	// Send AT+CREG
	//registered = atoi(x); // will return 0 or 5 if registered
	if(registered == 0 || registered == 5) {
		Serial.println(F("SIM is registered"));
	}
	else {
		Serial.println(F("SIM is not registered"));
		soft_restart();
	}
}

void GeneratePassword(char* imei) {
	// think of some mechanism
	//password[0] = sum;
	//password[1] = odd_number_sum;
	//password[2] = even_number_sum;
	//password[4] = imei[12];
	//password[5] = imei[13];
	//password[6] = imei[14];
	
	Serial.println(F("PASSWORD generated"));
	Serial.println(password);
}

void SMSServiceSetup(void) {
	// Set SMS input mode
	// send AT+CMGF=1
	
	// Set the input character set
	// Send AT+CSCS="GSM"
	
	// Set SMS mode
	// Send AT+CNMI	
	
	Serial.println(F("SMS Service Ready"));
}

void calculateRings(void) {
	int counter = 1;
	int state = -1;
	
	sim900_clean_buffer(gprsBuffer,32);
	sim900_read_buffer(gprsBuffer,32,DEFAULT_TIMEOUT);
	// count the number of rings till you get RELEASE
	while(strstr(gprsBuffer,"RELEASE")) == NULL) {
		sim900_clean_buffer(gprsBuffer,32);
		sim900_read_buffer(gprsBuffer,32,DEFAULT_TIMEOUT);
		if(NULL != strstr(gprsBuffer,"RING")) {
			counter++;
		}
	}
	
	Serial.println(counter);
	Serial.println(F(" RINGS"));
	// Check the number of rings
	switch (counter){
		case RINGS_TURN_ON:
			state = 1;
			Serial.println(F("PUMP turned ON"));
			UpdateResource(state);
			break;
			
		case RINGS_TURN_OFF:
			state = 0;
			Serial.println(F("PUMP turned OFF"));
			UpdateResource(state);
			break;
			
		case RINGS_STATUS:
			state = -2;
			Serial.println(F("PUMP status check"));
			UpdateResource(state);
			break;
			
		default:
			Serial.println(F("invalid RINGS"));
			break;
	}
}

void UpdateResource(int state) {
	if(state >= 0) {
		digitalWrite(PUMP, state);
		if(sms_reply_flag) {
			// send SMS
			String reply = "TURNED ";
			reply += (state > 0) ? "ON" : "OFF";
			Serial.println(F(reply));
			gsm.sendSMS(1, reply);
		}
	}
	
	else if(state == -2) {
		if(sms_reply_flag) {
			String reply = "STATUS";
			reply += (digitalRead(PUMP) > 0) ? "ON" : "OFF";
			// Send SMS
			Serial.println(reply);
			sim900.sendSMS(number, response.c_str());
		}
	}	
}

void checkSMSContent() {
	// Get the SMS content
	int state = -1;
	char position = gsm.IsSMSPresent(SMS_UNREAD);			

	if(position) {
		// read SMS content
		gsm.GetSMS(position, phone_num, sms_text, 100);
		String SMSContent = String(sms_text);
		SMSContent.toUpperCase();
		Serial.println(F("Printing SMS content"));
		Serial.println(SMSContent);
		
		if(checkIfNumberAuthorized(SMSContent)) {
			// check the content of the message
			if(SMSContent.indexOf("AUTH") != -1) {			
				// check the password
				// AUTH 1234567890
				Serial.print(F("Mobile Number "));
				Serial.print(mobile_number);
				Serial.println(F(" Authorised"));
				// make a phonebook entry
			}
			else if(SMSContent.indexOf("UNAUTH") != -1) {
				// check for password
				// UNAUTH 1234567890
				Serial.print(F("Mobile Number "));
				Serial.print(mobile_number);
				Serial.println(F(" Unauthorised"));
				// delete a phonebook entry
			}
			else if(SMSContent.indexOf("SMS ENABLE") != -1) {
				sms_reply_flag = true;
				// send SMS
				String s = "SMS ENABLED";
				Serial.println(s);
				// gsm.sendSMS(1, s);
			}
			else if(SMSContent.indexOf("SMS DISABLE") != -1) {
				sms_reply_flag = false;
				// send SMS
				String s = "SMS DISABLED";
				// gsm.sendSMS(1, s);
			}
			else if(SMSContent.indexOf("DATA ENABLE") != -1) {
				data_flag = true;
				// send SMS]
				String s = "DATA ENABLED";
				// gsm.sendSMS(1, s);
			}
			else if(SMSContent.indexOf("DATA DISABLE") != -1) {
				data_flag = false;
				// send SMS
				String s = "SMS DISABLED";
				// gsm.sendSMS(1, "DATA DISABLED");
			}
			else if(SMSContent.indexOf("TURN ON") != -1) {
				state = 1;
				UpdateResource(state);
				Serial.println(F("PUMP turned ON"));
			}
			else if(SMSContent.indexOf("TURN OFF") != -1) {
				state = 0;
				UpdateResource(state);
				Serial.println(F("PUMP turned OFF"));
			}
			else if(SMSContent.indexOf("STATUS") != -1) {
				state = -2;
				UpdateResource(state);
				Serial.println(F("PUMP status check"));
			}
		}
		else {
			if(SMSContent.indexOf("AUTH") != -1) {			
				// check the password
				// AUTH 1234567890 abcdef
				if(SMSContent.substring(16,22) == "abcdef") {
					// make phonebook entry
				}
			}
			else if(SMSContent.indexOf("UNAUTH") != -1) {
				// check for password
				// UNAUTH 1234567890 abcdef
				if(SMSContent.substring(16,22) == "abcdef") {
					//delete a phonebook entry
				}
			}
		}
	}
}

bool checkIfNumberAuthorized(String inComingString) {
	String mobile_number;
	
	// check if string is call string or sms string
	// Extract mobile number from the string
	mobile_number = inComingString.substring(21,34);
	
	// check this mobile number with the saved numbers
	if(number is matched) {
		return(true);
	}
	else 
		return(false);
}