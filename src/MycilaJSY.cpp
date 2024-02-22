// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#include "MycilaJSY.h"

#define TAG "JSY"

#define JSY_READ_RESPONSE_SIZE 61
#define JSY_RESET_RESPONSE_SIZE 8
#define JSY_BAUDS_RESPONSE_SIZE 8

static const uint8_t JSY_READ_MSG[] = {0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18};

void Mycila::JSY::begin(HardwareSerial* serial, const uint8_t jsyRXPin, const uint8_t jsyTXPin, const bool async, uint8_t core, uint32_t stackSize) {
  if (_enabled)
    return;

  if (serial == nullptr) {
    ESP_LOGE(TAG, "Disable JSY: Invalid serial");
    return;
  }

  if (GPIO_IS_VALID_OUTPUT_GPIO(jsyRXPin)) {
    _pinRX = (gpio_num_t)jsyRXPin;
  } else {
    ESP_LOGE(TAG, "Disable JSY: Invalid JSY RX pin: %u", _pinRX);
    _pinRX = GPIO_NUM_NC;
    return;
  }

  if (GPIO_IS_VALID_GPIO(jsyTXPin)) {
    _pinTX = (gpio_num_t)jsyTXPin;
  } else {
    ESP_LOGE(TAG, "Disable JSY: Invalid JSY TX pin: %u", _pinTX);
    _pinTX = GPIO_NUM_NC;
    return;
  }

  ESP_LOGI(TAG, "Enable JSY...");
  ESP_LOGD(TAG, "- JSY RX Pin (Serial TX): %u", _pinRX);
  ESP_LOGD(TAG, "- JSY TX Pin (Serial RX): %u", _pinTX);
  ESP_LOGD(TAG, "- Async: %s", async ? "true" : "false");

  _serial = serial;
  _baudRate = _detectBauds();

  if (_baudRate == JSYBaudRate::UNKNOWN) {
    ESP_LOGE(TAG, "Unable to read at any supported speed. Disabling.");
    _serial->end();
    return;
  }

  ESP_LOGI(TAG, "Detected speed: %u bauds", (uint32_t)_baudRate);

  assert(!async || xTaskCreateUniversal(_jsyTask, "jsyTask", stackSize, this, MYCILA_JSY_ASYNC_PRIORITY, &_taskHandle, core) == pdPASS);

  _enabled = true;
}

void Mycila::JSY::end() {
  if (_enabled) {
    ESP_LOGI(TAG, "Disable JSY...");
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
    ESP_LOGW(TAG, "Cannot read: Serial is busy!");
    return false;
  }

  _serial->write(JSY_READ_MSG, 8);
  _serial->flush();

  uint8_t buffer[JSY_READ_RESPONSE_SIZE];
  const size_t count = _timedRead(buffer, JSY_READ_RESPONSE_SIZE);

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
    return false;
  }

  if (count != JSY_READ_RESPONSE_SIZE || buffer[0] != 0x01) {
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
    ESP_LOGD(TAG, "Read failed: %d", count);
    return false;
  }

  _voltage1 = ((buffer[3] << 24) + (buffer[4] << 16) + (buffer[5] << 8) + buffer[6]) * 0.0001;
  _current1 = ((buffer[7] << 24) + (buffer[8] << 16) + (buffer[9] << 8) + buffer[10]) * 0.0001;
  _power1 = ((buffer[11] << 24) + (buffer[12] << 16) + (buffer[13] << 8) + buffer[14]) * (buffer[27] == 1 ? -0.0001 : 0.0001);
  _energy1 = ((buffer[15] << 24) + (buffer[16] << 16) + (buffer[17] << 8) + buffer[18]) * 0.0001;
  _powerFactor1 = ((buffer[19] << 24) + (buffer[20] << 16) + (buffer[21] << 8) + buffer[22]) * 0.001;
  _energyReturned1 = ((buffer[23] << 24) + (buffer[24] << 16) + (buffer[25] << 8) + buffer[26]) * 0.0001;
  // buffer[27] is the sign of power1
  // buffer[28] is the sign of power2
  // buffer[29] unused
  // buffer[30] unused
  _frequency = ((buffer[31] << 24) + (buffer[32] << 16) + (buffer[33] << 8) + buffer[34]) * 0.01;
  _voltage2 = ((buffer[35] << 24) + (buffer[36] << 16) + (buffer[37] << 8) + buffer[38]) * 0.0001;
  _current2 = ((buffer[39] << 24) + (buffer[40] << 16) + (buffer[41] << 8) + buffer[42]) * 0.0001;
  _power2 = ((buffer[43] << 24) + (buffer[44] << 16) + (buffer[45] << 8) + buffer[46]) * (buffer[28] == 1 ? -0.0001 : 0.0001);
  _energy2 = ((buffer[47] << 24) + (buffer[48] << 16) + (buffer[49] << 8) + buffer[50]) * 0.0001;
  _powerFactor2 = ((buffer[51] << 24) + (buffer[52] << 16) + (buffer[53] << 8) + buffer[54]) * 0.001;
  _energyReturned2 = ((buffer[55] << 24) + (buffer[56] << 16) + (buffer[57] << 8) + buffer[58]) * 0.0001;

  _lastReadSuccess = millis();
  return true;
}

bool Mycila::JSY::resetEnergy() {
  if (!_enabled)
    return false;

  ESP_LOGI(TAG, "Reset Energy data...");

  const uint8_t data[] = {0x01, 0x10, 0x00, 0x0C, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xFA};

  if (!_mutex.try_lock_for(std::chrono::milliseconds(1000))) {
    ESP_LOGW(TAG, "Cannot reset: Serial is busy!");
    return false;
  }

  _serial->write(data, 13);
  _serial->flush();

  uint8_t buffer[JSY_RESET_RESPONSE_SIZE];
  const size_t count = _timedRead(buffer, JSY_RESET_RESPONSE_SIZE);

  _mutex.unlock();

  if (!count) {
    return false;
  }

  if (count != 8) {
    ESP_LOGD(TAG, "Reset failed!");
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

  ESP_LOGI(TAG, "Update baud rate to %u...", (uint32_t)baudRate);

  uint8_t data[] = {0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00};

  switch (baudRate) {
    case JSYBaudRate::BAUD_4800:
      data[8] = 0x05;
      data[9] = 0x6B;
      data[10] = 0xD7;
      break;
    case JSYBaudRate::BAUD_9600:
      data[8] = 0x06;
      data[9] = 0x2B;
      data[10] = 0xD6;
      break;
    case JSYBaudRate::BAUD_19200:
      data[8] = 0x07;
      data[9] = 0xEA;
      data[10] = 0x16;
      break;
    case JSYBaudRate::BAUD_38400:
      data[8] = 0x08;
      data[9] = 0xAA;
      data[10] = 0x12;
      break;
    default:
      assert(false);
      break;
  }

  if (!_mutex.try_lock_for(std::chrono::milliseconds(1000))) {
    ESP_LOGW(TAG, "Cannot set baud rate: Serial is busy!");
    return false;
  }

  _serial->write(data, 11);
  _serial->flush();

  uint8_t buffer[JSY_BAUDS_RESPONSE_SIZE];
  const size_t count = _timedRead(buffer, JSY_BAUDS_RESPONSE_SIZE);

  if (!count) {
    return false;
  }

  if (count != 8) {
    ESP_LOGD(TAG, "Update baud rate to %u failed!");
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

#ifdef MYCILA_JSY_JSON_SUPPORT
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
  ESP_LOGD(TAG, "Open serial at %u bauds", (uint32_t)baudRate);
  _serial->begin((uint32_t)baudRate, SERIAL_8N1, _pinTX, _pinRX);
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
  const size_t count = _serial->readBytes(buffer, length) + _drop();
  if (count == 0) {
    ESP_LOGD(TAG, "Read timeout");
    return count;
  }
  // ESP_LOGD(TAG, "Read available after %d ms", millis() - now);
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
  _serial->write(JSY_READ_MSG, 8);
  _serial->flush();
  uint8_t buffer[JSY_READ_RESPONSE_SIZE];
  return _timedRead(buffer, JSY_READ_RESPONSE_SIZE) == JSY_READ_RESPONSE_SIZE && buffer[0] == 0x01;
}

Mycila::JSYBaudRate Mycila::JSY::_detectBauds() {
  const JSYBaudRate baudRates[] = {JSYBaudRate::BAUD_38400, JSYBaudRate::BAUD_19200, JSYBaudRate::BAUD_9600, JSYBaudRate::BAUD_4800};
  for (int i = 0; i < 16; i++) {
    ESP_LOGD(TAG, "Trying to read at %u bauds...", (uint32_t)baudRates[i % 4]);
    _openSerial(baudRates[i % 4]);
    if (_canRead())
      return baudRates[i % 4];
  }
  return JSYBaudRate::UNKNOWN;
}

void Mycila::JSY::_jsyTask(void* params) {
  JSY* jsy = reinterpret_cast<JSY*>(params);
  while (jsy->_enabled) {
    if (jsy->read())
      yield();
    else
      delay(MYCILA_JSY_READ_TIMEOUT_MS);
  }
  jsy->_taskHandle = NULL;
  vTaskDelete(NULL);
}
