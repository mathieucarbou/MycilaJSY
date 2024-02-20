# 1 "/var/folders/g1/2f08lvcn389gtltc9v97xhv40000gn/T/tmpcw4kvrfq"
#include <Arduino.h>
# 1 "/Users/mat/Data/Workspace/me/MycilaJSY/examples/ReadAsync/ReadAsync.ino"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;
void setup();
void loop();
#line 7 "/Users/mat/Data/Workspace/me/MycilaJSY/examples/ReadAsync/ReadAsync.ino"
void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;


  jsy.begin(&Serial2, 17, 16, true, 0);
}

void loop() {
  if (jsy.isEnabled()) {
    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }
  delay(1000);
}