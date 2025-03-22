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

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  jsy.setCallback([](const Mycila::JSY::EventType& event, const Mycila::JSY::Data& data) {
    if (event == Mycila::JSY::EventType::EVT_READ) {
      JsonDocument doc;
      data.toJson(doc.to<JsonObject>());
      serializeJson(doc, Serial);
      Serial.println();
    }
  });

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  // baud rate will be detected automatically
  jsy.begin(Serial2, RX2, TX2);
  // jsy.begin(Serial2, 16, 17);
  // jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::UNKNOWN);

  // if you know the bauds rate, you can set it manually
  // jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::BAUD_4800);
  // jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::BAUD_9600);
  // jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::BAUD_19200);
  // jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::BAUD_38400);
  jsy.setBaudRate(jsy.getMaxAvailableBaudRate());
}

void loop() {
  if (!jsy.read())
    Serial.printf("Failed to read JSY\n");
  delay(4000);
}
