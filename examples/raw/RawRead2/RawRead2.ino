#include <Arduino.h>

// Prof Solaire's code + improvements

static constexpr uint8_t request[] = {0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    yield();

  Serial2.begin(38400, SERIAL_8N1, RX2, TX2);
}

void loop() {
  Serial2.write(request, sizeof(request));

  printf("< ");
  size_t count = 0;
  while (Serial2.available()) {
    count++;
    printf("0x%02X ", Serial2.read());
  }
  printf("\ntotal: %d\n", count);

  delay(1000);
}
