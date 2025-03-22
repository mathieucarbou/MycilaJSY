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

static Mycila::JSY jsy;
static Mycila::JSY::Data jsyData; // might need to be volatile, depending on the use case

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.setCallback([](const Mycila::JSY::EventType& event, const Mycila::JSY::Data& data) {
    if (event == Mycila::JSY::EventType::EVT_READ) {
      jsyData = data;
    }
  });

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, RX2, TX2, true);
}

uint32_t lastTime = 0;

void loop() {
  if (millis() - lastTime > 3000) {
    JsonDocument doc;
    jsyData.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
    lastTime = millis();
  }

  delay(1000);
}
