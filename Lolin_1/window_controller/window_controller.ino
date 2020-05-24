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
const uint8_t PIN_BUTTON_OPEN = D6;
const uint8_t PIN_BUTTON_CLOSE = D7;
const uint8_t PIN_RELAIS_OPEN = D1;
const uint8_t PIN_RELAIS_CLOSE = D2;

const uint8_t PIN_MANUAL_MODE = BUILTIN_LED;

const uint8_t DHTPIN = D5;         // pin where the sensor is connected to
const uint8_t DHTTYPE = DHT11;     // define the type of sensor (DHT11 or DHT22)

const unsigned long RELAIS_INTERVAL = 10 * 1000;
const unsigned long KEEP_OPEN_INTERVAL = 10 * 1000;
const float HUMIDITY = 45;
const float TEMPERATURE = 20;
const unsigned long MEASURE_INTERVAL = 2 * 1000;

enum class Mode {Auto, Manual};
enum class State {Ready, Opening, OpenWait, OpenMeasure, Closing, Closed};

String printState(State _state) {
  switch(_state)
  {
    case State::Ready:
      return "Ready";
    case State::Opening:
      return "Opening";
    case State::OpenWait:
      return "Open (wait)";
    case State::OpenMeasure:
      return "Open (measure)";
    case State::Closing:
      return "Closing";
    case State::Closed:
      return "Closed";
  }
}

// create instance of DHT                          
DHT dht(DHTPIN, DHTTYPE, 6);

// create web server
ESP8266WebServer webServer(80);

Mode mode = Mode::Auto;
State state = State::Ready;
unsigned long timer, interval, last_measure;
int button_state_open = 0;
int button_state_close = 0;

bool event_open = false;
bool event_close = false;
bool event_manual = false;
bool event_auto = false;

// variables for measured values
float temperature = NAN;
float humidity = NAN;

void setup() {
  Serial.begin(9600);

  pinMode(PIN_BUTTON_OPEN, INPUT);
  pinMode(PIN_BUTTON_CLOSE, INPUT);

  pinMode(PIN_RELAIS_OPEN, OUTPUT);
  pinMode(PIN_RELAIS_CLOSE, OUTPUT);
  pinMode(PIN_MANUAL_MODE, OUTPUT);

  resetStateMachine();
  resetRelais();
  
  initWebServer();
  
  // initialize measuring
  pinMode(DHTPIN, INPUT);
  dht.begin();  
  last_measure = millis();
}

void resetStateMachine() {
  button_state_open = digitalRead(PIN_BUTTON_OPEN);
  button_state_close = digitalRead(PIN_BUTTON_CLOSE);

  state = State::Ready;
}

void resetRelais() {
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  digitalWrite(PIN_MANUAL_MODE, mode != Mode::Manual);  // the builtin LED is on when voltage is LOW (false)
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

  webServer.on("/", handle_IndexHtml);
  webServer.on("/data", handle_Data);
  webServer.on("/open", handle_Open);
  webServer.on("/close", handle_Close);
  webServer.on("/manual", handle_Manual);
  webServer.on("/auto", handle_Auto);
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

  if(mode == Mode::Auto) {
    if(event_manual) {
      event_manual = false;
      event_auto = false;
      mode = Mode::Manual;
      resetStateMachine();
      resetRelais();
    }
    else {
      state_machine_run(process_event_open() || ((current_state_open != button_state_open) && current_state_open),
                        process_event_close() || ((current_state_close != button_state_close) && current_state_close));
      button_state_open = current_state_open;
      button_state_close = current_state_close;
    }
  }
  else {
    if(event_auto) {
      event_manual = false;
      event_auto = false;
      mode = Mode::Auto;
      resetStateMachine();
      resetRelais();
    }
    else {
      digitalWrite(PIN_RELAIS_OPEN, digitalRead(PIN_BUTTON_OPEN));
      digitalWrite(PIN_RELAIS_CLOSE, digitalRead(PIN_BUTTON_CLOSE));
    }
  }
  
  webServer.handleClient();

  delay(10);
}

void state_machine_run(int open, int close) {
  switch(state) {
    case State::Ready:
      if(open) {
        tr_opening();
        Serial.println("State: Ready -> Opening");
      }
      if(close) {
        tr_closing();
        Serial.println("State: Ready -> Closing");
      }
      break;
    case State::Opening:
      if(close) {
        tr_stop();
        Serial.println("State: Opening -> Ready");
      }
      if(timer_passed(timer, interval))
      {
        tr_open();
        Serial.println("State: Opening -> OpenWait");
      }
      break;
    case State::Closing:
      if(open) {
        tr_stop();
        Serial.println("State: Closing -> Ready");
      }
      if(timer_passed(timer, interval)) 
      {
        tr_closed();
        Serial.println("State: Closing -> Closed");
      }
      break;
    case State::OpenWait:
      if(close) {
        tr_closing();
        Serial.println("State: OpenWait -> Closing");
      }
      if(timer_passed(timer, interval)) 
      {
        state = State::OpenMeasure;
        Serial.println("State: OpenWait -> OpenMeasure");
      }
      break;
    case State::OpenMeasure:
      if(close) {
        tr_closing();
        Serial.println("State: OpenMeasure -> Closing");
      }
      if(dry_or_cold(temperature, humidity))
      {
        tr_closing();
        Serial.println("State: OpenMeasure -> Closing");
      }
      break;
    case State::Closed:
      if(open) {
        tr_opening();
        Serial.println("State: Closed -> Opening");
      }
      break;
  }
}

void tr_opening() {
  timer = millis();
  interval = RELAIS_INTERVAL;
  digitalWrite(PIN_RELAIS_OPEN, HIGH);
  state = State::Opening;
}

void tr_open() {
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  timer = millis();
  interval = KEEP_OPEN_INTERVAL;
  state = State::OpenWait;
}

void tr_closing() {
  timer = millis();
  interval = RELAIS_INTERVAL;
  digitalWrite(PIN_RELAIS_CLOSE, HIGH);
  state = State::Closing;
}

void tr_closed() {
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  state = State::Closed;
}

void tr_stop() {
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  state = State::Ready;
}

bool timer_passed(unsigned long _timer, unsigned long _interval) {
  unsigned long currentMillis = millis();
  return ((unsigned long)(currentMillis - _timer) >= _interval);
}

bool dry_or_cold(float _temperature, float _humidity) {
  return _humidity <= HUMIDITY || _temperature <= TEMPERATURE ;
}

void handle_IndexHtml() {
  webServer.send(200, "text/html",  buildHtml(temperature,humidity));
}

void handle_Data() {
  String data = "" + String((int)temperature) + "/" + String((int)humidity) + "/" + printState(state);
  webServer.send(200, "text/html", data); 
}

bool process_event_open() {
  if(event_open) {
    event_open = false;
    return true;
  }

  return false;
}
  
bool process_event_close() {
  if(event_close) {
    event_close = false;
    return true;
  }

  return false;
}
  
void handle_Open() {
  Serial.println("*** Open ***");
  event_open = true;
  webServer.send(200, "text/html", "");
}

void handle_Close() {
  Serial.println("*** Close ***");
  event_close = true;
  webServer.send(200, "text/html", "");
}

void handle_Auto() {
  Serial.println("*** Auto ***");
  event_auto = true;
  webServer.send(200, "text/html", "");
}

void handle_Manual() {
  Serial.println("*** Manual ***");
  event_manual = true;
  webServer.send(200, "text/html", "");
}

void handle_NotFound(){
  webServer.send(404, "text/plain", "Not found");
}

String buildHtml(float _temperature,float _humidity){
  String page = "<!DOCTYPE html> <html>\n";
  page +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  page +="<meta charset=\"UTF-8\">";
  page +="<title>WeMos D1 mini Temperature & Humidity Report</title>\n";
  page +="<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">";
  page +="<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; width: 32em;}\n";
  page +="body {margin-top: 50px;} h1 {color: #444444; margin: 20px auto 30px;}\n";
  page +="h2 {color: #0d4c75; margin: 50px auto 20px; font-size: 3.0rem;}\n";
  page +="p {font-size: 3.0rem; color: #444444; margin-bottom: 10px;} i {width: 1.1em;}\n";
  page +="button {font-size: 2.0rem; color: #444444; width: 7em; padding: 8px;}\n";
  page +=".dht-labels {font-size: 1.5rem; vertical-align:middle; padding-bottom: 15px; width: 8em; display:inline-block;}\n";
  page +=".units { font-size: 1.5rem; }\n";
  page +="</style>\n";
  page +="</head>\n";
  page +="<body>\n";
  page +="<div id=\"webpage\">\n";
  page +="<h2>WeMos Lolin D1 mini</h2><h1>Temperature & Humidity Report</h1>\n";
  
  page +="<p><i class=\"fas fa-thermometer-half\" style=\"color:#059e8a;\"></i>\n"; 
  page +="<span class=\"dht-labels\">Temperature</span>\n"; 
  page +="<span id=\"temperature\">";
  page +=isnan(_temperature)? 0 : (int)_temperature;
  page +="</span>\n<sup class=\"units\">&deg;C</sup></p>\n";

  page +="<p><i class=\"fas fa-tint\" style=\"color:#00add6;\"></i>\n"; 
  page +="<span class=\"dht-labels\">Humidity</span>\n";
  page +="<span id=\"humidity\">";
  page +=isnan(_humidity)? 0 : (int)_humidity;
  page +="</span>\n<sup class=\"units\">%</sup></p>\n";

  page +="<p><i class=\"fas fa-eye\" style=\"color:#add600;\"></i>\n"; 
  page +="<span class=\"dht-labels\">Window State</span>\n";
  page +="<span id=\"state\">";
  page +=printState(state);
  page +="</span></p>\n";
  
  page +="<p><button onClick=\"sendCommand('open');\"><i class=\"fas fa-share\" style=\"color:#123456;\"></i>\n"; 
  page +="Open</button>\n";
  page +="<button onClick=\"sendCommand('close');\"><i class=\"fas fa-reply\" style=\"color:#123456;\"></i>\n"; 
  page +="Close</button></p>\n";
  page +="<p><button onClick=\"sendCommand('manual');\"><i class=\"far fa-hand-paper\" style=\"color:#123456;\"></i>\n"; 
  page +="Manual</button>\n";
  page +="<button onClick=\"sendCommand('auto');\"><i class=\"fas fa-robot\" style=\"color:#123456;\"></i>\n"; 
  page +="Auto</button></p>\n";
  
  page +="</div>\n";
  page +="</body>\n";
  page +="<script>\n";
  page +="setInterval(function ( ) {";
  page +="var xhttp = new XMLHttpRequest();";
  page +="xhttp.onreadystatechange = function() {";
  page +="  if (this.readyState == 4 && this.status == 200) {";
  page +="    var res = this.responseText.split(\"/\");";
  page +="    document.getElementById(\"temperature\").innerHTML = res[0];";
  page +="    document.getElementById(\"humidity\").innerHTML = res[1];";
  page +="    document.getElementById(\"state\").innerHTML = res[2];";
  page +="  }";
  page +="};";
  page +="xhttp.open(\"GET\", \"/data\", true);";
  page +="xhttp.send();";
  page +="}, 2000 ) ;";
  page +="function sendCommand(command) {";
  page +="  var xhttp = new XMLHttpRequest();";
  page +="  xhttp.open(\"GET\", \"/\" + command, true);";
  page +="  xhttp.send();";
  page +="}";
  page +="</script>";
  page +="</html>\n";
  return page;
}
