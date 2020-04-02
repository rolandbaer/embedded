// import used libraries
#include "DHT.h"  

void setup() {
  Serial.begin(9600);

  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);

  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);

}

void loop() {
  int left = digitalRead(D6);
  int right = digitalRead(D7);

  digitalWrite(D1, left);
  digitalWrite(D2, right);
}
