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

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.begin(Serial2, RX2, TX2);
}

uint32_t counts = 0;

void loop() {
  uint32_t now = millis();
  counts++;

  Serial.printf("JSY baud rate: %d\n", static_cast<int>(jsy.getBaudRate()));

  if (jsy.read()) {
    Serial.print("JSY read in ");
    Serial.print(millis() - now);
    Serial.print(" ms\n");

    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }

  if (counts % 7 == 0) {
    if (jsy.setBaudRate(jsy.getBaudRate() == jsy.getMaxAvailableBaudRate() ? jsy.getMinAvailableBaudRate() : jsy.getMaxAvailableBaudRate())) {
      Serial.printf("JSY baud rate updated to %d\n", static_cast<int>(jsy.getBaudRate()));
    } else {
      Serial.println("JSY baud rate update failed");
    }
  }

  delay(3000);
}
