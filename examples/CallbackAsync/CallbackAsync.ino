#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
  #define RX2     RX1
  #define TX2     TX1
#endif

// Pin: Relay  (ESP32)
#define RELAY_PIN 26
// #define RELAY_PIN 32

Mycila::JSY jsy;
Mycila::JSY::Data prevData;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.setCallback([](const Mycila::JSY::EventType eventType, const Mycila::JSY::Data& data) {
    if (prevData != data) {
      Serial.printf(" - %" PRId64 " EVT_CHANGE: %f W\n", esp_timer_get_time(), data.aggregate.activePower);
      prevData = data;
    }
  });

  jsy.begin(Serial2, RX2, TX2);
  if (jsy.getBaudRate() != Mycila::JSY::BaudRate::BAUD_38400) {
    jsy.setBaudRate(Mycila::JSY::BaudRate::BAUD_38400);
  }
  jsy.end();

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, RX2, TX2, true, 0, 4096);

  pinMode(RELAY_PIN, OUTPUT);
}

bool state = LOW;

void loop() {
  state = !state;
  digitalWrite(RELAY_PIN, state);
  delay(5000);
}
