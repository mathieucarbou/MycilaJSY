#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
#endif

// Pin: Relay  (ESP32)
#define RELAY_PIN 26
// #define RELAY_PIN 32

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.setCallback([](const Mycila::JSY::EventType eventType) {
    if (eventType == Mycila::JSY::EventType::EVT_CHANGE) {
      Serial.printf(" - %" PRIu32 " EVT_CHANGE: %f V, %f A, %f W\n", (uint32_t)millis(), jsy.data.channel2().voltage, jsy.data.channel2().current, jsy.data.channel2().activePower);
    }
  });

  jsy.begin(Serial2, 16, 17);
  if (jsy.getBaudRate() != Mycila::JSY::BaudRate::BAUD_38400) {
    jsy.setBaudRate(Mycila::JSY::BaudRate::BAUD_38400);
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
