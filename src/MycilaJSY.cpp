// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
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

static const uint8_t JSY_READ_REGISTERS_REQUEST[] = {
  JSY_ADDRESS_DEFAULT,
  JSY_CMD_READ,
  (uint8_t)(JSY_REGISTER_CH1_VOLTAGE >> 8),
  (uint8_t)(JSY_REGISTER_CH1_VOLTAGE & 0xFF),
  0x00, // 14 registers (high byte)
  0x0E, // 14 registers (low byte)
  0x44, // CRC
  0x18  // CRC
};

#define JSY_RESET_ENERGY_RESPONSE_SIZE 8

static const uint8_t JSY_RESET_ENERGY_REQUEST[] = {
  JSY_ADDRESS_DEFAULT,
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
  0xF3, // CRC
  0xFA  // CRC
};

#define JSY_CMD_SET_BAUDS_RESPONSE_SIZE 8

#ifndef GPIO_IS_VALID_OUTPUT_GPIO
  #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) ((gpio_num >= 0) && \
                                               (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
#endif

#ifndef GPIO_IS_VALID_GPIO
  #define GPIO_IS_VALID_GPIO(gpio_num) ((gpio_num >= 0) && \
                                        (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))
#endif

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

  _serial->write(JSY_READ_REGISTERS_REQUEST, sizeof(JSY_READ_REGISTERS_REQUEST));
  _serial->flush();

  uint8_t buffer[JSY_READ_REGISTERS_RESPONSE_SIZE];
  const size_t count = _timedRead(buffer, JSY_READ_REGISTERS_RESPONSE_SIZE);

  _mutex.unlock();

  if (!count) {
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

  if (count != JSY_READ_REGISTERS_RESPONSE_SIZE || buffer[0] != 0x01) {
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
    LOGD(TAG, "Read failed: %d", count);
    if (_callback) {
      _callback(JSYEventType::EVT_READ_ERROR);
    }
    return false;
  }

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

  LOGI(TAG, "Reset Energy data...");

  if (!_mutex.try_lock_for(std::chrono::milliseconds(1000))) {
    LOGW(TAG, "Cannot reset: Serial is busy!");
    return false;
  }

  _serial->write(JSY_RESET_ENERGY_REQUEST, sizeof(JSY_RESET_ENERGY_REQUEST));
  _serial->flush();

  uint8_t buffer[JSY_RESET_ENERGY_RESPONSE_SIZE];
  const size_t count = _timedRead(buffer, JSY_RESET_ENERGY_RESPONSE_SIZE);

  _mutex.unlock();

  if (!count) {
    return false;
  }

  if (count != JSY_RESET_ENERGY_RESPONSE_SIZE) {
    LOGW(TAG, "Reset failed!");
    return false;
  }

  return true;
}

bool Mycila::JSY::setBaudRate(const JSYBaudRate baudRate) {
  if (!_enabled)
    return false;

  if (baudRate == JSYBaudRate::UNKNOWN)
    return false;

  if (_baudRate == baudRate)
    return true;

  LOGI(TAG, "Update baud rate to %" PRIu32 "...", (uint32_t)baudRate);

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

  _serial->write(data, 11);
  _serial->flush();

  uint8_t buffer[JSY_CMD_SET_BAUDS_RESPONSE_SIZE];
  const size_t count = _timedRead(buffer, JSY_CMD_SET_BAUDS_RESPONSE_SIZE);

  if (!count) {
    return false;
  }

  if (count != 8) {
    LOGW(TAG, "Update baud rate to %" PRIu32 " failed!", (uint32_t)baudRate);
    return false;
  }

  _openSerial(baudRate);

  bool success = false;
  for (int i = 0; i < 5 && !success; i++) {
    success = _canRead();
  }

  _mutex.unlock();

  if (success) {
    _baudRate = baudRate;

  } else if (_baudRate != JSYBaudRate::UNKNOWN) {
    _openSerial(_baudRate);
  }

  return success;
}

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::toJson(const JsonObject& root) const {
  root["current1"] = _current1;
  root["current2"] = _current2;
  root["enabled"] = _enabled;
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
  root["time"] = _lastReadSuccess;
}
#endif

void Mycila::JSY::_openSerial(JSYBaudRate baudRate) {
  LOGD(TAG, "Open serial at %" PRIu32 " bauds", (uint32_t)baudRate);
  _serial->begin((uint32_t)baudRate, SERIAL_8N1, _pinRX, _pinTX);
  _serial->setTimeout(MYCILA_JSY_READ_TIMEOUT_MS);
  while (!_serial)
    yield();
  _serial->flush();
  _drop();
  while (!_serial->availableForWrite())
    yield();
}

size_t Mycila::JSY::_timedRead(uint8_t* buffer, size_t length) {
  // const uint32_t now = millis();
  // readBytes is used because is is faster on ESP IDF since it directly calls the uart_read_bytes function.
  // Most of implementations are using a while loop and wait to get the expected amount of data with available(),
  // and are manually handling timeout instead of doing a low level call.
  // This is not efficient and can lead to a few milliseconds lost because they have to introducing delays in the loop.
  // Here, we ensure to read the data as fast as possible when everything works, and when no data is available, we have a timeout.
  const size_t count = _serial->readBytes(buffer, length) + _drop();
  if (count == 0) {
    LOGD(TAG, "Read timeout");
    return count;
  }
  // LOGD(TAG, "Read available after %d ms", millis() - now);
  return count;
}

size_t Mycila::JSY::_drop() {
  size_t count = 0;
  // Serial.printf("Drop: ");
  while (_serial->available()) {
    // Serial.printf("%02X", _serial->read());
    _serial->read();
    count++;
  }
  // if (count > 0)
  //   Serial.printf("Drop: %d\n", count);
  return count;
}

bool Mycila::JSY::_canRead() {
  _serial->write(JSY_READ_REGISTERS_REQUEST, sizeof(JSY_READ_REGISTERS_REQUEST));
  _serial->flush();
  uint8_t buffer[JSY_READ_REGISTERS_RESPONSE_SIZE];
  return _timedRead(buffer, JSY_READ_REGISTERS_RESPONSE_SIZE) == JSY_READ_REGISTERS_RESPONSE_SIZE && buffer[0] == 0x01;
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
  for (int i = 0; i < MYCILA_JSY_DETECT_BAUDS_RETRIES * 2; i++) {
    LOGD(TAG, "Trying to read at %" PRIu32 " bauds...", (uint32_t)baudRates[i % baudsRateCount]);
    _openSerial(baudRates[i % baudsRateCount]);
    for (int j = 0; j < MYCILA_JSY_DETECT_BAUDS_RETRIES; j++) {
      if (_canRead()) {
        return baudRates[i % baudsRateCount];
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
      delay(MYCILA_JSY_READ_TIMEOUT_MS);
    }
  }
  jsy->_taskHandle = NULL;
  vTaskDelete(NULL);
}
