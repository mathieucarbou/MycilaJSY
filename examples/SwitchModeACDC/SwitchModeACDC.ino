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
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  jsy.begin(Serial2, RX2, TX2);

  // switch to fastest available speed or minimum speed
  if (jsy.getBaudRate() != jsy.getMaxAvailableBaudRate()) {
    jsy.setBaudRate(jsy.getMaxAvailableBaudRate());
  } else {
    jsy.setBaudRate(jsy.getMinAvailableBaudRate());
  }

  // switch to AC/DC mode
  switch (jsy.readMode()) {
    case Mycila::JSY::Mode::AC:
      Serial.println("Current mode: AC");
      Serial.println("Switching to DC mode...");
      if (jsy.setMode(Mycila::JSY::Mode::DC)) {
        Serial.println("Switched to DC mode");
      } else {
        Serial.println("Failed to switch to DC mode");
      }
      break;

    case Mycila::JSY::Mode::DC:
      Serial.println("Current mode: DC");
      Serial.println("Switching to AC mode...");
      if (jsy.setMode(Mycila::JSY::Mode::AC)) {
        Serial.println("Switched to AC mode");
      } else {
        Serial.println("Failed to switch to AC mode");
      }
      break;

    default:
      Serial.println("Current mode: Unknown");
      break;
  }

  switch (jsy.readMode()) {
    case Mycila::JSY::Mode::AC:
      Serial.println("Current mode: AC");
      break;

    case Mycila::JSY::Mode::DC:
      Serial.println("Current mode: DC");
      break;

    default:
      Serial.println("Current mode: Unknown");
      break;
  }
}

void loop() {
  uint32_t now = millis();
  if (jsy.read()) {
    Serial.printf("JSY read in %" PRIu32 " ms\n", (uint32_t)(millis() - now));
    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    Serial.printf("Failed to read JSY. Elapsed time: %" PRIu32 " ms\n", (uint32_t)(millis() - now));
  }
  delay(4000);
}
