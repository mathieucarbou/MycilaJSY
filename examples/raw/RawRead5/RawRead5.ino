#include <Arduino.h>

// Prof Solaire's code + improvements + readBytes + timeout + broadcast

static constexpr uint8_t request[] = {0x00, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x45, 0xC9};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    yield();

  Serial2.begin(38400, SERIAL_8N1, RX2, TX2);
}

void loop() {
  Serial2.setTimeout(50);
  Serial2.flush(false);
  Serial2.write(request, sizeof(request));

  uint8_t data[0xFF];
  uint8_t expectedLength = 61; // 0x38 + 3 (header) + 2 (CRC16)

  // JSY protocol:
  // Response size: 61 bytes: 0x01 0x03 0x38 ... 0x99 0xED
  // 0: 0x01 == address code
  // 1: 0x03 == function code
  // 2: 0x38 == number of data bytes to follow
  // 3-58: DATA (56 bytes)
  // 59-60: CRC16

  size_t count = 0;
  while (count < expectedLength) {
    size_t read = Serial2.readBytes(data + count, expectedLength - count);
    if (read) {
      count += read;
    } else {
      break;
    }
  }

  if (count < expectedLength) {
    printf("timeout error: %d != %d\n", count, expectedLength);
  } else {
    printf("total: %d\n", count);
  }

  printf("< ");
  for (size_t i = 0; i < count; i++) {
    printf("0x%02X ", data[i]);
  }
  printf("\n");

  if (Serial2.available()) {
    printf("< ");
    size_t dropped = 0;
    while (Serial2.available()) {
      dropped++;
      printf("0x%02X ", Serial2.read());
    }
    printf("\ndropped: %d\n", dropped);
  }

  delay(1000);
}
