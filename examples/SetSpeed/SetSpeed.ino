#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
#endif

Mycila::JSY::BaudRate target = Mycila::JSY::BaudRate::BAUD_38400;

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)// read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  jsy.begin(Serial2, 16, 17);

  if (jsy.isEnabled()) {
    if (jsy.setBaudRate(target)) {
      Serial.println("JSY baud rate updated");
    } else {
      Serial.println("JSY baud rate update failed");
    }
  } else {
    Serial.println("JSY is disabled");
  }
}

void loop() {
  uint32_t now = millis();
  if (jsy.read()) {
    Serial.print("JSY read in ");
    Serial.print(millis() - now);
    Serial.print(" ms\n");
    
    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }
  delay(1000);
}
