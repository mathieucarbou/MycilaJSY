// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaJSY.h"

#ifdef MYCILA_LOGGER_SUPPORT
  #include <MycilaLogger.h>
extern Mycila::Logger logger;
  #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
#else
  #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#endif

#ifndef GPIO_IS_VALID_OUTPUT_GPIO
  #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) ((gpio_num >= 0) && \
                                               (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
#endif

#ifndef GPIO_IS_VALID_GPIO
  #define GPIO_IS_VALID_GPIO(gpio_num) ((gpio_num >= 0) && \
                                        (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))
#endif

#define LOBYTE(x) (*((uint8_t*)&(x)))     // NOLINT
#define HIBYTE(x) (*((uint8_t*)&(x) + 1)) // NOLINT

#define TAG "JSY"

#define JSY_REGISTER_MODEL1              0x0000
#define JSY_REGISTER_MODEL2              0x0001
#define JSY_REGISTER_VOLTAGE_RANGE       0x0002 // 250V
#define JSY_REGISTER_CURRENT_RANGE       0x0003 // 800 (800 / 10 == 80A)
#define JSY_REGISTER_ID_AND_BAUDS        0x0004 // ID (high byte) and Bauds Rate (low byte)
#define JSY_REGISTER_CH1_VOLTAGE         0x0048
#define JSY_REGISTER_CH1_CURRENT         0x0049
#define JSY_REGISTER_CH1_POWER           0x004A
#define JSY_REGISTER_CH1_ENERGY          0x004B
#define JSY_REGISTER_CH1_PF              0x004C
#define JSY_REGISTER_CH1_ENERGY_RETURNED 0x004D
#define JSY_REGISTER_CH1_POWER_SIGN      0x004E
#define JSY_REGISTER_FREQUENCY           0x004F
#define JSY_REGISTER_CH2_VOLTAGE         0x0050
#define JSY_REGISTER_CH2_CURRENT         0x0051
#define JSY_REGISTER_CH2_POWER           0x0052
#define JSY_REGISTER_CH2_ENERGY          0x0053
#define JSY_REGISTER_CH2_PF              0x0054
#define JSY_REGISTER_CH2_ENERGY_RETURNED 0x0055

#define JSY_ADDRESS_BROADCAST 0x00
#define JSY_ADDRESS_DEFAULT   0x01

#define JSY_CMD_READ  0x03
#define JSY_CMD_WRITE 0x10

#define JSY_READ_REGISTERS_RESPONSE_SIZE 61

// CRC with JSY_ADDRESS_DEFAULT: 0x44 0x18
// CRC with JSY_ADDRESS_BROADCAST: 0x45 0xC9
// Ref: https://crccalc.com (CRC-16/MODBUS)
static constexpr uint8_t JSY_READ_REGISTERS_REQUEST[] PROGMEM = {
  JSY_ADDRESS_BROADCAST,
  JSY_CMD_READ,
  (uint8_t)(JSY_REGISTER_CH1_VOLTAGE >> 8),
  (uint8_t)(JSY_REGISTER_CH1_VOLTAGE & 0xFF),
  0x00, // 14 registers (high byte)
  0x0E, // 14 registers (low byte)
  0x45, // CRC
  0xC9  // CRC
};

#define JSY_RESET_ENERGY_RESPONSE_SIZE 8

// CRC with JSY_ADDRESS_DEFAULT: 0xF3 0xFA
// CRC with JSY_ADDRESS_BROADCAST: 0xF7 0x06
// Ref: https://crccalc.com (CRC-16/MODBUS)
static constexpr uint8_t JSY_RESET_ENERGY_REQUEST[] PROGMEM = {
  JSY_ADDRESS_BROADCAST,
  JSY_CMD_WRITE,
  0x00, // start address high
  0x0C, // start address low
  0x00, // number of registers to write high
  0x02, // number of registers to write low
  0x04, // number of bytes to follow
  0x00, // data
  0x00, // data
  0x00, // data
  0x00, // data
  0xF7, // CRC
  0x06  // CRC
};

#define JSY_CMD_SET_BAUDS_RESPONSE_SIZE 8

static constexpr uint16_t CRCTable[] PROGMEM = {
  0x0000,
  0xC0C1,
  0xC181,
  0x0140,
  0xC301,
  0x03C0,
  0x0280,
  0xC241,
  0xC601,
  0x06C0,
  0x0780,
  0xC741,
  0x0500,
  0xC5C1,
  0xC481,
  0x0440,
  0xCC01,
  0x0CC0,
  0x0D80,
  0xCD41,
  0x0F00,
  0xCFC1,
  0xCE81,
  0x0E40,
  0x0A00,
  0xCAC1,
  0xCB81,
  0x0B40,
  0xC901,
  0x09C0,
  0x0880,
  0xC841,
  0xD801,
  0x18C0,
  0x1980,
  0xD941,
  0x1B00,
  0xDBC1,
  0xDA81,
  0x1A40,
  0x1E00,
  0xDEC1,
  0xDF81,
  0x1F40,
  0xDD01,
  0x1DC0,
  0x1C80,
  0xDC41,
  0x1400,
  0xD4C1,
  0xD581,
  0x1540,
  0xD701,
  0x17C0,
  0x1680,
  0xD641,
  0xD201,
  0x12C0,
  0x1380,
  0xD341,
  0x1100,
  0xD1C1,
  0xD081,
  0x1040,
  0xF001,
  0x30C0,
  0x3180,
  0xF141,
  0x3300,
  0xF3C1,
  0xF281,
  0x3240,
  0x3600,
  0xF6C1,
  0xF781,
  0x3740,
  0xF501,
  0x35C0,
  0x3480,
  0xF441,
  0x3C00,
  0xFCC1,
  0xFD81,
  0x3D40,
  0xFF01,
  0x3FC0,
  0x3E80,
  0xFE41,
  0xFA01,
  0x3AC0,
  0x3B80,
  0xFB41,
  0x3900,
  0xF9C1,
  0xF881,
  0x3840,
  0x2800,
  0xE8C1,
  0xE981,
  0x2940,
  0xEB01,
  0x2BC0,
  0x2A80,
  0xEA41,
  0xEE01,
  0x2EC0,
  0x2F80,
  0xEF41,
  0x2D00,
  0xEDC1,
  0xEC81,
  0x2C40,
  0xE401,
  0x24C0,
  0x2580,
  0xE541,
  0x2700,
  0xE7C1,
  0xE681,
  0x2640,
  0x2200,
  0xE2C1,
  0xE381,
  0x2340,
  0xE101,
  0x21C0,
  0x2080,
  0xE041,
  0xA001,
  0x60C0,
  0x6180,
  0xA141,
  0x6300,
  0xA3C1,
  0xA281,
  0x6240,
  0x6600,
  0xA6C1,
  0xA781,
  0x6740,
  0xA501,
  0x65C0,
  0x6480,
  0xA441,
  0x6C00,
  0xACC1,
  0xAD81,
  0x6D40,
  0xAF01,
  0x6FC0,
  0x6E80,
  0xAE41,
  0xAA01,
  0x6AC0,
  0x6B80,
  0xAB41,
  0x6900,
  0xA9C1,
  0xA881,
  0x6840,
  0x7800,
  0xB8C1,
  0xB981,
  0x7940,
  0xBB01,
  0x7BC0,
  0x7A80,
  0xBA41,
  0xBE01,
  0x7EC0,
  0x7F80,
  0xBF41,
  0x7D00,
  0xBDC1,
  0xBC81,
  0x7C40,
  0xB401,
  0x74C0,
  0x7580,
  0xB541,
  0x7700,
  0xB7C1,
  0xB681,
  0x7640,
  0x7200,
  0xB2C1,
  0xB381,
  0x7340,
  0xB101,
  0x71C0,
  0x7080,
  0xB041,
  0x5000,
  0x90C1,
  0x9181,
  0x5140,
  0x9301,
  0x53C0,
  0x5280,
  0x9241,
  0x9601,
  0x56C0,
  0x5780,
  0x9741,
  0x5500,
  0x95C1,
  0x9481,
  0x5440,
  0x9C01,
  0x5CC0,
  0x5D80,
  0x9D41,
  0x5F00,
  0x9FC1,
  0x9E81,
  0x5E40,
  0x5A00,
  0x9AC1,
  0x9B81,
  0x5B40,
  0x9901,
  0x59C0,
  0x5880,
  0x9841,
  0x8801,
  0x48C0,
  0x4980,
  0x8941,
  0x4B00,
  0x8BC1,
  0x8A81,
  0x4A40,
  0x4E00,
  0x8EC1,
  0x8F81,
  0x4F40,
  0x8D01,
  0x4DC0,
  0x4C80,
  0x8C41,
  0x4400,
  0x84C1,
  0x8581,
  0x4540,
  0x8701,
  0x47C0,
  0x4680,
  0x8641,
  0x8201,
  0x42C0,
  0x4380,
  0x8341,
  0x4100,
  0x81C1,
  0x8081,
  0x4040};

void Mycila::JSY::begin(HardwareSerial& serial,
                        const int8_t rxPin,
                        const int8_t txPin,
                        const JSYBaudRate baudRate,
                        const bool async,
                        const uint8_t core,
                        const uint32_t stackSize,
                        const uint32_t pause) {
  if (_enabled)
    return;

  if (GPIO_IS_VALID_GPIO(rxPin)) {
    _pinRX = (gpio_num_t)rxPin;
  } else {
    LOGE(TAG, "Disable JSY: Invalid Serial RX (JSY TX pin): %" PRId8, rxPin);
    _pinRX = GPIO_NUM_NC;
    return;
  }

  if (GPIO_IS_VALID_OUTPUT_GPIO(txPin)) {
    _pinTX = (gpio_num_t)txPin;
  } else {
    LOGE(TAG, "Disable JSY: Invalid Serial TX (JSY RX pin): %" PRId8, txPin);
    _pinTX = GPIO_NUM_NC;
    return;
  }

  LOGI(TAG, "Enable JSY on Serial RX (JSY TX Pin): %" PRId8 " and Serial TX (JSY RX Pin): %" PRId8, rxPin, txPin);

  _pause = pause;
  _serial = &serial;

  if (baudRate == JSYBaudRate::UNKNOWN) {
    _baudRate = _detectBauds();
    if (_baudRate == JSYBaudRate::UNKNOWN) {
      LOGE(TAG, "Unable to read at any supported speed. Disabling.");
      _serial->end();
      return;
    } else {
      LOGI(TAG, "Detected speed: %" PRIu32 " bauds", (uint32_t)_baudRate);
    }
  } else {
    _baudRate = baudRate;
    _openSerial(baudRate);
    LOGW(TAG, "JSY bauds detection skipped, forcing baud rate: %" PRIu32, (uint32_t)_baudRate);
  }

  assert(!async || xTaskCreateUniversal(_jsyTask, "jsyTask", stackSize, this, MYCILA_JSY_ASYNC_PRIORITY, &_taskHandle, core) == pdPASS);

  _enabled = true;
}

void Mycila::JSY::end() {
  if (_enabled) {
    LOGI(TAG, "Disable JSY...");
    _enabled = false;
    _baudRate = JSYBaudRate::UNKNOWN;
    while (_taskHandle != NULL) {
      // JSY takes at least 40-160 ms to finish a read
      delay(50);
    }
    _current1 = 0;
    _current2 = 0;
    _frequency = 0;
    _power1 = 0;
    _power2 = 0;
    _powerFactor1 = 0;
    _powerFactor2 = 0;
    _voltage1 = 0;
    _voltage2 = 0;
    _serial->end();
  }
}

bool Mycila::JSY::read() {
  if (!_enabled)
    return false;

  if (!_mutex.try_lock_for(std::chrono::milliseconds(1000))) {
    LOGW(TAG, "Cannot read: Serial is busy!");
    return false;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.println("[JSY] read()");
  Serial.printf("[JSY] Sent %d > ", sizeof(JSY_READ_REGISTERS_REQUEST));
  for (size_t i = 0; i < sizeof(JSY_READ_REGISTERS_REQUEST); i++) {
    Serial.printf("0x%02X ", JSY_READ_REGISTERS_REQUEST[i]);
  }
  Serial.println();
#endif

  _serial->flush(false);
  _serial->write(JSY_READ_REGISTERS_REQUEST, sizeof(JSY_READ_REGISTERS_REQUEST));

  uint8_t buffer[JSY_READ_REGISTERS_RESPONSE_SIZE];
  ReadResult result = _timedRead(buffer, JSY_READ_REGISTERS_RESPONSE_SIZE, _baudRate);

  _mutex.unlock();

  if (result == ReadResult::READ_TIMEOUT) {
    // reset live values in case of read timeout
    _current1 = 0;
    _current2 = 0;
    _frequency = 0;
    _power1 = 0;
    _power2 = 0;
    _powerFactor1 = 0;
    _powerFactor2 = 0;
    _voltage1 = 0;
    _voltage2 = 0;
    if (_callback) {
      _callback(JSYEventType::EVT_READ_TIMEOUT);
    }
    return false;
  }

  if (result == ReadResult::READ_ERROR_COUNT || result == ReadResult::READ_ERROR_CRC) {
    // reset live values in case of read failure
    _current1 = 0;
    _current2 = 0;
    _frequency = 0;
    _power1 = 0;
    _power2 = 0;
    _powerFactor1 = 0;
    _powerFactor2 = 0;
    _voltage1 = 0;
    _voltage2 = 0;
    if (_callback) {
      _callback(JSYEventType::EVT_READ_ERROR);
    }
    return false;
  }

  assert(result == ReadResult::READ_SUCCESS);

  float voltage1 = ((buffer[3] << 24) + (buffer[4] << 16) + (buffer[5] << 8) + buffer[6]) * 0.0001;
  float current1 = ((buffer[7] << 24) + (buffer[8] << 16) + (buffer[9] << 8) + buffer[10]) * 0.0001;
  float power1 = ((buffer[11] << 24) + (buffer[12] << 16) + (buffer[13] << 8) + buffer[14]) * (buffer[27] == 1 ? -0.0001 : 0.0001);
  float energy1 = ((buffer[15] << 24) + (buffer[16] << 16) + (buffer[17] << 8) + buffer[18]) * 0.0001;
  float powerFactor1 = ((buffer[19] << 24) + (buffer[20] << 16) + (buffer[21] << 8) + buffer[22]) * 0.001;
  float energyReturned1 = ((buffer[23] << 24) + (buffer[24] << 16) + (buffer[25] << 8) + buffer[26]) * 0.0001;
  // buffer[27] is the sign of power1
  // buffer[28] is the sign of power2
  // buffer[29] unused
  // buffer[30] unused
  float frequency = ((buffer[31] << 24) + (buffer[32] << 16) + (buffer[33] << 8) + buffer[34]) * 0.01;
  float voltage2 = ((buffer[35] << 24) + (buffer[36] << 16) + (buffer[37] << 8) + buffer[38]) * 0.0001;
  float current2 = ((buffer[39] << 24) + (buffer[40] << 16) + (buffer[41] << 8) + buffer[42]) * 0.0001;
  float power2 = ((buffer[43] << 24) + (buffer[44] << 16) + (buffer[45] << 8) + buffer[46]) * (buffer[28] == 1 ? -0.0001 : 0.0001);
  float energy2 = ((buffer[47] << 24) + (buffer[48] << 16) + (buffer[49] << 8) + buffer[50]) * 0.0001;
  float powerFactor2 = ((buffer[51] << 24) + (buffer[52] << 16) + (buffer[53] << 8) + buffer[54]) * 0.001;
  float energyReturned2 = ((buffer[55] << 24) + (buffer[56] << 16) + (buffer[57] << 8) + buffer[58]) * 0.0001;

  bool changed = voltage1 != _voltage1 ||
                 current1 != _current1 ||
                 power1 != _power1 ||
                 energy1 != _energy1 ||
                 powerFactor1 != _powerFactor1 ||
                 energyReturned1 != _energyReturned1 ||
                 frequency != _frequency ||
                 voltage2 != _voltage2 ||
                 current2 != _current2 ||
                 power2 != _power2 ||
                 energy2 != _energy2 ||
                 powerFactor2 != _powerFactor2 ||
                 energyReturned2 != _energyReturned2;

  if (changed) {
    _voltage1 = voltage1;
    _current1 = current1;
    _power1 = power1;
    _energy1 = energy1;
    _powerFactor1 = powerFactor1;
    _energyReturned1 = energyReturned1;
    _frequency = frequency;
    _voltage2 = voltage2;
    _current2 = current2;
    _power2 = power2;
    _energy2 = energy2;
    _powerFactor2 = powerFactor2;
    _energyReturned2 = energyReturned2;
  }

  _lastReadSuccess = millis();

  if (_callback) {
    _callback(JSYEventType::EVT_READ);
    if (changed) {
      _callback(JSYEventType::EVT_CHANGE);
    }
  }

  return true;
}

bool Mycila::JSY::resetEnergy() {
  if (!_enabled)
    return false;

  LOGD(TAG, "Try reset energy data...");

  if (!_mutex.try_lock_for(std::chrono::milliseconds(1000))) {
    LOGW(TAG, "Cannot reset: Serial is busy!");
    return false;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.println("[JSY] resetEnergy()");
  Serial.printf("[JSY] Sent %d > ", sizeof(JSY_RESET_ENERGY_REQUEST));
  for (size_t i = 0; i < sizeof(JSY_RESET_ENERGY_REQUEST); i++) {
    Serial.printf("0x%02X ", JSY_RESET_ENERGY_REQUEST[i]);
  }
  Serial.println();
#endif

  _serial->flush(false);
  _serial->write(JSY_RESET_ENERGY_REQUEST, sizeof(JSY_RESET_ENERGY_REQUEST));

  uint8_t buffer[JSY_RESET_ENERGY_RESPONSE_SIZE];
  ReadResult result = _timedRead(buffer, JSY_RESET_ENERGY_RESPONSE_SIZE, _baudRate);

  _mutex.unlock();

  return result == ReadResult::READ_SUCCESS;
}

bool Mycila::JSY::setBaudRate(const JSYBaudRate baudRate) {
  if (!_enabled)
    return false;

  if (baudRate == JSYBaudRate::UNKNOWN)
    return false;

  if (_baudRate == baudRate)
    return true;

  LOGD(TAG, "Try update baud rate to %" PRIu32 "...", (uint32_t)baudRate);

  uint8_t data[] = {
    JSY_ADDRESS_BROADCAST,
    JSY_CMD_WRITE,
    (uint8_t)(JSY_REGISTER_ID_AND_BAUDS >> 8),
    (uint8_t)(JSY_REGISTER_ID_AND_BAUDS & 0xFF),
    0x00, // number of registers to write (high byte)
    0x01, // number of registers to write (low byte)
    0x02, // number of bytes to follow
    JSY_ADDRESS_DEFAULT,
    0x00, // BAUDS ID
    0x00, // CRC
    0x00  // CRC
  };

  // For CRC: https://crccalc.com
  // Select CRC-16/MODBUS
  switch (baudRate) {
    case JSYBaudRate::BAUD_1200:
      data[8] = 0x03;  // BAUDS ID
      data[9] = 0xEB;  // CRC
      data[10] = 0xD5; // CRC
      break;
    case JSYBaudRate::BAUD_2400:
      data[8] = 0x04;  // BAUDS ID
      data[9] = 0xAA;  // CRC
      data[10] = 0x17; // CRC
      break;
    case JSYBaudRate::BAUD_4800:
      data[8] = 0x05;  // BAUDS ID
      data[9] = 0x6B;  // CRC
      data[10] = 0xD7; // CRC
      break;
    case JSYBaudRate::BAUD_9600:
      data[8] = 0x06;  // BAUDS ID
      data[9] = 0x2B;  // CRC
      data[10] = 0xD6; // CRC
      break;
    case JSYBaudRate::BAUD_19200:
      data[8] = 0x07;  // BAUDS ID
      data[9] = 0xEA;  // CRC
      data[10] = 0x16; // CRC
      break;
    case JSYBaudRate::BAUD_38400:
      data[8] = 0x08;  // BAUDS ID
      data[9] = 0xAA;  // CRC
      data[10] = 0x12; // CRC
      break;
    default:
      assert(false);
      break;
  }

  if (!_mutex.try_lock_for(std::chrono::milliseconds(1000))) {
    LOGW(TAG, "Cannot set baud rate: Serial is busy!");
    return false;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.println("[JSY] setBaudRate()");
  Serial.printf("[JSY] Sent %d > ", sizeof(data));
  for (size_t i = 0; i < sizeof(data); i++) {
    Serial.printf("0x%02X ", data[i]);
  }
  Serial.println();
#endif

  _serial->flush(false);
  _serial->write(data, sizeof(data));

  uint8_t buffer[JSY_CMD_SET_BAUDS_RESPONSE_SIZE];
  ReadResult result = _timedRead(buffer, JSY_CMD_SET_BAUDS_RESPONSE_SIZE, _baudRate);

  if (result != ReadResult::READ_SUCCESS) {
    _mutex.unlock();
    return false;
  }

  _openSerial(baudRate);

  bool success = false;
  for (int i = 0; i < MYCILA_JSY_RETRY_COUNT && !success; i++) {
    success |= _canRead(baudRate);
  }

  if (success) {
    _baudRate = baudRate;
  } else if (_baudRate != JSYBaudRate::UNKNOWN) {
    _openSerial(_baudRate);
  }

  _mutex.unlock();

  return success;
}

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::toJson(const JsonObject& root) const {
  root["enabled"] = _enabled;
  root["speed"] = (uint32_t)_baudRate;
  root["time"] = _lastReadSuccess;
  root["current1"] = _current1;
  root["current2"] = _current2;
  root["energy_returned1"] = _energyReturned1;
  root["energy_returned2"] = _energyReturned2;
  root["energy1"] = _energy1;
  root["energy2"] = _energy2;
  root["frequency"] = _frequency;
  root["power_factor1"] = _powerFactor1;
  root["power_factor2"] = _powerFactor2;
  root["power1"] = _power1;
  root["power2"] = _power2;
  root["voltage1"] = _voltage1;
  root["voltage2"] = _voltage2;
}
#endif

void Mycila::JSY::_openSerial(JSYBaudRate baudRate) {
  LOGD(TAG, "Open serial at %" PRIu32 " bauds", (uint32_t)baudRate);
  _serial->begin((uint32_t)baudRate, SERIAL_8N1, _pinRX, _pinTX);
  _serial->setTimeout(200);
  while (!_serial)
    yield();
  while (!_serial->availableForWrite())
    yield();
  _drop();
  _serial->flush(false);
}

Mycila::JSY::ReadResult Mycila::JSY::_timedRead(uint8_t* buffer, const size_t expectedLength, const JSYBaudRate baudRate) {
  assert(expectedLength > 2);

  size_t count = 0;
  while (count < expectedLength) {
    size_t read = Serial2.readBytes(buffer + count, expectedLength - count);
    if (read) {
      count += read;
    } else {
      break;
    }
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] Read %d < ", count);
  for (size_t i = 0; i < count; i++) {
    Serial.printf("0x%02X ", buffer[i]);
  }
  Serial.println();
#endif

  _drop();

  if (count == 0) {
    LOGD(TAG, "Read timeout");
    return ReadResult::READ_TIMEOUT;
  }

  if (count != expectedLength) {
    LOGD(TAG, "Read error: %d != %d", count, expectedLength);
    return ReadResult::READ_ERROR_COUNT;
  }

  // CRC check
  uint16_t crc = 0xFFFF;
  uint8_t* data = buffer;
  size_t len = expectedLength - 2;
  while (len--) {
    uint8_t temp = *(data++) ^ LOBYTE(crc);
    crc = (crc >> 8) ^ pgm_read_word_near(CRCTable + temp);
  }

  if (buffer[expectedLength - 2] != LOBYTE(crc) || buffer[expectedLength - 1] != HIBYTE(crc)) {
    LOGD(TAG, "Read error: Bad CRC 0x%02X 0x%02X != 0x%02X 0x%02X", buffer[expectedLength - 2], buffer[expectedLength - 1], LOBYTE(crc), HIBYTE(crc));
    return ReadResult::READ_ERROR_CRC;
  }

  return ReadResult::READ_SUCCESS;
}

size_t Mycila::JSY::_drop() {
  size_t count = 0;
  if (_serial->available()) {
#ifdef MYCILA_JSY_DEBUG
    Serial.printf("[JSY] Drop < ");
#endif
    while (_serial->available()) {
#ifdef MYCILA_JSY_DEBUG
      Serial.printf("0x%02X ", _serial->read());
#else
      _serial->read();
#endif
      count++;
    }
#ifdef MYCILA_JSY_DEBUG
    Serial.println();
#endif
  }
  return count;
}

bool Mycila::JSY::_canRead(JSYBaudRate baudRate) {
#ifdef MYCILA_JSY_DEBUG
  Serial.println("[JSY] _canRead()");
  Serial.printf("[JSY] Sent %d > ", sizeof(JSY_READ_REGISTERS_REQUEST));
  for (size_t i = 0; i < sizeof(JSY_READ_REGISTERS_REQUEST); i++) {
    Serial.printf("0x%02X ", JSY_READ_REGISTERS_REQUEST[i]);
  }
  Serial.println();
#endif

  _serial->flush(false);
  _serial->write(JSY_READ_REGISTERS_REQUEST, sizeof(JSY_READ_REGISTERS_REQUEST));

  uint8_t buffer[JSY_READ_REGISTERS_RESPONSE_SIZE];
  return _timedRead(buffer, JSY_READ_REGISTERS_RESPONSE_SIZE, baudRate) == ReadResult::READ_SUCCESS;
}

Mycila::JSYBaudRate Mycila::JSY::_detectBauds() {
  constexpr JSYBaudRate baudRates[] = {
    JSYBaudRate::BAUD_38400,
    JSYBaudRate::BAUD_19200,
    JSYBaudRate::BAUD_9600,
    JSYBaudRate::BAUD_4800,
    JSYBaudRate::BAUD_2400,
    JSYBaudRate::BAUD_1200};
  constexpr size_t baudsRateCount = 6;
  for (int i = 0; i < baudsRateCount * 2; i++) {
    JSYBaudRate baudRate = baudRates[i % baudsRateCount];
    LOGD(TAG, "Trying to read at %" PRIu32 " bauds...", (uint32_t)baudRate);
    _openSerial(baudRate);
    for (int j = 0; j < MYCILA_JSY_RETRY_COUNT; j++) {
      if (_canRead(baudRate)) {
        return baudRate;
      }
    }
  }
  return JSYBaudRate::UNKNOWN;
}

void Mycila::JSY::_jsyTask(void* params) {
  JSY* jsy = reinterpret_cast<JSY*>(params);
  while (jsy->_enabled) {
    if (jsy->read()) {
      if (jsy->_pause > 0) {
        delay(jsy->_pause);
      } else {
        yield();
      }
    } else if (jsy->_pause > 0) {
      delay(jsy->_pause);
    } else {
      delay(10);
    }
  }
  jsy->_taskHandle = NULL;
  vTaskDelete(NULL);
}
