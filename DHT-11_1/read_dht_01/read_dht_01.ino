/*******************************************************************************************
 * Read data from DHT-11 sensor via an arduino nano                                        *
 *                                                                                         *
 * based on a script by Philippe Keller (www.bastelgarage.ch)                              *
 *******************************************************************************************/

// import used library
#include "DHT.h"

// definitions
#define DHTPIN 2          // pin of the arduino where the sensor is connected to
#define DHTTYPE DHT11     // define the type of sensor (DHT11 or DHT22)
       
// create instance of DHT                          
DHT dht(DHTPIN, DHTTYPE, 6);

void setup()
{
  Serial.begin(9600);
  Serial.println("DHT11 test program");
  dht.begin();
}

void loop()
{
  // Wait two seconds between measurements as the sensor will not measure faster
  delay(2000);
                                    
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  // validate values
  if (isnan(h) || isnan(t)) {       
    Serial.println("Error while reading data!");
    return;
  }

  // send data via serial connection to pc
  Serial.print("Luftfeuchtigkeit: ");
  Serial.print(h);
  Serial.print("%\t");
  Serial.print("Temperatur: ");
  Serial.print(t);
  // Serial.write("°");
  Serial.println("°C");
}
