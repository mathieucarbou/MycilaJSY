#include <Arduino.h>

// Prof Solaire's code + improvements + readBytes + timeout + broadcast

void setup() {
  Serial.begin(115200);
  while (!Serial)
    yield();

  Serial2.begin(38400, SERIAL_8N1, 16, 17);
}

void loop() {
  uint8_t request[] = {0x00, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x45, 0xC9};
  for (int i = 0; i < 8; i++) {
    Serial2.write(request[i]);
  }

  printf("< ");
  size_t count = 0;
  while (Serial2.available()) {
    count++;
    printf("0x%02X ", Serial2.read());
  }
  printf("\ntotal: %d\n", count);

  delay(1000);
}
