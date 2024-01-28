#include <MycilaJSY.h>
#include <MycilaLogger.h>

static const Mycila::JSYBaudRate target = Mycila::JSYBaudRate::BAUD_4800;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::Logger.forwardTo(&Serial);

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  Mycila::JSY.begin(17, 16);

  if (Mycila::JSY.isEnabled()) {
    Mycila::Logger.info("APP", "JSY is enabled at %d bauds", Mycila::JSY.getBaudRate());

    if (Mycila::JSY.getBaudRate() != target) {
      Mycila::Logger.warn("APP", "JSY baud rate need to be updated");

      if (Mycila::JSY.updateBaudRate(target)) {
        Mycila::Logger.info("APP", "JSY baud rate updated!");

      } else {
        Mycila::Logger.error("APP", "JSY baud rate update failed");
        Mycila::JSY.end();
      }

    } else {
      Mycila::Logger.info("APP", "JSY baud rate is already up to date to %d", target);
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
