#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
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

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  // baud rate will be detected automatically
  jsy.begin(Serial2, 16, 17);
  // jsy.begin(Serial2, 16, 17, Mycila::JSY::BaudRate::UNKNOWN);

  // if you know the bauds rate, you can set it manually
  // jsy.begin(Serial2, 16, 17, Mycila::JSY::BaudRate::BAUD_4800);
  // jsy.begin(Serial2, 16, 17, Mycila::JSY::BaudRate::BAUD_9600);
  // jsy.begin(Serial2, 16, 17, Mycila::JSY::BaudRate::BAUD_19200);
  // jsy.begin(Serial2, 16, 17, Mycila::JSY::BaudRate::BAUD_38400);
}

void loop() {
  uint32_t now = millis();
  if (jsy.read()) {
    Serial.printf("JSY read in %" PRIu32 " ms\n", (uint32_t)(millis() - now));
    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  } else {
    Serial.printf("Failed to read JSY. Elapsed time: %" PRIu32 " ms\n", (uint32_t)(millis() - now));
  }
  delay(1000);
}
