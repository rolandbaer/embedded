/*******************************************************************************************
 * Sample to read state of shelly cover controller Shelly Plus 2PM via WiFi                *
 *                                                                                         *
 *                                                                                         *
 *******************************************************************************************/

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

#include "credentials.h"

String shelly_ip = "192.168.1.132";

const unsigned long MEASURE_INTERVAL = 2 * 1000;

unsigned long last_measure;

bool timer_passed(unsigned long _timer, unsigned long _interval) {
  unsigned long currentMillis = millis();
  return ((unsigned long)(currentMillis - _timer) >= _interval);
}

void connectWiFi() {
  Serial.println("Connecting to ");
  Serial.println(ssid);

  // connecting to local wi-fi network
  WiFi.begin(ssid, password);

  // check wi-fi staus until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("my IP: ");
  Serial.println(WiFi.localIP());
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  Serial.printf("mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void read_and_parse() {
  WiFiClient client;
  HTTPClient http;

  // Send request
  http.useHTTP10(true);
  http.begin(client, "http://" + shelly_ip + "/rpc/Cover.GetStatus?id=0");
  http.GET();

  // Parse response
  DynamicJsonDocument doc(512);
  deserializeJson(doc, http.getStream());

  // Read values
  const char* state = doc["state"];
  Serial.println(state);

  // Disconnect
  http.end();
}

void setup() {
  Serial.begin(9600);
  connectWiFi();
}

void loop() {
  if(timer_passed(last_measure, MEASURE_INTERVAL)) {
    last_measure = millis();
    read_and_parse();
  }
}
