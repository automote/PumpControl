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
#include "GPRS_Shield_Arduino.h"


// SIM900 GSM module settings
#define PIN_TX		7
#define PIN_RX	    8
#define PIN_POWER   9
#define BAUDRATE	9600
#define MESSAGE_LENGTH 20

// Pump actuation pin
#define PUMP_ON		4
#define PUMP_OFF	5	

// For debugging interface
#define DEBUG	1

// Rings indentifier
#define RINGS_TURN_ON		3
#define RINGS_TURN_OFF		5
#define RINGS_STATUS		7
#define PB_ENTRY_INDEX_LOCATION 1
#define SMS_REPLY_LOCATION 2
#define DATA_REPLY_LOCATION 3


// Global Varables
bool rebootFlag = false;
bool smsReplyFlag = false;
bool dataFlag = false;
char *password = "abcdef";  // Default password
unsigned int PBEntryIndex = 1;
char gsmBuffer[64];
char *s = NULL;

// Create an instance of GPRS library
GPRS gsm(PIN_RX, PIN_TX, BAUDRATE);

// Function Declaration
void InitHardware(void);
void SIMCardSetup(void);
void SMSServiceSetup(void);
void calculateRings(void);
void checkSMSContent(void);
void checkAuthentication(void);

// Main function starts here

void setup() {
	int numberOfRings = 0;
	// Initialize the hardware
	InitHardware();
	
	// Setup SIM Card
	SIMCardSetup();
	
	// Setup SMS Service
	SMSServiceSetup();  
}

void loop() {
	// Wait for SMS or call
	if(gsm.readable()) {
		// Print the buffer
		sim900_read_buffer(gsmBuffer,sizeof(gsmBuffer),DEFAULT_TIMEOUT);
		Serial.println(gsmBuffer);
		// If incoming is call		
		// RING
		// +CLIP: "+91XXXXXXXXXX",145,"",,"TT+91XXXXXXXXXX",0
		if(NULL != strstr(gsmBuffer,"RING")) {
			handleRings();	
		}
		// If incoming is SMS
		// +CMTI: "SM", 2
		else if(NULL != (s = strstr(gsmBuffer,"+CMTI: \"SM\""))) { 
			handleSMS();
		}
		sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
	}
	else
		delay(100);
	
	if (rebootFlag) {
		Serial.println(F("Rebooting device"));
		delay(10000);
		soft_restart();
	}
}

void InitHardware(void) {
	// Start Serial editor
	Serial.begin(9600);
	
	if (EEPROM.read(500) != 786) {
		//if not load default config files to EEPROM
		writeInitalConfig();
	}
	// Read the config settings from EEPROM
	readConfig();
	Serial.println(F("Setting up the hardware"));
	
	// prepare the PUMP control GPIO
	pinMode(PUMP_ON, OUTPUT);
	digitalWrite(PUMP_ON, HIGH);
	
	pinMode(PUMP_OFF, OUTPUT);
	digitalWrite(PUMP_OFF, HIGH);
	
	// POWER ON the GSM module
	while(!gsm.init()) {
		Serial.println(F("Initialisation error"));
		delay(1000);
		soft_restart();
	}
	
	// Set the communication settings for SIM900
	// Sent ATE0
	sim900_check_with_cmd("ATE0\r\n","OK\r\n",CMD);
	delay(1000);
	// Send AT+CLIP=1
	sim900_check_with_cmd("AT+CLIP=1\r\n","OK\r\n",CMD);
	delay(1000);
	
}

void writeInitalConfig(void) {
	EEPROM.write(500, 786);
	EEPROM.write(PB_ENTRY_INDEX_LOCATION, 1); // Set default PBEntryIndex = 1
	EEPROM.write(SMS_REPLY_LOCATION, 0); // SMS Reply disabled
	EEPROM.write(DATA_REPLY_LOCATION, 0); // DATA disabled
}

void readConfig(void) {
	PBEntryIndex = EEPROM.read(1);
	smsReplyFlag = (EEPROM.read(2) != 0)? true : false;
	dataFlag = (EEPROM.read(3) != 0)? true : false;
}

void SIMCardSetup(void) {
		
	// check IMEI of device
	char imei[16];
	// Send AT+GSN
	gsm.getIMEI(imei);
	Serial.print(F("Received IMEI"));
	Serial.println(imei);
	
	// Generate Secret PASSWORD from IMEI
	Serial.println(F("Generating password"));
	GeneratePassword(imei);
	
	// Check for signal strength
	int *rssi, ber;
	// sent AT+CSQ
	gsm.getSignalStrength(rssi);
	Serial.print("Signal Strngth is ");
	Serial.println(rssi);
		
	// Check for SIM registration
	// Send AT+CREG?
	int registered, networkType;
	gsm.getSIMRegistration(registered,networkType);
	Serial.print(F("registered is "));
	Serial.println(registered);
	Serial.print(F("network type is "));
	Serial.println(networkType);
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
	

	Serial.println(F("PASSWORD generated"));
	Serial.println(password);
}

void SMSServiceSetup(void) {
	// Set SMS input mode
	// send AT+CMGF=1
	sim900_check_with_cmd("AT+CMGF=1\r\n","OK\r\n",CMD);
	delay(1000);
	
	// Set the input character set
	// Send AT+CSCS="GSM"
	sim900_check_with_cmd("AT+CSCS=\"GSM\"\r\n","OK\r\n",CMD);
	delay(1000);
	
	// Set SMS mode
	// Send AT+CNMI=2,1,0,0,0
	sim900_check_with_cmd("AT+CNMI=2,1,0,0\r\n","OK\r\n",CMD);
	delay(1000);
	// Save the settings for SMS
	// Send AT+CSAS=0
	sim900_check_with_cmd("AT+CSAS=0\r\n","OK\r\n",CMD);
	delay(1000);
	
	Serial.println(F("SMS Service Ready"));
}

void handleRings(void) {
	int counter = 1;
	int state = -1;
	char authString[64];
	char mobileNumber[16];
	sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
	sim900_read_buffer(gsmBuffer,sizeof(gsmBuffer),DEFAULT_TIMEOUT);
	strcpy(authString,gsmBuffer);
	// count the number of rings till you get RELEASE
	while(strstr(gsmBuffer,"NO CARRIER") == NULL) {
		sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
		sim900_read_buffer(gsmBuffer,sizeof(gsmBuffer),DEFAULT_TIMEOUT);
		if(NULL != strstr(gsmBuffer,"RING")) {
			counter++;
		}
	}
	
	Serial.println(counter);
	Serial.println(F(" RINGS"));
	getNumberFromString(authString, mobileNumber);
	if(checkIfNumberAuthorized(mobileNumber) > 0) {
		// Check the number of rings
		switch (counter){
			case RINGS_TURN_ON:
				state = 1;
				if(UpdateResource(state) == -1) {
					if(smsReplyFlag) {
						// send SMS
						gsm.sendSMS(mobileNumber,"TURNED ON");
						Serial.println(F("PUMP turned ON"));
					}
				}
				break;
				
			case RINGS_TURN_OFF:
				state = 0;
				if(UpdateResource(state) == -1) {
					if(smsReplyFlag) {
						// send SMS
						gsm.sendSMS(mobileNumber,"TURNED OFF");
						Serial.println(F("PUMP turned OFF"));
					}
				}
				break;
				
			case RINGS_STATUS:
				state = -2;
				char s = "STATUS ";
				if(UpdateResource(state)) {
					strcat(s, "ON");					
				}
				else {
					strcat(s, "OFF");
				}
				if(smsReplyFlag) {
					// send SMS
					gsm.sendSMS(mobileNumber,s);
					Serial.println(F("PUMP status check"));
				}
				break;
				
			default:
				Serial.println(F("invalid RINGS"));
				break;
		}
	}
}

int UpdateResource(int state) {
	switch (state) {
		case 0:
			digitalWrite(PUMP_OFF, LOW);
			digitalWrite(PUMP_ON, HIGH);
			return -1;
			break;
			
		case 1:
			digitalWrite(PUMP_ON, LOW);
			digitalWrite(PUMP_OFF, HIGH);
			return -1;
			break;
			
		case -2:
			if(!digitalRead(PUMP_ON)) {
				return 1;
			}
			else if(!digitalRead(PUMP_OFF)) {
				return 0;
			}
			break;
			
		default:
			Serial.println(F("Invalid Resource"));
			break;
	}
}

void handleSMS(void) {
	// Get the SMS content
	// +CMTI: "SM",<index>
	int state = -1;
	char message[100];
	char mobileNumber[16];
	char newMobileNumber[16];
	char dateTime[24];
	char *s, *p;
	char num[4];
	byte i = 0;
	
	int messageIndex = gsm.isSMSunread();			

	if(messageIndex > 0) {
		// read SMS content
		gsm.readSMS(messageIndex, message, MESSAGE_LENGTH, mobileNumber, dateTime);
		strupr(message);
		Serial.println(F("Printing SMS content"));
		Serial.println(message);
		
		if(checkIfNumberAuthorized(mobileNumber) > 0) {
			// check the content of the message
			if(NULL != ( s = strstr(message,"AUTH"))) {
				// AUTH 1234567890
				// Extract the mobile number from string
				s = strstr((char *)(s),"H");
				s = s + 2; // we are on the first digit of mobile number
				p = strstr((char *)(s),"\r"); // p is pointing to CR of the message string
				if(NULL != s) {
					i = 0;
					while (s < p) {
						newMobileNumber[i++] = *(s++);
					}
					newMobileNumber[i] = '\0';
				}
				Serial.println(newMobileNumber);
				
				// Make an entry into phonebook
				//AT+CPBW=PBEntryIndex,mobileNumber,129,"TTmobileNumber"
				sim900_flush_serial();
				sim900_send_cmd("AT+CPBW=");
				itoa(PBEntryIndex, num, 10);
				sim900_send_cmd(num);
				sim900_send_cmd(",");
				sim900_send_cmd(newMobileNumber);
				sim900_send_cmd(",129,\"TT");
				sim900_send_cmd(newMobileNumber);
				sim900_send_cmd("\r\n");
				
				sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
				sim900_read_buffer(gsmBuffer, sizeof(gsmBuffer), DEFAULT_TIMEOUT);
				sim900_wait_for_resp("OK\r\n", CMD);
				PBEntryIndex++;
				Serial.print(F("Mobile Number "));
				Serial.print(newMobileNumber);
				Serial.println(F(" Authorised"));
				// Save the index in EEPROM
				EEPROM.update(PB_ENTRY_INDEX_LOCATION, PBEntryIndex);
			}
			else if(NULL != ( s = strstr(message,"UNAUTH"))) {
				// UNAUTH 1234567890
				// Extract the mobile number
				s = strstr((char *)(s),"H");
				s = s + 2; // we are on the first digit of mobile number
				p = strstr((char *)(s),"\r"); // p is pointing to CR of the message string
				if(NULL != s) {
					i = 0;
					while (s < p) {
						newMobileNumber[i++] = *(s++);
					}
					newMobileNumber[i] = '\0';
				}
				Serial.println(newMobileNumber);
				
				// Find the entry from the phonebook
				// index = AT+CPBF="mobileNumber"
				int index = checkIfNumberAuthorized(newMobileNumber);
				
				// Delete the entry from the phonebook
				// AT+CPBW=index
				sim900_flush_serial();
				sim900_send_cmd("AT+CPBW=");
				itoa(index, num, 10);
				sim900_send_cmd(num);
				sim900_send_cmd("\r\n");
				
				sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
				sim900_read_buffer(gsmBuffer, sizeof(gsmBuffer), DEFAULT_TIMEOUT);
				sim900_wait_for_resp("OK\r\n", CMD);
								
				Serial.print(F("Mobile Number "));
				Serial.print(newMobileNumber);
				Serial.println(F(" Unauthorised"));
				
				// PBEntryIndex - 1 == index
				if(PBEntryIndex - 1 == index){
					PBEntryIndex--;
					// Save the index in EEPROM
					EEPROM.update(PB_ENTRY_INDEX_LOCATION, 	PBEntryIndex);
				}
			}
			else if(NULL != strstr(message,"SMS ENABLE")) {
				smsReplyFlag = true;
				// send SMS
				gsm.sendSMS(mobileNumber,"SMS ENABLED");
				// Save settings to EEPROM
				EEPROM.update(SMS_REPLY_LOCATION,1);
			}
			else if(NULL != strstr(message,"SMS DISABLE")) {
				smsReplyFlag = false;
				// send SMS
				gsm.sendSMS(mobileNumber,"SMS DISABLED");
				// Save setting to EEPROM
				EEPROM.update(SMS_REPLY_LOCATION,0);
			}
			else if(NULL != strstr(message,"DATA ENABLE")) {
				dataFlag = true;
				// send SMS
				gsm.sendSMS(mobileNumber, "DATA ENABLED");
				// Save setting to EEPROM
				EEPROM.update(DATA_REPLY_LOCATION,1);
			}
			else if(NULL != strstr(message,"DATA DISABLE")) {
				dataFlag = false;
				// send SMS
				gsm.sendSMS(mobileNumber, "DATA DISABLED");
				// Save setting to EEPROM
				EEPROM.update(DATA_REPLY_LOCATION,0);
			}
			else if(NULL != strstr(message,"TURN ON")) {
				state = 1;
				if(UpdateResource(state) == -1) {
					if(smsReplyFlag) {
						// send SMS
						gsm.sendSMS(mobileNumber,"TURNED ON");
						Serial.println(F("PUMP turned ON"));
					}
				}
			}
			else if(NULL != strstr(message,"TURN OFF")) {
				state = 0;
				if(UpdateResource(state) == -1) {
					if(smsReplyFlag) {
						// send SMS
						gsm.sendSMS(mobileNumber,"TURNED OFF");
						Serial.println(F("PUMP turned OFF"));
					}
				}
			}
			else if(NULL != strstr(message,"STATUS")) {
				state = -2;
				char s = "STATUS ";
				if(UpdateResource(state)) {
					strcat(s, "ON");					
				}
				else {
					strcat(s, "OFF");
				}
				if(smsReplyFlag) {
					// send SMS
					gsm.sendSMS(mobileNumber,s);
					Serial.println(F("PUMP status check"));
				}
			}
			else {
				Serial.println(F("Invalid Command"));
			}
		}
		else {
			if(NULL != ( s = strstr(message, "AUTH"))) {	
				// AUTH 1234567890 abcdef
				// Extract the mobile number from string
				s = strstr((char *)(s),"H");
				s = s + 2; // we are on the first digit of mobile number
				p = strstr((char *)(s)," "); // p is pointing to <space> of the message string
				if(NULL != s) {
					i = 0;
					while (s < p) {
						newMobileNumber[i++] = *(s++);
					}
					newMobileNumber[i] = '\0';
				}
				Serial.println(newMobileNumber);
				
				// get the password
				char userPass[7];
				s++; //now s is pointing to first character of password
				i = 0;
				while(i < 6){
					userPass[i++] = *(s++);
				}
				userPass[i] = '\0';
				Serial.println(userPass);
				
				// Compare the password
				if(!strcmp(userPass, password)) {
					// Make an entry into phonebook
					//AT+CPBW=PBEntryIndex,mobileNumber,129,"TTmobileNumber"
					sim900_flush_serial();
					sim900_send_cmd("AT+CPBW=");
					itoa(PBEntryIndex, num, 10);
					sim900_send_cmd(num);
					sim900_send_cmd(",");
					sim900_send_cmd(newMobileNumber);
					sim900_send_cmd(",129,\"TT");
					sim900_send_cmd(newMobileNumber);
					sim900_send_cmd("\r\n");

					sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
					sim900_read_buffer(gsmBuffer, sizeof(gsmBuffer), DEFAULT_TIMEOUT);
					sim900_wait_for_resp("OK\r\n", CMD);
					PBEntryIndex++;
					Serial.print(F("Mobile Number "));
					Serial.print(newMobileNumber);
					Serial.println(F(" Authorised"));
					// Save the index in EEPROM
					EEPROM.update(PB_ENTRY_INDEX_LOCATION, PBEntryIndex);
					}
			}
			else if(NULL != ( s = strstr(message,"UNAUTH"))) {
				// UNAUTH 1234567890 abcdef
				// Extract the mobile number from string
				s = strstr((char *)(s),"H");
				s = s + 2; // we are on the first digit of mobile number
				p = strstr((char *)(s)," "); // p is pointing to <space> of the message string
				if(NULL != s) {
					i = 0;
					while (s < p) {
						newMobileNumber[i++] = *(s++);
					}
					newMobileNumber[i] = '\0';
				}
				Serial.println(newMobileNumber);
				
				// get the password
				char userPass[7];
				s++; //now s is pointing to first character of password
				i = 0;
				while(i < 6){
					userPass[i++] = *(s++);
				}
				userPass[i] = '\0';
				Serial.println(userPass);
				
				// Compare the passwords
				if(!strcmp(userPass, password)) {
					// Find the entry from the phonebook
					// index = AT+CPBF="mobileNumber"
					int index = checkIfNumberAuthorized(newMobileNumber);

					// Delete the entry from the phonebook
					// AT+CPBW=index
					sim900_flush_serial();
					sim900_send_cmd("AT+CPBW=");
					itoa(index, num, 10);
					sim900_send_cmd(num);
					sim900_send_cmd("\r\n");

					sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
					sim900_read_buffer(gsmBuffer, sizeof(gsmBuffer), DEFAULT_TIMEOUT);
					sim900_wait_for_resp("OK\r\n", CMD);
									
					Serial.print(F("Mobile Number "));
					Serial.print(newMobileNumber);
					Serial.println(F(" Unauthorised"));

					// PBEntryIndex - 1 == index
					if(PBEntryIndex - 1 == index){
						PBEntryIndex--;
						// Save the index in EEPROM
						EEPROM.update(PB_ENTRY_INDEX_LOCATION, 	PBEntryIndex);
					}
				}
			}
			else {
				Serial.println(F("invalid authorization command"));
			}
		}
	}
}

bool getNumberFromString(char *inComingString, char *mobileNumber) {
	byte i = 0;
	char mobileNumber[16];
	char *p,*p2,*s;
	byte messageIndex = 0;
	char message[MESSAGE_LENGTH];
	char dateTime[24];
	
	// check if string is call string or sms string
	if(NULL != ( s = strstr(inComingString,"+CLIP:"))) {
		// inComingString is a call
		// Response is like:
		// RING
		// +CLIP: "+91XXXXXXXXXX",145,"",,"TT+91XXXXXXXXXX",0
		
		// Extract mobile number from the string
		p = strstr(s,"\"");
		p++; // we are on first character of phone number
		p2 = strstr(p,"\"");
		if (NULL != p) {
			i = 0;
			while(p < p2) {
				mobileNumber[i++] = *(p++);
			}
			mobileNumber[i] = '\0';
		}
		
		Serial.print(F("Mobile number obtained "));
		Serial.println(mobileNumber);
		return true;
	}
	return false;
}
		
int checkIfNumberAuthorized(char *mobileNumber) {
	// check this mobile number with the saved numbers and return the index if found
	// Searching for the mobile number
	char *s;
	// AT+CPBF="mobileNumber"
	sim900_send_cmd("AT+CPBF=\"");
	sim900_send_cmd(mobileNumber);
	sim900_send_cmd("\"\r\n");
	sim900_clean_buffer(gsmBuffer, sizeof(gsmBuffer));
	sim900_read_buffer(gsmBuffer, sizeof(gsmBuffer));
	
	// +CPBF: 1,"+91XXXXXXXXXX",145,"TT+91XXXXXXXXXX"
	// If number is present
	if(NULL != ( s = strstr(gsmBuffer, "+CPBF:"))) {
		s = strstr((char *)(s),":");
		return atoi(s+1);	
	}
	else 
		return -1;
}