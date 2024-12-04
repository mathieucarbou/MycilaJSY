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

// Pin: RX (ESP32) -> TX (JSY)
#define JSY_TX_PIN 16

// Pin: TX (ESP32) -> RX (JSY)
#define JSY_RX_PIN 17

// Pin: Relay  (ESP32)
#define RELAY_PIN 26
// #define RELAY_PIN 32

Mycila::JSY jsy;
JsonDocument doc;
const Mycila::JSY::BaudRate rates[] = {
  Mycila::JSY::BaudRate::BAUD_4800,
  Mycila::JSY::BaudRate::BAUD_9600,
  Mycila::JSY::BaudRate::BAUD_19200,
  Mycila::JSY::BaudRate::BAUD_38400,
};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.setCallback([](const Mycila::JSY::EventType eventType) {
    int64_t now = esp_timer_get_time();
    switch (eventType) {
      case Mycila::JSY::EventType::EVT_READ:
        Serial.printf(" - %" PRId64 " EVT_READ\n", now);
        break;
      case Mycila::JSY::EventType::EVT_CHANGE:
        Serial.printf(" - %" PRId64 " EVT_CHANGE: %f W\n", now, jsy.data.aggregate.activePower);
        break;
      default:
        Serial.printf(" - %" PRId64 " ERR\n", now);
        break;
    }
  });

  jsy.begin(Serial2, RX2, TX2);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  delay(2000);

  jsy.read();
  jsy.toJson(doc.to<JsonObject>());
  serializeJsonPretty(doc, Serial);
  Serial.println();

  // speed test for each bauds

  for (size_t i = 0; i < 4; i++) {
    digitalWrite(RELAY_PIN, LOW);

    jsy.setBaudRate(rates[i]);
    jsy.end();
    jsy.begin(Serial2, RX2, TX2);

    Serial.printf("\njsy.read() at %" PRIu32 " bauds:\n", rates[i]);

    digitalWrite(RELAY_PIN, HIGH);
    int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < (int64_t)2000000) {
      jsy.read();
    }
    digitalWrite(RELAY_PIN, LOW);
  }
}

void loop() {
  vTaskDelete(NULL);
}
