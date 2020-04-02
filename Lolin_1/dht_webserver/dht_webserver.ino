/*******************************************************************************************
 * Read data from DHT-11 sensor and present it with a web page                             *
 *                                                                                         *
 * Based on a Script by Bernhard Linz ( https://znil.net/ )                                *
 *******************************************************************************************/

// import used libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"

// include WiFi credentials
#include "credentials.h"

// definitions
#define DHTPIN D5         // pin of the arduino where the sensor is connected to
#define DHTTYPE DHT11     // define the type of sensor (DHT11 or DHT22)

// create web server
ESP8266WebServer webServer(80);

// create instance of DHT                          
DHT dht(DHTPIN, DHTTYPE, 6);

// variables for measured values
float temperature;
float humidity;

// initialization
void setup() {
  Serial.begin(9600);
  Serial.println("DHT11 test web server");

  // initialize measuring
  pinMode(DHTPIN, INPUT);
  dht.begin();

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

  webServer.on("/", handle_OnConnect);
  webServer.onNotFound(handle_NotFound);

  webServer.begin();
  Serial.println("HTTP server started");
}

void loop() {
  webServer.handleClient();
}

void handle_OnConnect() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  webServer.send(200, "text/html", buildHtml(temperature,humidity)); 
}

void handle_NotFound(){
  webServer.send(404, "text/plain", "Not found");
}

String buildHtml(float _temperature,float _humidity){
  String page = "<!DOCTYPE html> <html>\n";
  page +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  page +="<meta charset=\"UTF-8\">";
  page +="<meta http-equiv=\"refresh\" content=\"5\" >";
  page +="<title>WeMos D1 mini Temperature & Humidity Report</title>\n";
  page +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  page +="body{margin-top: 50px;} h1 {color: #444444;margin: 20px auto 30px;}\n";
  page +="h2 {color: #0d4c75;margin: 50px auto 20px;}\n";
  page +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  page +="</style>\n";
  page +="</head>\n";
  page +="<body>\n";
  page +="<div id=\"webpage\">\n";
  page +="<h2>WeMos Lolin D1 mini</h2><h1>Temperature & Humidity Report</h1>\n";
  
  page +="<p>Temperature: ";
  page +=(int)_temperature;
  page +=" °C</p>";
  page +="<p>Humidity: ";
  page +=(int)_humidity;
  page +=" %</p>";
  
  page +="</div>\n";
  page +="</body>\n";
  page +="</html>\n";
  return page;
}
