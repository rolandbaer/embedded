/*******************************************************************************************
 * First state machine for window handling                                                 *
 *                                                                                         *
 *                                                                                         *
 *******************************************************************************************/

#include "DHT.h"

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

enum State_enum {READY, OPENING, OPEN_WAIT, OPEN_MEASURE, CLOSING, CLOSED};

// create instance of DHT                          
DHT dht(DHTPIN, DHTTYPE, 6);

State_enum state = READY;
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
  
  // initialize measuring
  pinMode(DHTPIN, INPUT);
  dht.begin();  
  last_measure = millis();
}

void resetStateMachine() {
  button_state_open = digitalRead(PIN_BUTTON_OPEN);
  button_state_close = digitalRead(PIN_BUTTON_CLOSE);

  state = READY;
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

  delay(10);
}

void state_machine_run(int open, int close) {
  switch(state) {
    case READY:
      if(open) {
        tr_opening();
        Serial.println("State: READY -> OPENING");
      }
      if(close) {
        tr_closing();
        Serial.println("State: READY -> CLOSING");
      }
      break;
    case OPENING:
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
    case CLOSING:
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
    case OPEN_WAIT:
      if(close) {
        tr_closing();
        Serial.println("State: OPEN_WAIT -> CLOSING");
      }
      if(timer_passed(timer, interval)) 
      {
        state = OPEN_MEASURE;
        Serial.println("State: OPEN_WAIT -> OPEN_MEASURE");
      }
      break;
    case OPEN_MEASURE:
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
    case CLOSED:        
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
  state = OPENING;
}

void tr_open() {
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  timer = millis();
  interval = KEEP_OPEN_INTERVAL;
  state = OPEN_WAIT;  
}

void tr_closing() {
  timer = millis();
  interval = RELAIS_INTERVAL;
  digitalWrite(PIN_RELAIS_CLOSE, HIGH);
  state = CLOSING;
}

void tr_closed() {
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  state = CLOSED;
}

void tr_stop() {
  digitalWrite(PIN_RELAIS_CLOSE, LOW);
  digitalWrite(PIN_RELAIS_OPEN, LOW);
  state = READY;
}

bool timer_passed(unsigned long _timer, unsigned long _interval) {
  unsigned long currentMillis = millis();
  return ((unsigned long)(currentMillis - _timer) >= _interval);
}

bool dry_or_cold(float _temperature, float _humidity) {
  return _humidity <= HUMIDITY || _temperature <= TEMPERATURE ;
}
