/*
  SSL Web client

 This sketch connects to a website (https://www.example.com)
 using a WiFi ESP8266 module and checks the validity of the server certificate.

 This example is written for a network using WPA encryption.

 Circuit:
   1. On ESP8266 must be running (flashed) WiFiSPIESP application.
    
   2. Connect the master (Arduino or STM32F103) to the following pins on the esp8266:

            ESP8266         |        |
    GPIO    NodeMCU   Name  |   Uno  | STM32F103
  ===============================================
     15       D8       SS   |   D10  |    PA4
     13       D7      MOSI  |   D11  |    PA7
     12       D6      MISO  |   D12  |    PA6
     14       D5      SCK   |   D13  |    PA5

    Note: If the ESP is booting at the moment when the SPI Master (i.e. Arduino) has the Select line HIGH (deselected)
    the ESP8266 WILL FAIL to boot!
    
 original sketch for WiFi library created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe
 
 modified for WiFiSpi library 14 Mar 2017
 by Jiri Bilek
 */


#include <WiFiSpi.h>

// WiFi credentials
char ssid[] = "yourNetwork";        // your network SSID (name)
char pass[] = "secretPassword";     // your network password (use for WPA)

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(93,184,216,34);  // numeric IP for www.example.com (no DNS)
char server[] = "www.example.com";    // name address using DNS

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSpiClient client;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Initialize the WifiSpi library
  WiFiSpi.init();

  // check for the presence of the shield:
  if (WiFiSpi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  if (!WiFiSpi.checkProtocolVersion()) {
    Serial.println("Protocol version mismatch. Please upgrade the firmware");
    // don't continue:
    while (true);
  }

  // attempt to connect to Wifi network:
  int status;

  do {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open network:
    status = WiFiSpi.begin(ssid, pass);
  } while (status != WL_CONNECTED);
  
  Serial.println("Connected to wifi");
  printWifiStatus();

  // Set the server certificate SHA-1 fingerprint
  // Please check the correctness of the fingerprint - this certificate expires on 12-26-2021
  uint8_t fingerprint[] = {0x0A,0x28,0xA6,0xEB,0x17,0x6E,0xA9,0xCC,0x59,0x6F,
                           0x4C,0x73,0xFD,0x89,0x7E,0xFB,0xD3,0x2D,0xCA,0x2A};
  WiFiSpi.setSSLFingerprint(fingerprint);

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (client.connectSSL(server, 443)) {
    Serial.println("connected to server");
    
    // Make a HTTP request:
    // Note: client.print() is transmitting one char per a message that is awfully wasting the SPI bus bandwidth
    //       client.write(char*) is optimized and has minimum overhead
    client.write("GET / HTTP/1.1\r\n"
                 "Host: www.example.com\r\n"
                 "Connection: close\r\n\r\n");
  }
}

void loop() {
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting from server.");
    client.stop();

    // do nothing forevermore:
    while (true);
  }
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFiSpi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFiSpi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFiSpi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
