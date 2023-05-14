/*******************************************************************************************
 * Controller for electric window via a shelly plus 2 PM with state machine and web server *
 *                                                                                         *
 *                                                                                         *
 *******************************************************************************************/

// import used libraries
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

// include WiFi credentials
#include "credentials.h"

// definitions
#define SHELLY_IP "192.168.1.108:8080"
#define COVER_ID "0"
const unsigned long KEEP_OPEN_INTERVAL = 1 * 60 * 1000;
const float HUMIDITY = 55;
const float TEMPERATURE = 20;
const unsigned long MEASURE_INTERVAL = 5 * 1000;
const unsigned long CLOSING_INTERVAL = 90 * 1000;

const uint8_t PIN_MANUAL_MODE = BUILTIN_LED;

const uint8_t DHTPIN = D5;      // pin where the sensor is connected to
const uint8_t DHTTYPE = DHT11;  // define the type of sensor (DHT11 or DHT22)

enum class Mode { Auto,
                  Manual };
enum class State { Ready,
                   OpenWait,
                   OpenMeasure,
                   Closing,
                   Closed };
enum class Shelly { Open,
                    Closed,
                    Neither,
                    NoConnection };

String printState(State _state) {
  switch (_state) {
    case State::Ready:
      return "Ready";
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
Shelly shellyState = Shelly::Neither;
unsigned long timer, interval, last_measure;

bool event_open = false;
bool event_close = false;
bool event_manual = false;
bool event_auto = false;

// variables for measured values
float temperature = NAN;
float humidity = NAN;

void setup() {
  Serial.begin(9600);

  pinMode(PIN_MANUAL_MODE, OUTPUT);

  resetStateMachine();
  resetOutput();

  connectWiFi();
  initWebServer();

  // initialize measuring
  pinMode(DHTPIN, INPUT);
  dht.begin();
  last_measure = millis();
}

void resetStateMachine() {
  state = State::Ready;
}

void resetOutput() {
  digitalWrite(PIN_MANUAL_MODE, mode != Mode::Manual);  // the builtin LED is on when voltage is LOW (false)
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

void initWebServer() {
  webServer.on("/", handle_IndexHtml);
  webServer.on("/data", handle_Data);
  webServer.on("/manual", handle_Manual);
  webServer.on("/auto", handle_Auto);
  webServer.onNotFound(handle_NotFound);

  webServer.begin();
  Serial.println("HTTP server started");
}

void loop() {
  if (timer_passed(last_measure, MEASURE_INTERVAL)) {
    last_measure = millis();
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    Serial.printf("Measure: Temp %d°C Hum %d%%", (int)temperature, (int)humidity);
    Serial.println("");
    shellyState = read_and_parse();
  }

  if (mode == Mode::Auto) {
    if (event_manual) {
      event_manual = false;
      event_auto = false;
      mode = Mode::Manual;
      resetStateMachine();
      resetOutput();
    } else {
      state_machine_run(shellyState);
    }
  } else {
    if (event_auto) {
      event_manual = false;
      event_auto = false;
      mode = Mode::Auto;
      resetStateMachine();
      resetOutput();
    }
  }

  webServer.handleClient();

  delay(10);
}

void state_machine_run(Shelly shellyState) {
  switch (state) {
    case State::Ready:
      if (shellyState == Shelly::Open) {
        tr_open();
        Serial.println("State: Ready -> OpenWait");
      }
      if (shellyState == Shelly::Closed) {
        tr_closed();
        Serial.println("State: Ready -> Closed");
      }
      break;
    case State::OpenWait:
      if (shellyState == Shelly::Closed) {
        tr_closed();
        Serial.println("State: OpenWait -> Closed");
      }
      if (timer_passed(timer, interval)) {
        state = State::OpenMeasure;
        Serial.println("State: OpenWait -> OpenMeasure");
      }
      break;
    case State::OpenMeasure:
      if (shellyState == Shelly::Closed) {
        tr_closed();
        Serial.println("State: OpenMeasure -> Closed");
      }
      if (dry_or_cold(temperature, humidity)) {
        tr_closing();
        Serial.println("State: OpenMeasure -> Closing");
      }
      break;
    case State::Closing:
      if (shellyState == Shelly::Closed) {
        tr_closed();
        Serial.println("State: Closing -> Closed");
      }
      break;
    case State::Closed:
      if (shellyState == Shelly::Open) {
        tr_open();
        Serial.println("State: Closed -> OpenWait");
      }
      break;
  }
}

Shelly read_and_parse() {
  WiFiClient client;
  HTTPClient http;

  // Send request
  http.useHTTP10(true);
  http.begin(client, "http://" SHELLY_IP "/rpc/Cover.GetStatus?id=" COVER_ID);
  int httpStatusCode = http.GET();
  if(200 == httpStatusCode)
  {
    // Parse response
    DynamicJsonDocument doc(512);
    deserializeJson(doc, http.getStream());

    // Disconnect
    http.end();

    // Read values
    const char* state = doc["state"];
    Serial.print("Shelly state: ");
    Serial.println(state);
    if (strcmp("open", state) == 0)
      return Shelly::Open;

    if (strcmp("closed", state) == 0)
      return Shelly::Closed;
  }
  else
  {
    return Shelly::NoConnection;
  }
  return Shelly::Neither;
}

void send_close() {
  WiFiClient client;
  HTTPClient http;

  // Send request
  http.useHTTP10(true);
  http.begin(client, "http://" SHELLY_IP "/rpc/Cover.Close?id=" COVER_ID);
  http.GET();

  // Disconnect
  http.end();
}

void tr_open() {
  timer = millis();
  interval = KEEP_OPEN_INTERVAL;
  state = State::OpenWait;
}

void tr_closing() {
  timer = millis();
  interval = CLOSING_INTERVAL;
  send_close();
  state = State::Closing;
}

void tr_closed() {
  state = State::Closed;
}

bool timer_passed(unsigned long _timer, unsigned long _interval) {
  unsigned long currentMillis = millis();
  return ((unsigned long)(currentMillis - _timer) >= _interval);
}

bool dry_or_cold(float _temperature, float _humidity) {
  return _humidity <= HUMIDITY || _temperature <= TEMPERATURE;
}

void handle_IndexHtml() {
  webServer.send(200, "text/html", buildHtml(temperature, humidity));
}

void handle_Data() {
  String data = "" + String((int)temperature) + "/" + String((int)humidity) + "/" + printState(state)  + "/" + (shellyState == Shelly::NoConnection ? "No Connection" : "");
  webServer.send(200, "text/html", data);
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

void handle_NotFound() {
  webServer.send(404, "text/plain", "Not found");
}

String buildHtml(float _temperature, float _humidity) {
  String page = "<!DOCTYPE html> <html>\n";
  page += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  page += "<meta charset=\"UTF-8\">";
  page += "<title>WeMos D1 mini Temperature & Humidity Report</title>\n";
  page += "<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">";
  page += "<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; width: 32em;}\n";
  page += "body {margin-top: 50px;} h1 {color: #444444; margin: 20px auto 30px;}\n";
  page += "h2 {color: #0d4c75; margin: 50px auto 20px; font-size: 3.0rem;}\n";
  page += "p {font-size: 2.2rem; color: #444444; margin-bottom: 10px;} i {width: 1.1em;}\n";
  page += "button {font-size: 2.0rem; color: #444444; width: 7em; padding: 8px;}\n";
  page += ".dht-labels {font-size: 1.5rem; vertical-align:middle; padding-bottom: 15px; width: 7em; display:inline-block;}\n";
  page += ".units { font-size: 1.5rem; }\n";
  page += "</style>\n";
  page += "</head>\n";
  page += "<body>\n";
  page += "<div id=\"webpage\">\n";
  page += "<h2>WeMos Lolin D1 mini</h2><h1>Temperature & Humidity Report</h1>\n";

  page += "<p><i class=\"fas fa-thermometer-half\" style=\"color:#059e8a;\"></i>\n";
  page += "<span class=\"dht-labels\">Temperature</span>\n";
  page += "<span id=\"temperature\">";
  page += isnan(_temperature) ? 0 : (int)_temperature;
  page += "</span>\n<sup class=\"units\">&deg;C</sup></p>\n";

  page += "<p><i class=\"fas fa-tint\" style=\"color:#00add6;\"></i>\n";
  page += "<span class=\"dht-labels\">Humidity</span>\n";
  page += "<span id=\"humidity\">";
  page += isnan(_humidity) ? 0 : (int)_humidity;
  page += "</span>\n<sup class=\"units\">%</sup></p>\n";

  page += "<p><i class=\"fas fa-eye\" style=\"color:#add600;\"></i>\n";
  page += "<span class=\"dht-labels\">Window State</span>\n";
  page += "<span id=\"state\">";
  page += printState(state);
  page += "</span></p>\n";

  page += "<p id=\"error_block\" style=\"visibility: hidden;\"><i class=\"fas fa-exclamation-circle\" style=\"color:#d60000;\"></i>\n";
  page += "<span class=\"dht-labels\">Error:</span>\n";
  page += "<span id=\"error\">";
    page += "</span></p>\n";

  page += "<p><button onClick=\"sendCommand('manual');\"><i class=\"far fa-hand-paper\" style=\"color:#123456;\"></i>\n";
  page += "Manual</button>\n";
  page += "<button onClick=\"sendCommand('auto');\"><i class=\"fas fa-robot\" style=\"color:#123456;\"></i>\n";
  page += "Auto</button></p>\n";

  page += "</div>\n";
  page += "</body>\n";
  page += "<script>\n";
  page += "setInterval(function ( ) {";
  page += "var xhttp = new XMLHttpRequest();";
  page += "xhttp.onreadystatechange = function() {";
  page += "  if (this.readyState == 4 && this.status == 200) {";
  page += "    var res = this.responseText.split(\"/\");";
  page += "    document.getElementById(\"temperature\").innerHTML = res[0];";
  page += "    document.getElementById(\"humidity\").innerHTML = res[1];";
  page += "    document.getElementById(\"state\").innerHTML = res[2];";
  page += "    document.getElementById(\"error\").innerHTML = res[3];";
  page += "    document.getElementById(\"error_block\").style = res[3] ? \"visibility: visible;\" : \"visibility: hidden;\";";
  page += "  }";
  page += "};";
  page += "xhttp.open(\"GET\", \"/data\", true);";
  page += "xhttp.send();";
  page += "}, 2000 ) ;";
  page += "function sendCommand(command) {";
  page += "  var xhttp = new XMLHttpRequest();";
  page += "  xhttp.open(\"GET\", \"/\" + command, true);";
  page += "  xhttp.send();";
  page += "}";
  page += "</script>";
  page += "</html>\n";
  return page;
}
