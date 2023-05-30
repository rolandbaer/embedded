# controller for electrified window

This programm handles a window with an electrified opener/closer depending on
the state of the shelly plus 2PM controller and the measured values of a DHT-11
sensor. It provides also a web page to control the window and provide the values
of the sensor and the state of the window.

The window is opened or closed by sending API commands to the shelly plus 2PM.

As soon as the humidity or temperature is under a defined level the window is
closed automatically.

With the web page the window can also be remote controlled and there is also a
manual mode where the window will not be closed automatically.

Copy `credentials.h.sample` to `credentials.h` and change the ssid and
password to your WiFi network credentials.

Board:
- LOLIN(WEMOS) D1 R2 & mini

Required Libraries:
- ESP8266WiFi
- ESP8266 Web Server (ESP8266WebServer)
- ESP8266HTTPClient
- ArduinoJson by Benoit Blanchon
- Adafruit Unified Sensor by Adafruit (Adafruit_Unified_Sensor)
- DHT sensor library by Adafruit (DHT_sensor_library)

To run the program you should it connect like in the schema below. Take care
of the DHT-11 or DHT-12 version you have. Mine had the connectors
VCC / Data / GND, but there are also modules with Data / VCC / GND!

![Connection schema for state_machine_1](D1mini_DHT11_Webserver_Steckplatine.png)
