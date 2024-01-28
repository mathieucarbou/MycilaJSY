#include <MycilaJSY.h>
#include <MycilaLogger.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::Logger.forwardTo(&Serial);

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  Mycila::JSY.begin(17, 16);

  if (Mycila::JSY.isEnabled()) {
    Mycila::JSYBaudRate target = Mycila::JSY.getBaudRate() == Mycila::JSYBaudRate::BAUD_38400 ? Mycila::JSYBaudRate::BAUD_4800 : Mycila::JSYBaudRate::BAUD_38400;

    if (Mycila::JSY.updateBaudRate(target)) {
      Mycila::Logger.info("APP", "JSY baud rate updated!");

    } else {
      Mycila::Logger.error("APP", "JSY baud rate update failed");
    }

  } else {
    Mycila::Logger.info("APP", "JSY is disabled");
  }
}

void loop() {
  if (Mycila::JSY.read()) {
    JsonDocument doc;
    Mycila::JSY.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }
  delay(1000);
}
