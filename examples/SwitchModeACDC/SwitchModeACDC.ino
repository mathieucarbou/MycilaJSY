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

  Mycila::JSY::BaudRate wantedBaudRate = jsy.getMaxAvailableBaudRate();
  // Mycila::JSY::BaudRate wantedBaudRate = Mycila::JSY::BaudRate::BAUD_9600;

  Mycila::JSY::Mode wantedMode = Mycila::JSY::Mode::AC;
  // Mycila::JSY::Mode wantedMode = Mycila::JSY::Mode::DC;

  // switch to fastest available speed or minimum speed
  if (jsy.getBaudRate() != wantedBaudRate) {
    Serial.printf("Switching to baud rate %" PRIu32 "...\n", wantedBaudRate);
    jsy.setBaudRate(wantedBaudRate);
  }

  // switch to AC/DC mode
  if (jsy.readMode() != wantedMode) {
    Serial.printf("Switching to mode %s...\n", wantedMode == Mycila::JSY::Mode::AC ? "AC" : "DC");
    jsy.setMode(wantedMode);
  }

  // verify mode change
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
