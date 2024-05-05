#include <ArduinoJson.h>
#include <MycilaJSY.h>

// Pin: RX (ESP32) -> TX (JSY)
#define JSY_TX_PIN 16

// Pin: TX (ESP32) -> RX (JSY)
#define JSY_RX_PIN 17

// Pin: Relay  (ESP32)
#define RELAY_PIN 26
// #define RELAY_PIN 32

Mycila::JSY jsy;
JsonDocument doc;
const Mycila::JSYBaudRate rates[] = {
  Mycila::JSYBaudRate::BAUD_4800,
  Mycila::JSYBaudRate::BAUD_9600,
  Mycila::JSYBaudRate::BAUD_19200,
  Mycila::JSYBaudRate::BAUD_38400,
};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.setCallback([](const Mycila::JSYEventType eventType) {
    int64_t now = esp_timer_get_time();
    switch (eventType) {
      case Mycila::JSYEventType::EVT_READ:
        Serial.printf(" - %" PRId64 " EVT_READ\n", now);
        break;
      case Mycila::JSYEventType::EVT_CHANGE:
        Serial.printf(" - %" PRId64 " EVT_CHANGE: %f W\n", now, jsy.getPower2());
        break;
      default:
        Serial.printf(" - %" PRId64 " ERR\n", now);
        break;
    }
  });

  jsy.begin(&Serial2, 16, 17);

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
    jsy.begin(&Serial2, 16, 17);

    Serial.printf("\njsy.read() at %d bauds:\n", static_cast<int>(rates[i]));

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
