#include <ArduinoJson.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
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

  jsy.begin(Serial2, 16, 17);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  jsy.read();
  jsy.read();
  jsy.read();
  jsy.toJson(doc.to<JsonObject>());
  serializeJsonPretty(doc, Serial);
  Serial.println();

  constexpr float minimumLoadPower = 600;
  constexpr float toleratedDiff = minimumLoadPower / 100; // 1%

  // speed test for each bauds

  for (size_t i = 0; i < 4; i++) {
    jsy.setBaudRate(rates[i]);
    jsy.end();
    jsy.begin(Serial2, 16, 17);

    Serial.printf("\njsy.read() at %d bauds:\n", static_cast<int>(rates[i]));

    for (size_t rounds = 1; rounds <= 4; rounds++) {
      Serial.printf(" - ROUND: %d\n", rounds);

      digitalWrite(RELAY_PIN, LOW);
      while (jsy.getActivePower2() > 0) {
        jsy.read();
      }

      float last = 0;
      float now = 0;

      int64_t reactivityTime = 0;
      size_t reactivityCount = 1;

      int64_t rampUpTime = 0;
      size_t rampUpCount = 1;

      int64_t rampDownTime = 0;
      size_t rampDownCount = 1;

      digitalWrite(RELAY_PIN, HIGH);
      int64_t start = esp_timer_get_time();
      while (true) {
        jsy.read();
        now = jsy.getActivePower2();

        if (reactivityTime == 0) {
          if (now > 0) {
            reactivityTime = esp_timer_get_time() - start;
          } else {
            reactivityCount++;
          }
        }

        if (now > minimumLoadPower && abs(now - last) <= toleratedDiff) {
          break;
        } else {
          last = now;
          rampUpCount++;
        }
      }
      rampUpTime = esp_timer_get_time() - start;

      Serial.printf("   * Load Detection time: %" PRId64 " us (%d reads)\n", reactivityTime, reactivityCount);
      Serial.printf("   * Ramp up time: %" PRId64 " us (%d reads)\n", rampUpTime, rampUpCount);

      digitalWrite(RELAY_PIN, LOW);
      start = esp_timer_get_time();
      while (true) {
        jsy.read();
        now = jsy.getActivePower2();

        if (now == 0) {
          break;
        } else {
          rampDownCount++;
        }
      }
      rampDownTime = esp_timer_get_time() - start;

      Serial.printf("   * Ramp down time: %" PRId64 " us (%d reads)\n", rampDownTime, rampDownCount);
    }
  }
}

void loop() {
  vTaskDelete(NULL);
}
