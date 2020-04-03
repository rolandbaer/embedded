/*******************************************************************************************
 * Controller for electric window with state machine and web server                        *
 *                                                                                         *
 *                                                                                         *
 *******************************************************************************************/

// import used libraries
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// include WiFi credentials
#include "credentials.h"

// definitions
#define PIN_BUTTON_OPEN D6
#define PIN_BUTTON_CLOSE D7
#define PIN_RELAIS_OPEN D1
#define PIN_RELAIS_CLOSE D2

#define DHTPIN D5         // pin where the sensor is connected to
#define DHTTYPE DHT11     // define the type of sensor (DHT11 or DHT22)

#define RELAIS_INTERVAL 10 * 1000
#define KEEP_OPEN_INTERVAL 10 * 1000
#define HUMIDITY 45
#define TEMPERATURE 20
#define MEASURE_INTERVAL 2 * 1000

enum State_enum {STATE_READY, STATE_OPENING, STATE_OPEN_WAIT, STATE_OPEN_MEASURE, STATE_CLOSING, STATE_CLOSED};

String printState(State_enum _state) {
  switch(_state)
  {
    case STATE_READY:
      return "Ready";
    case STATE_OPENING:
      return "Opening";
    case STATE_OPEN_WAIT:
      return "Open (wait)";
    case STATE_OPEN_MEASURE:
      return "Open (measure)";
    case STATE_CLOSING:
      return "Closing";
    case STATE_CLOSED:
      return "Closed";
  }
}

// create instance of DHT                          
DHT dht(DHTPIN, DHTTYPE, 6);

// create web server
ESP8266WebServer webServer(80);

State_enum state = STATE_READY;
unsigned long timer, interval, last_measure;
int button_state_open = 0;
int button_state_close = 0;

// variables for measured values
float temperature = NAN;
float humidity = NAN;

void setup() {
  Serial.begin(9600);

  pinMode(PIN_BUTTON_OPEN, INPUT);
  pinMode(PIN_BUTTON_CLOSE, INPUT);

  pinMode(PIN_RELAIS_OPEN, OUTPUT);
  pinMode(PIN_RELAIS_CLOSE, OUTPUT);

  resetStateMachine();

  initWebServer();
  
  // initialize measuring
  pinMode(DHTPIN, INPUT);
  dht.begin();  
  last_measure = millis();
}

void resetStateMachine() {
  button_state_open = digitalRead(PIN_BUTTON_OPEN);
  button_state_close = digitalRead(PIN_BUTTON_CLOSE);

  state = STATE_READY;
}

void initWebServer() {
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
  int current_state_open = digitalRead(PIN_BUTTON_OPEN);
  int current_state_close = digitalRead(PIN_BUTTON_CLOSE);

  if(timer_passed(last_measure, MEASURE_INTERVAL)) {
    last_measure = millis();
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    Serial.printf("Measure: Temp %d°C Hum %d%%", (int)temperature, (int)humidity);
    Serial.println("");
  }
  
  state_machine_run((current_state_open != button_state_open) && current_state_open,
                    (current_state_close != button_state_close) && current_state_close);
  button_state_open = current_state_open;
  button_state_close = current_state_close;

  webServer.handleClient();

  delay(10);
}

void state_machine_run(int open, int close) {
  switch(state) {
    case STATE_READY:
      if(open) {
        tr_opening();
        Serial.println("State: READY -> OPENING");
      }
      if(close) {
        tr_closing();
        Serial.println("State: READY -> CLOSING");
      }
      break;
    case STATE_OPENING:
      if(close) {
        tr_stop();
        Serial.println("State: OPENING -> READY");
      }
      if(timer_passed(timer, interval))
      {
        tr_open();
        Serial.println("State: OPENING -> OPEN_WAIT");
      }
      break;
    case STATE_CLOSING:
      if(open) {
        tr_stop();
        Serial.println("State: CLOSING -> READY");
      }
      if(timer_passed(timer, interval)) 
      {
        tr_closed();
        Serial.println("State: CLOSING -> CLOSED");
      }
      break;
    case STATE_OPEN_WAIT:
      if(close) {
        tr_closing();
        Serial.println("State: OPEN_WAIT -> CLOSING");
      }
      if(timer_passed(timer, interval)) 
      {
        state = STATE_OPEN_MEASURE;
        Serial.println("State: OPEN_WAIT -> OPEN_MEASURE");
      }
      break;
    case STATE_OPEN_MEASURE:
      if(close) {
        tr_closing();
        Serial.println("State: OPEN_MEASURE -> CLOSING");
      }
      if(dry_or_cold(temperature, humidity)) 
      {
        tr_closing();
        Serial.println("State: OPEN_MEASURE -> CLOSING");
      }
      break;
    case STATE_CLOSED:        
      if(open) {
        tr_opening();
        Serial.println("State: CLOSED -> OPENING");
      }
      break;
  }
}

void tr_opening() {
  timer = millis();
  interval = RELAIS_INTERVAL;
  digitalWrite(PIN_RELAIS_OPEN, HIGH);
  state = STATE_OPENING;
}

void tr_open() {
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  timer = millis();
  interval = KEEP_OPEN_INTERVAL;
  state = STATE_OPEN_WAIT;  
}

void tr_closing() {
  timer = millis();
  interval = RELAIS_INTERVAL;
  digitalWrite(PIN_RELAIS_CLOSE, HIGH);
  state = STATE_CLOSING;
}

void tr_closed() {
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  state = STATE_CLOSED;
}

void tr_stop() {
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  state = STATE_READY;
}

bool timer_passed(unsigned long _timer, unsigned long _interval) {
  unsigned long currentMillis = millis();
  return ((unsigned long)(currentMillis - _timer) >= _interval);
}

bool dry_or_cold(float _temperature, float _humidity) {
  return _humidity <= HUMIDITY || _temperature <= TEMPERATURE ;
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
  
  page +="<p>Window State: ";
  page +=printState(state);
  page +="</p>"; 
   
  page +="</div>\n";
  page +="</body>\n";
  page +="</html>\n";
  return page;
}
