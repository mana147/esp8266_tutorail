
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <MD5.h>


#define DEBUG 1


MDNSResponder mdns;
IPAddress l;
String st;
const int req_buffer_size = 20;
String req_value_buf[req_buffer_size];
String req_name_buf[req_buffer_size];
String ssid = "AMIN";
String password = "1234567890";
WiFiServer server(80);
const char* time_host = "google.com";


#define PRINTDEBUG(STR) \
  {  \
    if (DEBUG) Serial.println(STR); \
  }

String makeMD5(String text) {
	byte bbuff[16];
	String hash = "";
	MD5Builder nonce_md5;
	nonce_md5.begin();
	nonce_md5.add(text);
	nonce_md5.calculate();
	nonce_md5.getBytes(bbuff);
	for (byte i = 0; i < 16; i++) hash += byte2hex(bbuff[i]);
	return hash;
}

String byte2hex(byte Zahl) {
	String Hstring = String(Zahl, HEX);
	if (Zahl < 16) { Hstring = "0" + Hstring; }
	return Hstring;
}

String hashing(String in)
{
	String h = in;
	String md5 = makeMD5(h);
	return md5;
}

void setup() {
	//Enable Serial
	Serial.begin(921600);

	//Enable EEPROM
	EEPROM.begin(512);
	delay(10);

	// PRINTDEBUG("HELLO!");
	PRINTDEBUG();
	PRINTDEBUG();
	PRINTDEBUG("Startup");

	// read eeprom for ssid and pass
	PRINTDEBUG("Reading EEPROM ssid");
	// Read SSID EEPROM
	String esid;
	for (int i = 0; i < 32; ++i)
	{
		esid += char(EEPROM.read(i));
	}
	Serial.print("SSID: ");
	PRINTDEBUG(esid);
	PRINTDEBUG("Reading EEPROM pass");
	// Read password from EEPROM
	String epass = "";
	for (int i = 32; i < 96; ++i)
	{
		epass += char(EEPROM.read(i));
	}
	Serial.print("PASS: ");
	PRINTDEBUG(epass);
	PRINTDEBUG("Reading EEPROM IP");
	// Read ip from EEPROM
	String eip = "";
	for (int i = 96; i < 99; ++i)
	{
		eip += char(EEPROM.read(i));
	}
	eip = eip.c_str();
	Serial.print("IP: ");
	PRINTDEBUG(eip);
	int gate_ip = char(EEPROM.read(301));
	IPAddress ipad(192, 168, 1, eip.toInt());
	IPAddress gateway(192, 168, 1, 2);
	IPAddress subnet(255, 255, 255, 0);
	String chip_id = String(ESP.getFlashChipId());
	ssid = ssid + chip_id;

	if ((esid.length() > 2) && (eip.toInt()>199)) {
		// test esid 

		WiFi.begin(esid.c_str(), epass.c_str());
		WiFi.config(ipad, gateway, subnet);

		if (testWifi() == 20) {
			WiFi.mode(WIFI_STA);
			launchWeb(0);
			return;
		}
	}
	setupAP();
}

String send_web_http(String url, String url_host)
{
	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;

	if (!client.connect(url_host.c_str(), httpPort)) {
		PRINTDEBUG("connection failed");
		return "0-0";
	}

	// We now create a URI for the request

	PRINTDEBUG("Requesting URL: ");
	PRINTDEBUG(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + url_host + "\r\n" +
		"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 10000) {
			PRINTDEBUG(">>> Client Timeout !");
			client.stop();
			return "0";
		}
	}
	String line = "";
	// Read all the lines of the reply from server and print them to Serial
	while (client.available()) {
		line += client.readStringUntil('\r');
		PRINTDEBUG(line);
	}

	Serial.println();
	Serial.println("closing connection");
	return line;
}

void gettime()
{
	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(time_host, httpPort)) {
		PRINTDEBUG("connection failed");
		return;
	}

	// We now create a URI for the request
	String url = "/time/";

	PRINTDEBUG("Requesting URL: ");
	PRINTDEBUG(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + time_host + "\r\n" +
		"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			PRINTDEBUG(">>> Client Timeout !");
			client.stop();
			return;
		}
	}

	// Read all the lines of the reply from server and print them to Serial
	while (client.available()) {
		String line = client.readStringUntil('\r');
		if (line.indexOf("time:") >= 0)
		{
			String current_time = line.substring(line.indexOf("time:") + 5);
			PRINTDEBUG(current_time);
		}
	}

	Serial.println();
	Serial.println("closing connection");
}

int testWifi(void) {
	int c = 0;
	PRINTDEBUG("Waiting for Wifi to connect");
	while (c < 20) {
		if (WiFi.status() == WL_CONNECTED) { return(20); }
		delay(500);
		Serial.print(WiFi.status());
		c++;
	}
	PRINTDEBUG("Connect timed out, opening AP");
	return(10);
}

void launchWeb(int webtype) {
	PRINTDEBUG("");
	PRINTDEBUG("WiFi connected");
	PRINTDEBUG(WiFi.localIP());
	PRINTDEBUG(WiFi.softAPIP());
	if (webtype)l = WiFi.softAPIP();
	else l = WiFi.localIP();
	PRINTDEBUG(l);
	if (!mdns.begin("esp8266")) {
		PRINTDEBUG("Error setting up MDNS responder!");
		while (1) {
			delay(1000);
		}
	}
	PRINTDEBUG("mDNS responder started");
	// Start the server
	server.begin();
	PRINTDEBUG("Server started");
	int b = 20;
	int c = 0;
	while (b == 20) {
		b = mdns1(webtype);
	}
}

void setupAP(void) {

	WiFi.mode(WIFI_STA);
	if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
	delay(100);
	int n = WiFi.scanNetworks();
	PRINTDEBUG("scan done");
	if (n == 0)
	{
		PRINTDEBUG("no networks found");
	}
	else
	{
		Serial.print(n);
		PRINTDEBUG(" networks found");
		for (int i = 0; i < n; ++i)
		{
			// Print SSID and RSSI for each network found
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			PRINTDEBUG((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
			delay(10);
		}
	}
	PRINTDEBUG("");
	st = "<ul>";
	for (int i = 0; i < n; ++i)
	{
		// Print SSID and RSSI for each network found
		st += "<li>";
		st += i + 1;
		st += ": ";
		st += WiFi.SSID(i);
		st += " (";
		st += WiFi.RSSI(i);
		st += ")";
		st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
		st += "</li>";
	}
	st += "</ul>";
	delay(100);
	WiFi.mode(WIFI_AP);
	delay(100);
	WiFi.softAP(ssid.c_str(), password.c_str());
	PRINTDEBUG("softap");
	PRINTDEBUG("");
	launchWeb(1);
	PRINTDEBUG("over");
}

void req_byte_inter(String in)
{
	int str_len = in.length() + 1;
	byte input[str_len];
	in.getBytes(input, str_len);
}

void req_inter(String in)
{
	int i = 0;
	// Read each command pair 
	// Length (with one extra character for the null terminator)
	int str_len = in.length() + 1;

	// Prepare the character array (the buffer) 
	char input[str_len];

	// Copy it over 
	in.toCharArray(input, str_len);
	char* command = strtok(input, "&");
	while (command != 0)
	{
		// Split the command in two values
		char* separator = strchr(command, '=');
		if (separator != 0)
		{
			// Actually split the string in 2: replace ':' with 0
			*separator = 0;
			req_name_buf[i] = command;
			++separator;
			req_value_buf[i++] = separator;
		}
		// Find the next command in input string
		command = strtok(0, "&");
	}
}

int mdns1(int webtype)
{
	// Check for any mDNS queries and send responses
	mdns.update();
	WiFiClient client = server.available();
	if (!client) {
		return(20);
	}
	PRINTDEBUG("");
	PRINTDEBUG("New client");
	String req = client.readStringUntil('\r');
	PRINTDEBUG(req);
	// Wait for data from client to become available
	if (client.connected() && !client.available()) {
		PRINTDEBUG("Gone");
		return(20);
	}

	// Read the first line of HTTP request


	// First line of HTTP request looks like "GET /path HTTP/1.1"
	// Retrieve the "/path" part by finding the spaces
	int addr_start = req.indexOf(' ');
	int addr_end = req.indexOf(' ', addr_start + 1);
	if (addr_start == -1 || addr_end == -1) {
		Serial.print("Invalid request: ");
		PRINTDEBUG(req);
		return(20);
	}
	req = req.substring(addr_start + 1, addr_end);
	Serial.print("Request: ");
	PRINTDEBUG(req);
	client.flush();

	String s;

	if (webtype == 1) {
		PRINTDEBUG(req);
		if (req == "/")
		{
			IPAddress ip = WiFi.softAPIP();
			String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
			s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>AMIN";
			s += "<p>";
			s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><label>IP: </label><input name='ip' length=3><input type='submit'></form>";
			s += "</html>\r\n\r\n";
			PRINTDEBUG("Sending 200");
		}
		else if (req.startsWith("/a?ssid=")) {
			String t = "/a?";
			req = req.substring(t.length());
			req_inter(req);
			for (int i = 0; i<3; i++)
			{
				PRINTDEBUG(req_name_buf[i]);
				PRINTDEBUG(req_value_buf[i]);
			}

			PRINTDEBUG("clearing eeprom");
			for (int i = 0; i < 512; ++i) { EEPROM.write(i, 0); }
			String qsid = req_value_buf[0];
			String qpass = req_value_buf[1];
			String qip = req_value_buf[2];

			PRINTDEBUG("writing eeprom ssid:");
			for (int i = 0; i < qsid.length(); ++i)
			{
				EEPROM.write(i, qsid[i]);
				Serial.print("Wrote: ");
				PRINTDEBUG(qsid[i]);
			}
			PRINTDEBUG("writing eeprom pass:");
			for (int i = 0; i < qpass.length(); ++i)
			{
				EEPROM.write(32 + i, qpass[i]);
				Serial.print("Wrote: ");
				PRINTDEBUG(qpass[i]);
			}
			PRINTDEBUG("writing eeprom ip:");
			for (int i = 0; i < qip.length(); ++i)
			{
				EEPROM.write(96 + i, qip[i]);
				Serial.print("Wrote: ");
				PRINTDEBUG(qip[i]);
			}
			EEPROM.commit();
			s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>AMIN<p>";
			s += "</p></html>\r\n\r\n";
		}
		else if (req.startsWith("/cleareeprom")) {
			s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>AMIN";
			s += "<p>Clearing the EEPROM<p>";
			s += "</html>\r\n\r\n";
			PRINTDEBUG("Sending 200");
			PRINTDEBUG("clearing eeprom");
			for (int i = 0; i < 512; ++i) { EEPROM.write(i, 0); }
			EEPROM.commit();
		}
		else if (req.startsWith("/a?update="))
		{
			String t = "/a?";
			req = req.substring(t.length());
			req_inter(req);
			PRINTDEBUG(req_name_buf[0]);
			PRINTDEBUG(req_value_buf[0]);
			t_httpUpdate_return ret = ESPhttpUpdate.update(req_value_buf[0]);

			switch (ret) {
			case HTTP_UPDATE_FAILED:
				Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
				break;

			case HTTP_UPDATE_NO_UPDATES:
				Serial.println("HTTP_UPDATE_NO_UPDATES");
				break;

			case HTTP_UPDATE_OK:
				Serial.println("HTTP_UPDATE_OK");
				break;
			}
		}
		else
		{
			s = "HTTP/1.1 404 Not Found\r\n\r\n";
			PRINTDEBUG("Sending 404");
		}
	}
	else
	{
		if (req == "/")
		{
			s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>AMIN";
			s += "<p>";
			s += "</html>\r\n\r\n";
			PRINTDEBUG("Sending 200");
		}
		else if (req.startsWith("/cleareeprom")) {
			s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>AMIN";
			s += "<p>Clearing the EEPROM<p>";
			s += "</html>\r\n\r\n";
			PRINTDEBUG("Sending 200");
			PRINTDEBUG("clearing eeprom");
			for (int i = 0; i < 512; ++i) { EEPROM.write(i, 0); }
			EEPROM.commit();
		}
		else if (req.startsWith("/time")) {
			// sending an HTML request to Google: will be discussed in the next article
			PRINTDEBUG(send_web_http("/time/", "google.com"));
		}
		else if (req.startsWith("/a?update=")) {
			String t = "/a?";
			req = req.substring(t.length());
			req_inter(req);
			PRINTDEBUG(req_name_buf[0]);
			PRINTDEBUG(req_value_buf[0]);
			t_httpUpdate_return ret = ESPhttpUpdate.update(req_value_buf[0]);

			switch (ret) {
			case HTTP_UPDATE_FAILED:
				Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
				break;

			case HTTP_UPDATE_NO_UPDATES:
				PRINTDEBUG("HTTP_UPDATE_NO_UPDATES");
				break;

			case HTTP_UPDATE_OK:
				PRINTDEBUG("HTTP_UPDATE_OK");
				break;
			}
			s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>AMIN<p>";
			s += String(1);
			s += "</p></html>\r\n\r\n";
		}
		else
		{
			s = "HTTP/1.1 404 Not Found\r\n\r\n";
			PRINTDEBUG("Sending 404");
		}
	}
	client.print(s);
	PRINTDEBUG("Done with client");
	return(20);
}

void loop() {
	// put your main code here, to run repeatedly:

}
