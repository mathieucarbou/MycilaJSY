#include <Arduino.h>

// Prof Solaire's code + improvements + readBytes + timeout

static constexpr uint8_t request[] = {0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    yield();

  Serial2.begin(38400, SERIAL_8N1, 16, 17);
}

void loop() {
  Serial2.write(request, sizeof(request));
  Serial2.setTimeout(1000);

  uint8_t data[61];
  size_t count = Serial2.readBytes(data, 61);

  printf("< ");
  for (size_t i = 0; i < count; i++) {
    printf("0x%02X ", data[i]);
  }
  printf("\ntotal: %d\n", count);

  printf("< ");
  size_t dropped = 0;
  while (Serial2.available()) {
    dropped++;
    printf("0x%02X ", Serial2.read());
  }
  printf("\ndropped: %d\n", dropped);

  delay(1000);
}
