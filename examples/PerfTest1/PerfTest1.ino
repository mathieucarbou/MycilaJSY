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

  jsy.begin(Serial2, RX2, TX2);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  delay(1000);

  jsy.read();
  jsy.read();
  jsy.read();
  jsy.toJson(doc.to<JsonObject>());
  serializeJsonPretty(doc, Serial);
  Serial.println();

  // speed test for each bauds

  for (size_t i = 0; i < 4; i++) {
    jsy.setBaudRate(rates[i]);
    jsy.end();
    jsy.begin(Serial2, RX2, TX2);

    Serial.printf("\njsy.read() at %" PRIu32 " bauds:\n", rates[i]);

    // Serial.printf(" -");
    int64_t times[50];
    for (size_t j = 0; j < 50; j++) {
      times[j] = esp_timer_get_time();
      if (jsy.read()) {
        times[j] = esp_timer_get_time() - times[j];
      } else {
        times[j] = 0;
      }
      // Serial.printf(" %" PRId64, times[j]);
    }
    // Serial.println();

    // compute
    double sum = 0;
    size_t n = 0;
    size_t err = 0;
    int64_t min = INT64_MAX;
    int64_t max = 0;
    for (size_t j = 0; j < 50; j++) {
      if (times[j] > 0) {
        sum += times[j];
        n++;
        min = min < times[j] ? min : times[j];
        max = max > times[j] ? max : times[j];
      } else {
        err++;
      }
    }

    // show results
    Serial.printf(" - Errors: %d\n", err);
    Serial.printf(" - Average read time: %" PRId64 " us\n", static_cast<int64_t>(sum / n));
    Serial.printf(" - Min read time: %" PRId64 " us\n", (min == INT64_MAX) ? 0 : min);
    Serial.printf(" - Max read time: %" PRId64 " us\n", max);

    delay(1000);
  }

  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  vTaskDelete(NULL);
}
