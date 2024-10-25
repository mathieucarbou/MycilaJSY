#include <Arduino.h>
#include <AsyncUDP.h>
#include <WiFi.h>

#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include <FastCRC32.h>    // https://github.com/RobTillaart/CRC

// #define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x01 // old json format
#define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x02 // supports all JSY models
#define MYCILA_UDP_PORT              53964

AsyncUDP udp;

void setup() {
  Serial.begin(115200);

#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  udp.onPacket([](AsyncUDPPacket packet) {
    // buffer[0] == MYCILA_UDP_MSG_TYPE_JSY_DATA (1)
    // buffer[1] == size_t (4)
    // buffer[5] == MsgPack (?)
    // buffer[5 + size] == CRC32 (4)

    size_t len = packet.length();
    uint8_t* buffer = packet.data();

    if (len < 5 || buffer[0] != MYCILA_UDP_MSG_TYPE_JSY_DATA)
      return;

    uint32_t size;
    memcpy(&size, buffer + 1, 4);

    if (len != size + 9)
      return;

    // crc32
    FastCRC32 crc32;
    crc32.add(buffer, size + 5);
    uint32_t crc = crc32.calc();

    if (memcmp(&crc, buffer + size + 5, 4) != 0)
      return;

    JsonDocument doc;
    deserializeMsgPack(doc, buffer + 5, size);
    serializeJsonPretty(doc, Serial);
    Serial.println();
  });

  WiFi.mode(WIFI_STA);
  WiFi.begin("IoT");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  udp.listen(MYCILA_UDP_PORT);
}

// Destroy default Arduino async task
void loop() { vTaskDelete(NULL); }
