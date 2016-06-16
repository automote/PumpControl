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
#include <string.h>
#include "SoftReset.h"
// Add SIM900 library here
#include "GPRS_Shield_Arduino.h"


// SIM900 GSM module settings
#define PIN_TX		50
#define PIN_RX	    51
//#define PIN_POWER   9
#define BAUDRATE	9600
#define MESSAGE_LENGTH 30

// Pump actuation pin
#define PUMP_ON		52
#define PUMP_OFF	53	

// For debugging interface
#define DEBUG 1
#define DELAY_TIME 1000

// Rings indentifier
#define RINGS_TURN_ON		3
#define RINGS_TURN_OFF		5
#define RINGS_STATUS		7
#define PB_ENTRY_INDEX_LOCATION 1
#define SMS_REPLY_LOCATION 2
#define DATA_REPLY_LOCATION 3
#define MISSEDCALL_LOCATION 4

// Global Constant
const char* company_name = "thingTronics";
const char* hardware_version = "v0.1";
const char* software_version = "v0.2";

// Global Varables
bool rebootFlag = false;
bool smsReplyFlag = true;
bool missedCallFlag = true;
bool dataFlag = true;
char *password = "ABCDEF";  // Default password
unsigned int PBEntryIndex = 1;
byte messageIndex = 0;
char gsmBuffer[64];
bool pumpFlag = false;
char dateTime_buffer[24];
char dateTime[24];
char providerName[10];

// Create an instance of GPRS library
GPRS gsm(PIN_RX, PIN_TX, BAUDRATE);

// Function Definitions
void InitHardware(void);
void writeInitalConfig(void);
void readConfig(void);
void GeneratePassword(char* imei);
void SIMCardSetup(void);
void SMSServiceSetup(void);
void handleRings(void); 
void handleSMS(byte messageIndex);
int UpdateResource(int state);
bool getNumberFromString(char *inComingString, char *mobileNumber);
int checkIfNumberAuthorized(char *mobileNumber);

// Main function starts here

void setup() {
	
	// Initialize the hardware
	InitHardware();
	
	// Setup SIM Card
	SIMCardSetup();
	
	// Setup SMS Service
	SMSServiceSetup();  
	
	Serial.println(F("Listening for SMS or Call"));
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
		if(NULL != strstr(gsmBuffer,"+CLIP:")) {
			if(missedCallFlag) {
				Serial.println(F("handling calls"));
				handleRings();
			}
		}
		
		// If incoming is SMS
		// +CMTI: "SM", 2
		messageIndex = gsm.isSMSunread();
		if(messageIndex > 0) {
			Serial.println(F("handling SMS"));
			handleSMS(messageIndex);
			messageIndex = 0;
		}
		
		sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
	}
	
	else
		delay(100);
	
	if (rebootFlag) {
		Serial.println(F("Rebooting device"));
		delay(10 * DELAY_TIME);
		soft_restart();
	}
}

void InitHardware(void) {
	// Start Serial editor
	Serial.begin(BAUDRATE);
	
	//while(!Serial);
	
	if (EEPROM.read(500) != 175) {
		//if not load default config files to EEPROM
		writeInitalConfig();
	}
	// Read the config settings from EEPROM
	readConfig();	
	
	// Printing company info
	Serial.println("---------------------------");
	Serial.print(F("Comapany Name: "));
	Serial.println(company_name);
	Serial.print(F("Hardware Version: "));
	Serial.println(hardware_version);
	Serial.print(F("Software Version: "));
	Serial.println(software_version);
	Serial.println("---------------------------");
	Serial.println(F("Setting up the hardware"));
	
	// prepare the PUMP control GPIO
	pinMode(PUMP_ON, OUTPUT);
	digitalWrite(PUMP_ON, HIGH);
	
	pinMode(PUMP_OFF, OUTPUT);
	digitalWrite(PUMP_OFF, HIGH);
	
	delay(15 * DELAY_TIME);
	// POWER ON the GSM module
	while(!gsm.init()) {
		Serial.println(F("Initialisation error"));
		delay(DELAY_TIME);
		soft_restart();
	}
	
	// Set the communication settings for SIM900
	// Sent ATE0
	sim900_check_with_cmd(F("ATE0\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
	// Send AT+CLIP=1
	sim900_check_with_cmd(F("AT+CLIP=1\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
	
}

void writeInitalConfig(void) {
	Serial.println(F("Writing default configuration"));
	EEPROM.write(500, 175);
	EEPROM.write(PB_ENTRY_INDEX_LOCATION, 1); // Set default PBEntryIndex = 1
	EEPROM.write(SMS_REPLY_LOCATION, 1); // SMS Reply disabled
	EEPROM.write(DATA_REPLY_LOCATION, 1); // DATA disabled
	EEPROM.write(MISSEDCALL_LOCATION, 1); // Missed call disabled
}

void readConfig(void) {
	Serial.println(F("Reading config"));
	PBEntryIndex = EEPROM.read(1);
	smsReplyFlag = (EEPROM.read(2) != 0)? true : false;
	dataFlag = (EEPROM.read(3) != 0)? true : false;
	missedCallFlag = (EEPROM.read(4) != 0)? true: false;
}

void SIMCardSetup(void) {
	// check IMEI of device
	char imei[16];
	// Send AT+GSN
	gsm.getIMEI(imei);
	Serial.print(F("Received IMEI "));
	Serial.println(imei);
	
	// Generate Secret PASSWORD from IMEI
#ifdef DEBUG
	Serial.println(F("Generating password"));
#endif
	GeneratePassword(imei);
	
	// Check for Service Provider's Name
	// AT+CSPN?
	gsm.getProviderName(providerName);
	Serial.println("-------------------------");
	Serial.print("PROVIDER NAME :");
	Serial.println(providerName);
	Serial.println("-------------------------");
	
	// Check for signal strength
	int rssi, ber;
	// sent AT+CSQ
	gsm.getSignalStrength(&rssi);
#ifdef DEBUG
	Serial.print(F("Signal Strength is "));
	Serial.println(rssi);
#endif
	// Check for SIM registration
	// Send AT+CREG?
	int registered, networkType;
	gsm.getSIMRegistration(&networkType);
	if( networkType == 1 || networkType == 5) {
		Serial.println(F("SIM is registered"));
		if(networkType == 1) {
#ifdef DEBUG
			Serial.println(F("NETWORK type is LOCAL (1)"));
#endif
		}
		else if(networkType == 5) {
#ifdef DEBUG
			Serial.println(F("NETWORK type is ROAMING (5)"));
#endif
		}
	}
	else {
		Serial.println(F("SIM is not registered"));
		soft_restart();
	}
	
	// Set the clock to update from Service Provider
	// AT+CLTS=1
	sim900_check_with_cmd(F("AT+CLTS=1\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
}

void GeneratePassword(char* imei) {
	// think of some mechanism
	
#ifdef DEBUG
	Serial.print(F("PASSWORD generated: "));
	Serial.println(password);
#endif
}

void SMSServiceSetup(void) {
	// Set SMS input mode
	Serial.println(F("Setting input mode"));
	// send AT+CMGF=1
	sim900_check_with_cmd(F("AT+CMGF=1\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
	
	// Set the input character set
	Serial.println(F("Setting character mode: GSM"));
	// Send AT+CSCS="GSM"
	sim900_check_with_cmd(F("AT+CSCS=\"GSM\"\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
	
	// Set SMS mode
	Serial.println(F("Setting SMS message indications"));
	// Send AT+CNMI=2,1,0,0,0
	sim900_check_with_cmd(F("AT+CNMI=2,1,0,0\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
	
	// Save the settings for SMS
	Serial.println(F("Saving settings to memory"));
	// Send AT+CSAS=0
	sim900_check_with_cmd(F("AT+CSAS=0\r\n"),"OK\r\n",CMD);
	delay(DELAY_TIME);
	Serial.println(F("SMS Service Ready"));
}

void handleRings(void) {
	byte counter = 1;
	byte state = -1;
	char authString[64];
	char mobileNumber[16];
	char *s;
	s = (char *)malloc(60);
	
	strcpy(authString,gsmBuffer);
	// count the number of rings till you get RELEASE
	do {
		sim900_clean_buffer(gsmBuffer,sizeof(gsmBuffer));
		sim900_read_buffer(gsmBuffer,sizeof(gsmBuffer),DEFAULT_TIMEOUT);
	
		if(NULL != strstr(gsmBuffer,"+CLIP:")) {
			counter++;
		}
	}while(NULL == strstr(gsmBuffer,"NO CARRIER"));
	
	Serial.print(counter);
	Serial.println(F(" RINGS"));
	getNumberFromString(authString, mobileNumber);
	if(checkIfNumberAuthorized(mobileNumber) > 0) {
		// Check the number of rings
		switch (counter){
			case 2:
			case 3:
				state = 1;
				//gsm.getDateTime(char *buffer)
				gsm.getDateTime(dateTime);
				strcpy(dateTime_buffer,dateTime);
				if(UpdateResource(state)) {
					strcpy(s,"TURNED ON");
					
					if(smsReplyFlag) {
						// send SMS
						gsm.sendSMS(mobileNumber,s);
					}
#ifdef DEBUG
					Serial.println(F("PUMP turned ON"));
#endif
				}
				break;
				
			case 4:
			case 5:
				state = 0;
				char t1buffer[140];
				strcpy(t1buffer,"THE PUMP IS TURNED OFF.\nTURNED ON TIME IS ");
				strcat(t1buffer,dateTime_buffer);
				if(UpdateResource(state)) {
					strcpy(s,"TURNED OFF");

					if(smsReplyFlag) {
						// send SMS
						gsm.sendSMS(mobileNumber,t1buffer);
					}
#ifdef DEBUG
				Serial.println(F("PUMP turned OFF"));
#endif
				}
				break;
				
			case 6:
			case 7:
				state = 2;
				// construct the status string
				strcpy(s,"PUMP ");
				if(UpdateResource(state) > 0)
					strcat(s, "ON");
				else 
					strcat(s, "OFF");
				
				strcat(s, "\nSMS ");
				
				if(smsReplyFlag)
					strcat(s, "ENABLED");
				else
					strcat(s, "DISABLED");
				
				strcat(s, "\nDATA ");
				
				if(dataFlag)
					strcat(s, "ENABLED");
				else
					strcat(s, "DISABLED");
				
				strcat(s, "\nMISSEDCALL ");
				
				if(missedCallFlag)
					strcat(s, "ENABLED");
				else
					strcat(s, "DISABLED");
             
				if(smsReplyFlag) {
					// send SMS
					gsm.sendSMS(mobileNumber,s);
				}
		
#ifdef DEBUG
				Serial.println(F("PUMP status check"));
#endif
				break;
				
			default:
#ifdef DEBUG
				Serial.println(F("invalid RINGS"));
#endif
				break;
		}


	}
	Serial.println(F("call handling done"));
}

int UpdateResource(int state) {
	switch (state) {
		
		case 0:
			{
				digitalWrite(PUMP_OFF, LOW);
			delay(DELAY_TIME);
			digitalWrite(PUMP_OFF, HIGH);
#ifdef DEBUG
			Serial.println(F("PUMP OFF"));
#endif
			pumpFlag = false;
			return 1;
			break;
			}
			
		case 1:
			digitalWrite(PUMP_ON, LOW);
			delay(DELAY_TIME);
			digitalWrite(PUMP_ON, HIGH);
#ifdef DEBUG
			Serial.println(F("PUMP ON"));
#endif
			pumpFlag = true;
			return 1;
			break;
		
		case 2:
#ifdef DEBUG
			Serial.println(F("Sending pump status"));
#endif
			return pumpFlag;			
			break;
			
		default:
#ifdef DEBUG
			Serial.println(F("Invalid Resource"));
#endif
			return -1;
			break;
	}
}

void handleSMS(byte messageIndex) {
	// Get the SMS content
	int state = -1;
	char message[100];
	char mobileNumber[16];
	char newMobileNumber[16];
	char *s, *p;
	char num[4];
	byte i = 0;
	
	// read SMS content
	gsm.readSMS(messageIndex, message, MESSAGE_LENGTH, mobileNumber, dateTime);
	strupr(message);
#ifdef DEBUG
	Serial.println(F("Printing SMS content"));
	Serial.println(message);
#endif

	// delete SMS to save memory
	gsm.deleteSMS(messageIndex);

	if(checkIfNumberAuthorized(mobileNumber) > 0) {
		// check the content of the message
		if(NULL != ( s = strstr(message,"ADD"))) {
			// ADD 1234567890
			// Extract the mobile number from string
			s = s + 4; // we are on the first digit of mobile number
			if(NULL != s) {
				i = 0;
				while(i < 10) {
					newMobileNumber[i++] = *(s++);
				}
				newMobileNumber[i] = '\0';
			}
#ifdef DEBUG
			Serial.print(F("New mobile number is "));
			Serial.println(newMobileNumber);
#endif
			// Find the entry from the phonebook
			// index = AT+CPBF="mobileNumber"
			int index = checkIfNumberAuthorized(newMobileNumber);
			
			// If number already present donot add
			if(index <= 0) {
				// Make an entry into phonebook
				//AT+CPBW=PBEntryIndex,"newmobileNumber",145,"TTmobileNumber"
				PBEntryIndex++;
#ifdef DEBUG
				Serial.print(F("PB index is "));
				Serial.println(PBEntryIndex);
#endif
				sim900_flush_serial();
				sim900_send_cmd(F("AT+CPBW="));
				itoa(PBEntryIndex, num, 10);
				sim900_send_cmd(num);
				sim900_send_cmd(F(",\"+91"));
				sim900_send_cmd(newMobileNumber);
				sim900_send_cmd(F("\",145,\"TT+91"));
				sim900_send_cmd(newMobileNumber);
				sim900_send_cmd(F("\"\r\n"));
				
				sim900_wait_for_resp("OK\r\n", CMD);
#ifdef DEBUG
				Serial.print(F("Mobile Number "));
				Serial.print(newMobileNumber);
				Serial.println(F(" Authorised"));
#endif
				gsm.sendSMS(mobileNumber, "Mobile Number Authorised");
				
				// Save the index in EEPROM
				EEPROM.update(PB_ENTRY_INDEX_LOCATION, PBEntryIndex);
			}
			else {
#ifdef DEBUG
				Serial.println(F("number already authorised"));
#endif
				gsm.sendSMS(mobileNumber, "Mobile number already Authorised");
			}
		}
		else if(NULL != ( s = strstr(message,"REMOVE"))) {
			// REMOVE 1234567890
			// Extract the mobile number
			s = s + 7; // we are on the first digit of mobile number
			if(NULL != s) {
				i = 0;
				while (i < 10) {
					newMobileNumber[i++] = *(s++);
				}
				newMobileNumber[i] = '\0';
			}
#ifdef DEBUG
			Serial.print(F("New mobile number is "));
			Serial.println(newMobileNumber);
#endif
			// Find the entry from the phonebook
			// index = AT+CPBF="mobileNumber"
			int index = checkIfNumberAuthorized(newMobileNumber);
			
			if(index > 0) {
#ifdef DEBUG
				Serial.print(newMobileNumber);
				Serial.println(F(" is authorised"));
				Serial.println(F("deleting the number"));
#endif
				// Delete the entry from the phonebook
				// AT+CPBW=index
				sim900_flush_serial();
				sim900_send_cmd(F("AT+CPBW="));
				itoa(index, num, 10);
				sim900_send_cmd(num);
				sim900_send_cmd(F("\r\n"));

				sim900_wait_for_resp("OK\r\n", CMD);
#ifdef DEBUG				
				Serial.print(F("Mobile Number "));
				Serial.print(newMobileNumber);
				Serial.println(F(" Unauthorised"));
#endif
				gsm.sendSMS(mobileNumber, "Mobile Number Unauthorised");

				// PBEntryIndex == index
				if(PBEntryIndex == index){
					PBEntryIndex--;
					// Save the index in EEPROM
					EEPROM.update(PB_ENTRY_INDEX_LOCATION, 	PBEntryIndex);
				}
			}
			else {
#ifdef DEBUG
				Serial.println(F("invalid number"));
#endif
				gsm.sendSMS(mobileNumber, "invalid mobile number");
			}
			
		}
		else if(NULL != strstr(message,"SMS ENABLE")) {
			smsReplyFlag = true;
			// send SMS
			if(smsReplyFlag) {
				gsm.sendSMS(mobileNumber,"SMS ENABLED");
			}
			// Save settings to EEPROM
			EEPROM.update(SMS_REPLY_LOCATION,1);
#ifdef DEBUG
			Serial.println(F("SMS ENABLED"));
#endif
		}
		else if(NULL != strstr(message,"SMS DISABLE")) {
			smsReplyFlag = false;
			// Save setting to EEPROM
			EEPROM.update(SMS_REPLY_LOCATION,0);
#ifdef DEBUG
			Serial.println(F("SMS DISABLED"));
#endif
		}
		else if(NULL != strstr(message,"DATA ENABLE")) {
			dataFlag = true;
			// send SMS
			if(smsReplyFlag){
				gsm.sendSMS(mobileNumber, "DATA ENABLED");
			}
			// Save setting to EEPROM
			EEPROM.update(DATA_REPLY_LOCATION,1);
#ifdef DEBUG
			Serial.println(F("DATA ENABLED"));
#endif			
		}
		else if(NULL != strstr(message,"DATA DISABLE")) {
			dataFlag = false;
			// send SMS
			if(smsReplyFlag) {
				gsm.sendSMS(mobileNumber, "DATA DISABLED");
			}
			// Save setting to EEPROM
			EEPROM.update(DATA_REPLY_LOCATION,0);
#ifdef DEBUG
			Serial.println(F("DATA DISABLED"));
#endif
		}
		else if(NULL != strstr(message,"MISSEDCALL ENABLE")) {
			missedCallFlag = true;
			// send SMS
			if(smsReplyFlag){
				gsm.sendSMS(mobileNumber, "MISSEDCALL ENABLED");
			}
			// Save setting to EEPROM
			EEPROM.update(MISSEDCALL_LOCATION,1);
#ifdef DEBUG
			Serial.println(F("MISSED CALL ENABLED"));
#endif
		}
		else if(NULL != strstr(message,"MISSEDCALL DISABLE")) {
			missedCallFlag = false;
			// send SMS
			if(smsReplyFlag) {
				gsm.sendSMS(mobileNumber, "MISSEDCALL DISABLED");
			}
			// Save setting to EEPROM
			EEPROM.update(MISSEDCALL_LOCATION,0);
#ifdef DEBUG
			Serial.println(F("MISSED CALL ENABLED"));
#endif
		}
		else if(NULL != strstr(message,"TURN ON")) {
			state = 1;
				//copy dateTime 
			strcpy(dateTime_buffer,dateTime);
			if(UpdateResource(state)) {
				if(smsReplyFlag) {
					// send SMS
					gsm.sendSMS(mobileNumber,"TURNED ON");
				}
			}
#ifdef DEBUG
			Serial.println(F("PUMP turned ON"));
#endif
		}
		else if(NULL != strstr(message,"TURN OFF")) {
			state = 0;
			char tbuffer[100]="PUMP IS TURNED OFF. TURNED ON TIME IS ";
			strcat(tbuffer,dateTime_buffer);
			if(UpdateResource(state)) {
				if(smsReplyFlag) {
					// send SMS   
         Serial.println("sending SMS");
         gsm.sendSMS(mobileNumber,tbuffer);
				}

			}
#ifdef DEBUG
			Serial.println(F("PUMP turned OFF"));
#endif		
		}
		else if(NULL != strstr(message,"BALANCE")) {
			// BALANCE *123#
			char code[6];
			char USSDResponse[140];
			char resultCode[2];
			s = s + 8; // we are on * of USSD code
			p = strstr((char *)(s),"#"); // p is pointing to # of the message string
			
			if(NULL != s) {
				i = 0;
				while (s < p) {
					code[i++] = *(s++);
				}
				code[i] = '\0';
			}
#ifdef DEBUG			
			Serial.print(F("The USSD code is "));
			Serial.println(code);
#endif		
			sim900_flush_serial();
			gsm.sendUSSDSynchronous(code,resultCode,USSDResponse);
            gsm.cancelUSSDSession();
#ifdef DEBUG			
            Serial.print(F("Sending Response "));
            Serial.println(USSDResponse);
#endif			

            gsm.sendSMS(mobileNumber,USSDResponse);
#ifdef DEBUG
			Serial.println(F("USSD response sent"));
#endif  
		}
		else if(NULL != strstr(message,"STATUS")) {
			state = 2;
			char *s;
			s = (char *)malloc(60);
			// construct the status string
			strcpy(s,"PUMP ");
			if(UpdateResource(state) > 0)
				strcat(s, "ON");
			else 
				strcat(s, "OFF");
			
			strcat(s, "\nSMS ");
			if(smsReplyFlag)
				strcat(s, "ENABLED");
			else
				strcat(s, "DISABLED");
			
			strcat(s, "\nDATA ");
			if(dataFlag)
				strcat(s, "ENABLED");
			else
				strcat(s, "DISABLED");
			
			strcat(s, "\nMISSEDCALL ");
			if(missedCallFlag)
				strcat(s, "ENABLED");
			else
				strcat(s, "DISABLED");
			
			if(smsReplyFlag) {
				// send SMS
				gsm.sendSMS(mobileNumber,s);
			}
#ifdef DEBUG			
			Serial.println(F("Sending device status"));
#endif
		}
		else {
#ifdef DEBUG
			Serial.println(F("Invalid Command"));
#endif
		}
	}
	else {
		if(NULL != ( s = strstr(message, "ADD"))) {
#ifdef DEBUG		
			Serial.print(F("Entry index is "));
			Serial.println(PBEntryIndex);
#endif
			// ADD 1234567890 ABCDEF
			// Extract the mobile number from string
			s = s + 4; // we are on the first digit of mobile number
			p = strstr((char *)(s)," "); // p is pointing to <space> of the message string
			if(NULL != s) {
				i = 0;
				while (s < p) {
					newMobileNumber[i++] = *(s++);
				}
				newMobileNumber[i] = '\0';
			}
#ifdef DEBUG
			Serial.print(F("New mobile number is "));
			Serial.println(newMobileNumber);
#endif
			// get the password
			char userPass[7];
			s++; //now s is pointing to first character of password
			i = 0;
			while(i < 6){
				userPass[i++] = *(s++);
			}
			userPass[i] = '\0';
#ifdef DEBUG
			Serial.print(F("User password is "));
			Serial.println(userPass);
#endif			
			// Compare the password
			if(!strcmp(userPass, password)) {
				Serial.println(F("Password matched"));
				
				// Find the entry from the phonebook
				// index = AT+CPBF="mobileNumber"
				int index = checkIfNumberAuthorized(newMobileNumber);
				
				if(index <= 0) {
					// Make an entry into phonebook
					//AT+CPBW=PBEntryIndex,mobileNumber,145,"TTmobileNumber"
					PBEntryIndex++;
					sim900_flush_serial();
					sim900_send_cmd(F("AT+CPBW="));
					itoa(PBEntryIndex, num, 10);
					sim900_send_cmd(num);
					Serial.println(num);
					sim900_send_cmd(F(",\"+91"));
					sim900_send_cmd(newMobileNumber);
					sim900_send_cmd(F("\",145,\"TT+91"));
					sim900_send_cmd(newMobileNumber);
					sim900_send_cmd(F("\"\r\n"));

#ifdef DEBUG
					Serial.println(gsmBuffer);
#endif
					sim900_wait_for_resp("OK\r\n", CMD);
		
#ifdef DEBUG		
					Serial.print(F("Mobile Number "));
					Serial.print(newMobileNumber);
					Serial.println(F(" Authorised"));
#endif
					gsm.sendSMS(mobileNumber, "Mobile Number Authorised");
					// Save the index in EEPROM
					EEPROM.update(PB_ENTRY_INDEX_LOCATION, PBEntryIndex);
				}
				else {
#ifdef DEBUG
					Serial.println(F("number already authorised"));
#endif
					gsm.sendSMS(mobileNumber, "mobile number already authorised");
				}
			}
			else {
#ifdef DEBUG
					Serial.println(F("Password did not match"));
#endif
					gsm.sendSMS(mobileNumber, "Wrong password");
			}
		}
		else if(NULL != ( s = strstr(message,"REMOVE"))) {
#ifdef DEBUG		
			Serial.println(F("Removing the number from memory"));
			Serial.print(F("Entry index is "));
			Serial.println(PBEntryIndex);
#endif
			// REMOVE 1234567890 ABCDEF
			// Extract the mobile number from string
			s = s + 7; // we are on the first digit of mobile number
			p = strstr((char *)(s)," "); // p is pointing to <space> of the message string
			if(NULL != s) {
				i = 0;
				while (s < p) {
					newMobileNumber[i++] = *(s++);
				}
				newMobileNumber[i] = '\0';
			}
#ifdef DEBUG
			Serial.print(F("New mobile number is "));
			Serial.println(newMobileNumber);
#endif
			// get the password
			char userPass[7];
			s++; //now s is pointing to first character of password
			i = 0;
			while(i < 6){
				userPass[i++] = *(s++);
			}
			userPass[i] = '\0';
#ifdef DEBUG			
			Serial.print(F("User password is "));
			Serial.println(userPass);
#endif			
			// Compare the passwords
			if(!strcmp(userPass, password)) {
				Serial.println(F("Password matched"));
				// Find the entry from the phonebook
				// index = AT+CPBF="mobileNumber"
				int index = checkIfNumberAuthorized(newMobileNumber);

				if(index > 0) {
#ifdef DEBUG		
					Serial.print(newMobileNumber);
					Serial.println(F(" is authorised"));
					Serial.println(F("deleting the number"));
#endif
					// Delete the entry from the phonebook
					// AT+CPBW=index
					sim900_flush_serial();
					sim900_send_cmd(F("AT+CPBW="));
					itoa(index, num, 10);
					sim900_send_cmd(num);
					sim900_send_cmd(F("\r\n"));

					sim900_wait_for_resp("OK\r\n", CMD);
#ifdef DEBUG									
					Serial.print(F("Mobile Number "));
					Serial.print(newMobileNumber);
					Serial.println(F(" Unauthorised"));
#endif
					gsm.sendSMS(mobileNumber, "Mobile Number unauthorised");

					// PBEntryIndex == index
					if(PBEntryIndex == index){
						PBEntryIndex--;
#ifdef DEBUG						
						Serial.println(F("new PB index is "));
						Serial.println(PBEntryIndex);
#endif						
						// Save the index in EEPROM
						EEPROM.update(PB_ENTRY_INDEX_LOCATION, 	PBEntryIndex);
					}
				}
				else {
#ifdef DEBUG					
					Serial.println(F("invalid number"));
#endif					
					gsm.sendSMS(mobileNumber, "invalid mobile number");
				}
				
			}
			else {
#ifdef DEBUG				
				Serial.println(F("Password did not match"));
#endif			
				gsm.sendSMS(mobileNumber, "Wrong password");
			}
		}
		else {
#ifdef DEBUG
			Serial.println(F("invalid authorization command"));
#endif
		}
	}
	Serial.println(F("SMS handling done"));
}

bool getNumberFromString(char *inComingString, char *mobileNumber) {
	byte i = 0;
	char *p, *p2, *s;
	
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
#ifdef DEBUG		
		Serial.print(F("Mobile number obtained "));
		Serial.println(mobileNumber);
#endif
		return true;
	}
	return false;
}

int checkIfNumberAuthorized(char *mobileNumber) {
	// check this mobile number with the saved numbers and return the index if found
	// Searching for the mobile number
	char *s, *p;
	char index[4];
	byte i = 0;
	// AT+CPBF="mobileNumber"
	sim900_send_cmd(F("AT+CPBF=\""));
	sim900_send_cmd(mobileNumber);
	sim900_send_cmd(F("\"\r\n"));
	sim900_clean_buffer(gsmBuffer, sizeof(gsmBuffer));
	sim900_read_buffer(gsmBuffer, sizeof(gsmBuffer));
	
	// +CPBF: 1,"+91XXXXXXXXXX",145,"TT+91XXXXXXXXXX"
	// If number is present
	if(NULL != ( s = strstr(gsmBuffer, "+CPBF:"))) {
		s = s + 7;
		p = strstr((char *)(s), ","); //point to first ,
		if(NULL != s) {
			i = 0;
			while (s < p) {
				index[i++] = *(s++);
			}
			index[i] = '\0';
		}
		Serial.println(F("number is authorized"));
		return atoi(index);	
	}
	else {
		Serial.println(F("number is not authorized"));
		return -1;
	}
}
