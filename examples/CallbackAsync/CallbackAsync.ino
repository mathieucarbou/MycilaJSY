#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

// Pin: Relay  (ESP32)
#define RELAY_PIN 26
// #define RELAY_PIN 32

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.setCallback([](const Mycila::JSYEventType eventType) {
    if (eventType == Mycila::JSYEventType::EVT_CHANGE) {
      Serial.printf(" - %" PRIu32 " EVT_CHANGE: %f V, %f A, %f W\n", millis(), jsy.getVoltage2(), jsy.getCurrent2(), jsy.getPower2());
    }
  });

  jsy.begin(Serial2, 16, 17);
  if (jsy.getBaudRate() != Mycila::JSYBaudRate::BAUD_38400) {
    jsy.setBaudRate(Mycila::JSYBaudRate::BAUD_38400);
  }
  jsy.end();

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, 16, 17, true, 0, 4096);

  pinMode(RELAY_PIN, OUTPUT);
}

bool state = LOW;

void loop() {
  state = !state;
  digitalWrite(RELAY_PIN, state);
  delay(5000);
}
