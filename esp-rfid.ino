/* ------------------ RFID ESP Door lock ---------------
   
   wiring the MFRC522 to ESP8266 (ESP-12)
	RST	 = GPIO5
	SDA(SS) = GPIO4 
	MOSI	= GPIO13
	MISO	= GPIO12
	SCK	 = GPIO14
	GND	 = GND
	3.3V	= 3.3V
   ----------------------------------------------------- */

/* ------------------ Includes ------------------------- */
#include <ESP8266WiFi.h>		// Base of the whole project
#include <ESP8266mDNS.h>		// Zero-config Library (Bonjour, Avahi) http://esp-rfid.local
#include <DNSServer.h>			// Used for captive portal
#include <WiFiUdp.h>			// Library for manipulating UDP packets which is used by NTP Client to get Timestamps
#include <NtpClientLib.h>		// To timestamp RFID scans we get Unix Time from NTP Server
#include <SPI.h>				// SPI library to communicate with the peripherals as well as SPIFFS
#include <MFRC522.h>			// RFID library
#include <FS.h>					// SPIFFS Library for access to the onboard storage
#include <ArduinoJson.h>		// JSON Library for Encoding and Parsing Json object to send browser. We do that because Javascript has built-in JSON parsing.
#include <ESPAsyncTCP.h>		// Async TCP Library is mandatory for Async Web Server
#include <ESPAsyncWebServer.h>	// Async Web Server with built-in WebSocket Plug-in
#include <SPIFFSEditor.h>		// This creates a web page on server which can be used to edit text based files.
#include <TimeLib.h>			// Library for converting epochtime to a date
#include <SD.h>					// SD card library for storing the log
#ifdef ESP8266
extern "C" {
#include "user_interface.h"		// Used to get Wifi status information
}
#endif

/* ------------------ Definitions ---------------------- */
#define greenLed 5
#define redLed 4
#define blueLed 0

/* ------------------ Object declarations -------------- */
MFRC522 mfrc522 = MFRC522(); // Create MFRC522 instance
// Create AsyncWebServer instance on port "80"
AsyncWebServer server(80);
// Create WebSocket instance on URL "/ws"
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

DNSServer dnsServer;

// Create UDP instance for NTP Client
WiFiUDP ntpUDP;

/* ------------------ Variables ------------------------ */
String admin_pass;
const char *auth_pass;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

// Variables for whole scope
String filename = "/P/";
//flag to use from web update to reboot the ESP
bool shouldReboot = false;

bool inAPMode = false;
bool SDAvailable = false;
bool denyAcc = false;
bool activateRelay = false;
unsigned long previousMillis = 0;
int activateTime, timeZone, loggingOption = 0, relayPin, relayType;

extern "C" uint32_t _SPIFFS_start;
extern "C" uint32_t _SPIFFS_end;

/* ------------------ Setup ---------------------------- */
void setup() {
	Serial.begin(115200);	// Initialize serial communications
	Serial.println();
	Serial.println(F("[ INFO ] ESP RFID v0.2"));

	// Start SPIFFS filesystem
	SPIFFS.begin();

	if (loadConfiguration()) Serial.println(F("[ INFO ] Loaded configuration"));
	else { Serial.println(F("[ WARN ] Failed to load configuration")); return;}

	pinMode(relayPin, OUTPUT);
	pinMode(redLed, OUTPUT);
	pinMode(blueLed, OUTPUT);
	if (relayType == 1) digitalWrite(relayPin, LOW);
	else digitalWrite(relayPin, HIGH);

	turnOnLed(blueLed);
}

/* ------------------ Main loop ------------------------ */
void loop() {
	// check for a new update and restart
	if (shouldReboot) {
		Serial.println(F("[ UPDT ] Rebooting..."));
		delay(100);
		ESP.restart();
	}

	if (inAPMode) dnsServer.processNextRequest();

	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= activateTime && activateRelay) {
		activateRelay = false;
		if (relayType == 1) digitalWrite(relayPin, LOW);
		else digitalWrite(relayPin, HIGH);
		turnOnLed(blueLed);
	}
	if (currentMillis - previousMillis >= activateTime && denyAcc) {
		denyAcc = false;
		turnOnLed(blueLed);
	}
	if (activateRelay) {
		turnOnLed(greenLed);
		if (relayType == 1) digitalWrite(relayPin, HIGH);
		else digitalWrite(relayPin, LOW);
	}

	// Another loop for RFID Events, since we are using polling method instead of Interrupt we need to check RFID hardware for events
	rfidloop();	
}

/* ------------------ RFID Functions ------------------- */
// RFID Specific Loop
void rfidloop() {
	//If a new PICC placed to RFID reader continue
	if ( ! mfrc522.PICC_IsNewCardPresent()) {
		delay(50);
		return;
	}
	//Since a PICC is placed get Serial (UID) and continue
	if ( ! mfrc522.PICC_ReadCardSerial()) {
		delay(50);
		return;
	}
	// We got UID tell PICC to stop responding
	mfrc522.PICC_HaltA();

	// There are Mifare PICCs which have 4 byte or 7 byte UID
	// Get PICC's UID and store on a variable
	Serial.print(F("[ INFO ] PICC's UID: "));
	String uid = "";
	for (int i = 0; i < mfrc522.uid.size; ++i) {
		uid += String(mfrc522.uid.uidByte[i], HEX);
	}
	Serial.print(uid);
	// Get PICC type
	MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
	String type = mfrc522.PICC_GetTypeName(piccType);

	// We are going to use filesystem to store known UIDs.
	int isKnown = 0;  // First assume we don't know until we got a match
	// If we know the PICC we need to know if its User have an Access
	int accType = 0;  // First assume User do not have access
	String timedAccess = "";
	//Log functionality
	bool gaveAccess = false;
	String logUsername = "unknown";
	// Prepend /P/ on filename so we distinguish UIDs from the other files
	filename = "/P/";
	filename += uid;

	fs::File f = SPIFFS.open(filename, "r");
	// Check if we could find it above function returns true if the file is exists
	if (f) {
		isKnown = 1; // we found it and label it as known
		// Now we need to read contents of the file to parse JSON object contains Username and Access Status
		size_t size = f.size();
		// Allocate a buffer to store contents of the file.
		std::unique_ptr<char[]> buf(new char[size]);
		// We don't use String here because ArduinoJson library requires the input
		// buffer to be mutable. If you don't use ArduinoJson, you may as well
		// use configFile.readString instead.
		f.readBytes(buf.get(), size);
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(buf.get());
		// Check if we succesfully parse JSON object
		if (json.success()) {
			// Get username Access Status
			String username = json["user"];
			logUsername = username;
			accType = json["accType"];
			String validdate = json["validDate"];
			bool isValid = false;
			Serial.println(" = known PICC");
			Serial.print("[ INFO ] User Name: ");
			Serial.print(username);
			// Check if user have an access
			if (validdate != "") {
				String date = getDate();
				//Date comes in format yyyy-mm-dd
				int currentDate[3] {date.substring(0,4).toInt(), date.substring(5,7).toInt(), date.substring(8).toInt()};
				int validationDate[3] {validdate.substring(0,4).toInt(), validdate.substring(5,7).toInt(), validdate.substring(8).toInt()};
				if (currentDate[0] < validationDate[0]) isValid = true;
				if (currentDate[0] = validationDate[0]) {
					if (currentDate[1] < validationDate[1]) isValid = true;
					if (currentDate[1] = validationDate[1] && currentDate[2] <= validationDate[2]) isValid = true;
				}
			}
			else isValid = true;
			if (isValid) {
				if (accType == 1) {
					gaveAccess = true;
					allowAccess();
				}
				else if (accType == 0) {
					gaveAccess = false;
					denyAccess();
				}
				else if (accType == 2) {
					//Check timed access
					// 0:sunday, 1:monday etc
					//Format: (0_12:00-24:00 1_08:00-16:00)
					const char* timedAccessBuffer = json["timedAcc"];
					timedAccess = String(timedAccessBuffer);
					int dayCount = (timedAccess.length()/13);
					int allowedDays[dayCount];
					for (int i = 0; i < dayCount; i++) {
						allowedDays[i] = String(timedAccess.charAt(i*14)).toInt();
					}
					unsigned long epochTime = now();
					int currentDay = ((epochTime / 86400L) + 4 ) % 7;
					bool checkTime = false;
					int checkTimeOnPos = 0;
					//Check if current day is one of the allowed days
					for (int i = 0; i < dayCount; i++)
					{
						if (currentDay == allowedDays[i]) {
							checkTime = true;
							checkTimeOnPos = i;
							break;
						} 
					}
					if (checkTime) {
						String fromTime = timedAccess.substring((checkTimeOnPos*14) + 2,(checkTimeOnPos*14) + 7);
						String untillTime = timedAccess.substring((checkTimeOnPos*14) + 8,(checkTimeOnPos*14) + 13);
						unsigned long epochTime = now();
						int currentHour = (epochTime % 86400L) / 3600, currentMinute = (epochTime % 3600) / 60;
						if ((fromTime.substring(0,2).toInt()) < currentHour && (untillTime.substring(0,2).toInt()) > currentHour) { allowAccess(); gaveAccess = true; }//Within the hours
						else if ((fromTime.substring(0,2).toInt()) == currentHour && (fromTime.substring(3).toInt()) < currentMinute) { allowAccess(); gaveAccess = true; }//Same hour as from time check the minutes
						else if ((untillTime.substring(0,2).toInt()) == currentHour && (untillTime.substring(3).toInt()) > currentMinute) { allowAccess(); gaveAccess = true; }//Same hour as untill time check the minutes
						else { denyAccess(); gaveAccess = false; }
					}
					else {
						gaveAccess = false;
						denyAccess();
					}
				}
			}
			else {
				gaveAccess = false;
				denyAccess();
			}
			// Also inform Administrator Portal
			// Encode a JSON Object and send it to All WebSocket Clients
			DynamicJsonBuffer jsonBuffer2;
			JsonObject& root = jsonBuffer2.createObject();
			root["command"] = "piccscan";
			// UID of Scanned RFID Tag
			root["uid"] = uid;
			// Type of PICC
			root["type"] = type;
			// A boolean 1 for known tags 0 for unknown
			root["known"] = isKnown;
			// An int 1 for granted 0 for denied access, 2 for timed
			root["accType"] = accType;
			// Username
			root["user"] = username;
			// Timed access
			if (accType == 2) root["timedAcc"] = timedAccess;
			else root["timedAcc"] = "";
			root["validDate"] = validdate;
			size_t len = root.measureLength();
			AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
			if (buffer) {
				root.printTo((char *)buffer->get(), len + 1);
				ws.textAll(buffer);
			  }
		}
		else {
			Serial.println("");
			Serial.println(F("[ WARN ] Failed to parse User Data"));
		}
	f.close();
	}
	else {
		// If we don't know the UID, inform Administrator Portal so admin can give access or add it to database
		Serial.print(" = unknown PICC");
		gaveAccess = false;
		denyAccess();
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		root["command"] = "piccscan";
		// UID of Scanned RFID Tag
		root["uid"] = uid;
		// Type of PICC
		root["type"] = type;
		// A boolean 1 for known tags 0 for unknown
		root["known"] = isKnown;
		root["validDate"] = "";
		size_t len = root.measureLength();
		AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
		if (buffer) {
			root.printTo((char *)buffer->get(), len + 1);
			ws.textAll(buffer);
		}
	}
	// So far got we got UID of Scanned RFID Tag, checked it if it's on the database and access status, informed Administrator Portal
	if (loggingOption != 0) {
		String dataString = getTime() + "," + uid + "," + logUsername + "," + gaveAccess;
		String logfile = getDate();
		logfile.remove(7,1);
		logfile.remove(4,1);
		if (loggingOption == 1) createLogSD(dataString,logfile);
		else if (loggingOption == 2) createLogSPIFFS(dataString, logfile);
	}
}

void allowAccess() {
	activateRelay = true;  // Give user Access to Door, Safe, Box whatever you like
	previousMillis = millis();
	Serial.println(" has access");
}
	
void denyAccess() {
	turnOnLed(redLed);
	denyAcc = true;
	previousMillis = millis();
	Serial.println(" does not have access");
}

// Configure RFID Hardware
void setupRFID(int rfid_ss, int rfid_gain) {
	SPI.begin();		   // MFRC522 Hardware uses SPI protocol
	mfrc522.PCD_Init(rfid_ss, UINT8_MAX);	// Initialize MFRC522 Hardware
	// Set RFID Hardware Antenna Gain
	// This may not work with some boards
	mfrc522.PCD_SetAntennaGain(rfid_gain);
	Serial.printf("[ INFO ] RFID SS_PIN: %u and Gain Factor: %u", rfid_ss, rfid_gain);
	Serial.println("");
	ShowReaderDetails(); // Show details of PCD - MFRC522 Card Reader details
}

void ShowReaderDetails() {
	// Get the MFRC522 software version
	byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
	Serial.print(F("[ INFO ] MFRC522 Version: 0x"));
	Serial.print(v, HEX);
	if (v == 0x91)
		Serial.print(F(" = v1.0"));
	else if (v == 0x92)
		Serial.print(F(" = v2.0"));
	else if (v == 0x88)
		Serial.print(F(" = clone"));
	else
		Serial.print(F(" (unknown)"));
	Serial.println("");
	// When 0x00 or 0xFF is returned, communication probably failed
	if ((v == 0x00) || (v == 0xFF)) {
		Serial.println(F("[ WARN ] Communication failure, check if MFRC522 properly connected"));
	}
}

/* ------------------ WiFi Functions ------------------- */
// Try to connect Wi-Fi
bool connectSTA(const char* sta_ssid, const char* sta_pass, byte bssid[6]) {
	WiFi.mode(WIFI_STA);
	// First connect to a wi-fi network
	WiFi.begin(sta_ssid, sta_pass, 0, bssid);
	// Inform user we are trying to connect
	Serial.print(F("[ INFO ] Trying to connect WiFi: "));
	Serial.print(sta_ssid);
	// We try it for 20 seconds and give up on if we can't connect
	unsigned long now = millis();
	uint8_t timeout = 20; // define when to time out in seconds
	// Wait until we connect or 20 seconds pass
	do {
		if (WiFi.status() == WL_CONNECTED) {
			break;
		}
		delay(500);
		Serial.print(F("."));
	}
	while (millis() - now < timeout * 1000);
	// We now out of the while loop, either time is out or we connected. check what happened
	if (WiFi.status() == WL_CONNECTED) { // Assume time is out first and check
		Serial.println();
		Serial.print(F("[ INFO ] Client IP address: ")); // Great, we connected, inform
		Serial.println(WiFi.localIP());
		return true;
	}
	else { // We couln't connect, time is out, inform
		Serial.println();
		Serial.println(F("[ WARN ] Couldn't connect in time"));
		return false;
	}
}

// Fallback to AP Mode, so we can connect to ESP if there is no Internet connection
bool setupAP(const char* ap_ssid, const char* ap_pass) {
	WiFi.mode(WIFI_AP);
	Serial.println(F("[ INFO ] Configuring access point... "));
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(53, "*", apIP);
	bool result = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	Serial.print(F("[ INFO ] Captive portal setup: "));
	Serial.println(result ? "Ready" : "Failed!");
	Serial.print(F("[ INFO ] Setting up access point: "));
	result = WiFi.softAP(ap_ssid, ap_pass);
	Serial.println(result ? "Ready" : "Failed!");
	// Access Point IP
	IPAddress myIP = WiFi.softAPIP();
	inAPMode = true;
	Serial.print(F("[ INFO ] AP IP address: "));
	Serial.println(myIP);
	Serial.print(F("[ INFO ] AP SSID: "));
	Serial.println(ap_ssid);
	Serial.print(F("[ INFO ] AP PASS: "));
	Serial.println(ap_pass);
	return result;
}

boolean captivePortal(AsyncWebServerRequest *request) {
	if (!isIp(request->host()) ) {
		AsyncWebServerResponse *response = request->beginResponse(302,"text/plain","");
		response->addHeader("Location", String("http://" + ipToString(apIP)));
		request->send(response);
		return true;
	}
	return false;
}

boolean isIp(String str) {
	for (int i = 0; i < str.length(); i++) {
		int c = str.charAt(i);
		if (c != '.' && (c < '0' || c > '9')) {
			return false;
		}
	}
	return true;
}

String ipToString(IPAddress ip) {
	String res = "";
	for (int i = 0; i < 3; i++) {
		res += String((ip >> (8 * i)) & 0xFF) + ".";
	}
	res += String(((ip >> 8 * 3)) & 0xFF);
	return res;
}

/* ------------------ SPIFFS Functions ----------------- */
bool loadConfiguration() {
	fs::File configFile = SPIFFS.open("/auth/config.json", "r");
	if (!configFile) {
		Serial.println(F("[ WARN ] Failed to open config file"));
		return false;
	}
	size_t size = configFile.size();
	// Allocate a buffer to store contents of the file.
	std::unique_ptr<char[]> buf(new char[size]);
	// We don't use String here because ArduinoJson library requires the input
	// buffer to be mutable. If you don't use ArduinoJson, you may as well
	// use configFile.readString instead.
	configFile.readBytes(buf.get(), size);
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(buf.get());
	if (!json.success()) {
		Serial.println(F("[ WARN ] Failed to parse config file"));
		return false;
	}

	//Webserver variables
	const char *admin_pass_buffer = json["auth_pass"];
	admin_pass = String(admin_pass_buffer);
	auth_pass = const_cast<char*>(admin_pass.c_str());

	//Wifi variables
	const char *wifi_hostname = json["wifi_hostname"];
	const char *ap_ssid = json["ap_ssid"];
	const char *ap_pass = json["ap_pass"];
	const char *sta_ssid = json["sta_ssid"];
	const char *sta_pass = json["sta_pass"];
	int wifimode = json["wifimode"];
	const char * bssidmac = json["sta_bssid"];
	byte bssid[6];
	parseBytes(bssidmac, ':', bssid, 6, 16);

	//Time variables
	const char * ntpserver = json["ntpserver"];
	int ntpinter = json["ntpinterval"];
	int timeZone = json["timezone"];

	//Hardware variables
	int SD_SS = json["sd_ss"];
	loggingOption = json["create_log"];
	int MFRC522_SS = json["rfid_ss"];
	int rfidgain = json["rfid_gain"];
	relayPin = json["relay_gpio"];
	relayType = json["relay_type"];
	activateTime = json["relay_time"];
	
	configFile.close();

	WiFi.hostname(wifi_hostname);
	  
	if (wifimode == 1) {
		Serial.println(F("[ INFO ] ESP-RFID is running in AP Mode "));
		if (!setupAP(ap_ssid, ap_pass)) return false;
	}
	else {
		if (!connectSTA(sta_ssid, sta_pass, bssid)){ // If unable to connect to WiFi setup Access point
			if (!setupAP(ap_ssid, ap_pass)) return false;
		  }
	}

	setupRFID(MFRC522_SS,rfidgain);
	setupWebserver();

	// Start mDNS service so we can connect to http://esp-rfid.local (if Bonjour installed on Windows or Avahi on Linux)
	if (!MDNS.begin(wifi_hostname)) {
		Serial.println(F("Error setting up MDNS responder!"));
	}
	// Add Web Server service to mDNS
	MDNS.addService("http", "tcp", 80);

	NTP.begin(ntpserver, timeZone);
	NTP.setInterval(ntpinter * 60); // Poll every x minutes

	if (!SD.begin(SD_SS,SPI_QUARTER_SPEED)) {
		SDAvailable = false;
		Serial.println(F("[ INFO ] SD Card failed to connect or not present"));
	}
	else {
		SDAvailable = true;
		Serial.println(F("[ INFO ] SD Card initialized"));
	}

	if (SDAvailable) {
		Serial.print(F("[ INFO ] SD Card type: "));
		switch (SD.type()) {
			case SD_CARD_TYPE_SD1:
				Serial.println("SD1");
				break;
			case SD_CARD_TYPE_SD2:
				Serial.println("SD2");
				break;
			case SD_CARD_TYPE_SDHC:
				Serial.println("SDHC");
				break;
			default:
				Serial.println("Unknown");
		  }
		Serial.print(F("[ INFO ] SD Card Fat Type: FAT"));
		Serial.println(SD.fatType());
	}

	return true;
}

/* ------------------ Webserver Functions -------------- */
void setupWebserver(){
	// Start WebSocket Plug-in and handle incoming message on "onWsEvent" function
	server.addHandler(&ws);
	ws.onEvent(onWsEvent);

	// Configure web server
	// Add Text Editor (http://esp-rfid.local/edit) to Web Server. This feature likely will be dropped on final release.
	server.addHandler(new SPIFFSEditor("admin", admin_pass));

	// Serve confidential files in /auth/ folder with a Basic HTTP authentication
	server.serveStatic("/auth/", SPIFFS, "/auth/").setDefaultFile("users.htm").setAuthentication("admin", auth_pass);
	// Serve all files in root folder
	server.serveStatic("/", SPIFFS, "/");
	// Handle what happens when requested web file couldn't be found

	// Simple Firmware Update Handler
	server.on("/auth/update", HTTP_POST, [](AsyncWebServerRequest * request) {
		shouldReboot = !Update.hasError();
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
		response->addHeader("Connection", "close");
		request->send(response);
	}, [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
	if (!index) {
		Serial.printf("[ UPDT ] Firmware update started: %s\n", filename.c_str());
		Update.runAsync(true);
		if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
			Update.printError(Serial);
		}
	}
	if (!Update.hasError()) {
		if (Update.write(data, len) != len) {
			Update.printError(Serial);
		}
	}
	if (final) {
		if (Update.end(true)) {
			Serial.printf("[ UPDT ] Firmware update finished: %uB\n", index + len);
		} else {
			Update.printError(Serial);
		}
	}
	});

	// Simple SPIFFs Update Handler
	server.on("/auth/spiupdate", HTTP_POST, [](AsyncWebServerRequest * request) {
		shouldReboot = !Update.hasError();
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
		response->addHeader("Connection", "close");
		request->send(response);
	}, [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
	if (!index) {
		Serial.printf("[ UPDT ] SPIFFS update started: %s\n", filename.c_str());
		Update.runAsync(true);
		size_t spiffsSize = ((size_t) &_SPIFFS_end - (size_t) &_SPIFFS_start);
		if (!Update.begin(spiffsSize, U_SPIFFS)) {
			Update.printError(Serial);
		}
	}
	if (!Update.hasError()) {
		if (Update.write(data, len) != len) {
			Update.printError(Serial);
		}
	}
	if (final) {
		if (Update.end(true)) {
			Serial.printf("[ UPDT ] SPIFFS update finished: %uB\n", index + len);
		} else {
			Update.printError(Serial);
		}
	}
	});

	  server.onNotFound([](AsyncWebServerRequest *request) {
		if (captivePortal(request)) { // If captive portal redirect instead of displaying the error page.
			return;
		}
  
		String message = "File Not Found\n\n";
		message += "URI: ";
		message += request->url();
		message += "\nMethod: ";
		message += ( request->method() == HTTP_GET ) ? "GET" : "POST";
		message += "\nArguments: ";
		message += request->args();
		message += "\n";

		for (uint8_t i = 0; i < request->args(); i++ ) {
			message += " " + request->argName ( i ) + ": " + request->arg ( i ) + "\n";
		}
		  
		AsyncWebServerResponse *response = request->beginResponse(404,"text/plain",message);
		response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
		response->addHeader("Pragma", "no-cache");
		response->addHeader("Expires", "-1");
		request->send(response); 
	});

	// Start Web Server
	server.begin();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
	if (type == WS_EVT_ERROR) {
		Serial.printf("[ WARN ] WebSocket[%s][%u] error(%u): %s\r\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
	}
	else if (type == WS_EVT_DATA) {
		AwsFrameInfo * info = (AwsFrameInfo*)arg;
			String msg = "";

		if (info->final && info->index == 0 && info->len == len) {
			//the whole message is in a single frame and we got all of it's data
			for (size_t i = 0; i < info->len; i++) {
				msg += (char) data[i];
			}

			// We should always get a JSON object (stringfied) from browser, so parse it
			DynamicJsonBuffer jsonBuffer;
			JsonObject& root = jsonBuffer.parseObject(msg);
			if (!root.success()) {
				Serial.println(F("[ WARN ] Couldn't parse WebSocket message"));
				return;
			}

			// Web Browser sends some commands, check which command is given
			const char * command = root["command"];
		  
			// Check whatever the command is and act accordingly
			if (strcmp(command, "remove")  == 0) {
				const char* uid = root["uid"];
				filename = "/P/";
				filename += uid;
				SPIFFS.remove(filename);
			}
			else if (strcmp(command, "configfile")  == 0) {
				fs::File f = SPIFFS.open("/auth/config.json", "w+");
				if (f) {
					root.prettyPrintTo(f);
					f.close();
					ESP.reset();
				}
			}
			else if (strcmp(command, "picclist")  == 0) {
				sendPICClist();
			}
			else if (strcmp(command, "status")  == 0) {
				sendStatus();
			}
			else if (strcmp(command, "userfile")  == 0) {
				const char* uid = root["uid"];
				filename = "/P/";
				filename += uid;
				fs::File f = SPIFFS.open(filename, "w+");
				// Check if we created the file
				if (f) {
					f.print(msg);
					f.close();
				}
			}
			else if (strcmp(command, "testrelay")  == 0) {
				activateRelay = true;
				previousMillis = millis();
			}
			else if (strcmp(command, "scan")  == 0) {
				WiFi.scanNetworksAsync(printScanResult);
			}
			else if (strcmp(command, "gettime")  == 0) {
				sendTime();
			}
			else if (strcmp(command, "settime")  == 0) {
				unsigned long t = root["epoch"];
				setTime(t);
				sendTime();
			}
			else if (strcmp(command, "getconf")  == 0) {
				fs::File configFile = SPIFFS.open("/auth/config.json", "r");
				if (configFile) {
					size_t len = configFile.size();
					AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
					if (buffer) {
						configFile.readBytes((char *)buffer->get(), len + 1);
						ws.textAll(buffer);
					}
					configFile.close();
				}
			}
			else if (strcmp(command, "datelist")  == 0) {
				if (loggingOption == 1) listLogsSD();
				else if (loggingOption == 2) listLogsSPIFFS();
			}
			else if (strcmp(command, "loglist") ==0) {
				if (loggingOption == 1) readLogSD(root["msg"]);
				else if (loggingOption == 2) readLogSPIFFS(root["msg"]);
			}
			else if (strcmp(command, "remlog") ==0) {
				if (loggingOption == 1) deleteLogSD(root["msg"]);
				else if (loggingOption == 2) deleteLogSPIFFS(root["msg"]);
			}
		}   
	}
}

void sendPICClist() {
	Dir dir = SPIFFS.openDir("/P/");
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["command"] = "picclist";

	JsonArray& data = root.createNestedArray("piccs");
	JsonArray& data2 = root.createNestedArray("users");
	JsonArray& data3 = root.createNestedArray("accType");
	JsonArray& data4 = root.createNestedArray("timedAcc");
	JsonArray& data5 = root.createNestedArray("validDate");
	while (dir.next()) {
		fs::File f = SPIFFS.open(dir.fileName(), "r");
		size_t size = f.size();
		// Allocate a buffer to store contents of the file.
		std::unique_ptr<char[]> buf(new char[size]);
		// We don't use String here because ArduinoJson library requires the input
		// buffer to be mutable. If you don't use ArduinoJson, you may as well
		// use configFile.readString instead.
		f.readBytes(buf.get(), size);
		DynamicJsonBuffer jsonBuffer2;
		JsonObject& json = jsonBuffer2.parseObject(buf.get());
		if (json.success()) {
			String username = json["user"];
			int accType = json["accType"];
			String timedAccess = json["timedAcc"];
			String validdate = json["validDate"];
			data2.add(username);
			data3.add(accType);
			data4.add(timedAccess);
			data5.add(validdate);
		}
		data.add(dir.fileName());
		f.close();
	}
	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
	}
}

void sendTime() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["command"] = "gettime";
	root["epoch"] = now();
	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
	}
}

void sendStatus() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	int mode = WiFi.getMode(); //const char* modes[] = { "NULL", "STA", "AP", "STA+AP" };

	FSInfo fsinfo;
	if (!SPIFFS.info(fsinfo)) {
		Serial.print(F("[ WARN ] Error getting info on SPIFFS"));
	}

	struct ip_info info;

	if (mode == 1) { //Station mode
		wifi_get_ip_info(STATION_IF, &info);
		struct station_config conf;
		wifi_station_get_config(&conf);
		root["ssid"] = String(reinterpret_cast<char*>(conf.ssid));
		root["dns"] = printIP(WiFi.dnsIP());
		root["mac"] = WiFi.macAddress();
	} else if (mode == 2) { //SoftAP mode
		wifi_get_ip_info(SOFTAP_IF, &info);
		struct softap_config conf;
		wifi_softap_get_config(&conf);
		root["ssid"] = String(reinterpret_cast<char*>(conf.ssid));
		root["dns"] = printIP(WiFi.softAPIP()); //TODO: Get dns from config or info
		root["mac"] = WiFi.softAPmacAddress();
	}
	IPAddress ipaddr = IPAddress(info.ip.addr);
	IPAddress gwaddr = IPAddress(info.gw.addr);
	IPAddress nmaddr = IPAddress(info.netmask.addr);
	root["ip"] = printIP(ipaddr);
	root["gateway"] = printIP(gwaddr);
	root["netmask"] = printIP(nmaddr);

	root["command"] = "status";
	root["heap"] = ESP.getFreeHeap();
	root["chipid"] = String(ESP.getChipId(), HEX);
	root["cpu"] = ESP.getCpuFreqMHz();
	root["availsize"] = ESP.getFreeSketchSpace();
	root["availspiffs"] = fsinfo.totalBytes - fsinfo.usedBytes;
	root["spiffssize"] = fsinfo.totalBytes;
	
	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
	}
}

String printIP(IPAddress adress) {
	return (String)adress[0] + "." + (String)adress[1] + "." + (String)adress[2] + "." + (String)adress[3];
}

// Send Scanned SSIDs to websocket clients as JSON object
void printScanResult(int networksFound) {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["command"] = "ssidlist";
	JsonArray& data = root.createNestedArray("ssid");
	JsonArray& data2 = root.createNestedArray("bssid");
	for (int i = 0; i < networksFound; ++i) {
		// Print SSID for each network found
		data.add(WiFi.SSID(i));
		data2.add(WiFi.BSSIDstr(i));
	}
	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
	}
	WiFi.scanDelete();
}

/* ------------------ Date and Time Functions ---------- */
String getTime() {
	unsigned long epochTime = now();
	unsigned long hours = (epochTime % 86400L) / 3600;
	String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);
	unsigned long minutes = (epochTime % 3600) / 60;
	String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);
	unsigned long seconds = epochTime % 60;
	String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

	return hoursStr + ":" + minuteStr + ":" + secondStr;
}

String getDate() {
	unsigned long epochTime = now();
	uint8_t dateMonth = month(epochTime);
	uint8_t dateDay = day(epochTime);
	String dateTime =  String(year(epochTime)) + "-";
	if (dateMonth < 10) dateTime += "0" + String(dateMonth) + "-";
	else dateTime += String(dateMonth) + "-";
	if (dateDay < 10) dateTime += "0" + String(dateDay);
	else dateTime += String(dateDay);
	return dateTime;
}

/* ------------------ Logging Functions ---------------- */
bool createLogSD(String dataString, String filename) {
	if (!SDAvailable) return false;
	if (!SD.exists("Data/")) SD.mkdir("Data/");
	sd::File f = SD.open("Data/" + filename, FILE_WRITE);
	if (f) {
		f.println(dataString);
		f.close();
		return true;
	}
	else {
		Serial.print(F("[ WARN ] Error writing to file on SD card: "));
		Serial.println(filename);
		return false;
	}
}

bool readLogSD(String filename) {
	if (!SDAvailable) return false;
	sd::File f = SD.open("Data/" + filename);
	if (!f) {
		Serial.print(F("[ WARN ] Error reading file on SD card: "));
		Serial.println(filename);
		return false;
	}

	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["command"] = "loglist";

	JsonArray& data = root.createNestedArray("time");
	JsonArray& data2 = root.createNestedArray("uid");
	JsonArray& data3 = root.createNestedArray("username");
	JsonArray& data4 = root.createNestedArray("granted");

	while (f.available()) {
		String databuffer = f.readStringUntil('\n');
		int seperatorIndex[3];
		for (int i = 0; i < 3; i++) {
			if (i == 0) seperatorIndex[i] = databuffer.indexOf(',');
			else seperatorIndex[i] = databuffer.indexOf(',', seperatorIndex[i-1]+1);
		}
		data.add(databuffer.substring(0,seperatorIndex[0]));
		data2.add(databuffer.substring(seperatorIndex[0]+1,seperatorIndex[1]));
		data3.add(databuffer.substring(seperatorIndex[1]+1,seperatorIndex[2]));
		data4.add(databuffer.substring(seperatorIndex[2]+1,seperatorIndex[2]+2));
	}

	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
		return true;
	}
	else return false;
}

bool deleteLogSD(String filename) {
	if (!SDAvailable) return false;
	if (!SD.exists("Data/" + filename)) return true;
	if (SD.remove("Data/" + filename)) return true;
	else {
		Serial.print(F("[ WARN ] Error removing file from SD card: "));
		Serial.println(filename);
		return false;
	}
}

bool listLogsSD() {
	if (!SDAvailable) return false;
	
	DynamicJsonBuffer jsonBuffer;
	JsonObject& jsonroot = jsonBuffer.createObject();
	jsonroot["command"] = "datelist";

	JsonArray& data = jsonroot.createNestedArray("date");

	sd::File root;
	root = SD.open("Data/");
	while (true) {
		sd::File entry = root.openNextFile();
		if (! entry) {
			// no more files
			break;
		}
		data.add((String)entry.name());
		entry.close();
	}

	size_t len = jsonroot.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		jsonroot.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
		return true;
	}
	else return false;
}

bool readUserLogSD(String UID) {
	if (!SDAvailable) return false;
	return true;
}

bool createLogSPIFFS(String dataString, String filename) {
	fs::File f;
	if (SPIFFS.exists("Data/" + filename)) f = SPIFFS.open("Data/" + filename, "r+");
	else f = SPIFFS.open("Data/" + filename,"w+");
	if (f) {
		f.seek(0,SeekEnd);
		f.println(dataString);
		f.close();
		return true;
	}
	else {
		Serial.print(F("[ WARN ] Error writing to file on SPIFFS: "));
		Serial.println(filename);
		return false;
	}
}

bool readLogSPIFFS(String filename) {
	fs::File f = SPIFFS.open("Data/" + filename, "r");
	if (!f) {
		Serial.print(F("[ WARN ] Error reading file on SPIFFS: "));
		Serial.println(filename);
		return false;
	}

	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["command"] = "loglist";

	JsonArray& data = root.createNestedArray("time");
	JsonArray& data2 = root.createNestedArray("uid");
	JsonArray& data3 = root.createNestedArray("username");
	JsonArray& data4 = root.createNestedArray("granted");

	while (f.available()) {
		String databuffer = f.readStringUntil('\n');
		int seperatorIndex[3];
		for (int i = 0; i < 3; i++) {
			if (i == 0) seperatorIndex[i] = databuffer.indexOf(',');
			else seperatorIndex[i] = databuffer.indexOf(',', seperatorIndex[i-1]+1);
		}
		data.add(databuffer.substring(0,seperatorIndex[0]));
		data2.add(databuffer.substring(seperatorIndex[0]+1,seperatorIndex[1]));
		data3.add(databuffer.substring(seperatorIndex[1]+1,seperatorIndex[2]));
		data4.add(databuffer.substring(seperatorIndex[2]+1,seperatorIndex[2]+2));
	}

	size_t len = root.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		root.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
		return true;
	}
	else return false;
}

bool deleteLogSPIFFS(String filename) {
	if (!SPIFFS.exists("Data/" + filename)) return true;
	if (SPIFFS.remove("Data/" + filename)) return true;
	else {
		Serial.print(F("[ WARN ] Error removing file from SPIFFS: "));
		Serial.println(filename);
		return false;
	}
}

bool listLogsSPIFFS() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& jsonroot = jsonBuffer.createObject();
	jsonroot["command"] = "datelist";

	JsonArray& data = jsonroot.createNestedArray("date");

	Dir dir = SPIFFS.openDir("Data/");
	while (dir.next()) {
		data.add((String)dir.fileName().substring(5));
	}

	size_t len = jsonroot.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
	if (buffer) {
		jsonroot.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
		return true;
	}
	else return false;
}

/* ------------------ Misc Functions ------------------- */
void turnOnLed(int pin) {
	switch (pin) {
		case greenLed:
			digitalWrite(redLed, LOW);
			digitalWrite(blueLed, LOW);
			  break;
		case redLed:
			digitalWrite(redLed, HIGH);
			digitalWrite(blueLed, LOW);
			  break;
		case blueLed:
			digitalWrite(redLed, LOW);
			digitalWrite(blueLed, HIGH);
			  break;
		default:
			break;
	}
}

void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) {
	for (int i = 0; i < maxBytes; i++) {
		bytes[i] = strtoul(str, NULL, base);// Convert byte
		str = strchr(str, sep);				// Find next separator
		if (str == NULL || *str == '\0') {
			break;							// No more separators, exit
		}
		str++;								// Point to next character after separator
	}
}
