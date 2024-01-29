// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#include "MycilaJSY.h"

#include <algorithm>

#define TAG "JSY"

#define SUCCESSFUL_JSY_READ_COUNT 61
#define JSY_MIN_PAUSE 30
#define JSY_DETECTION_READ_COUNT 10
#define JSY_RESET_LOOP_COUNT 30
#define JSY_UPDATE_BAUDS_LOOP_COUNT 30

static const uint8_t JSY_READ_MSG[] = {0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18};

void Mycila::JSYClass::begin(const uint8_t jsyRXPin, const uint8_t jsyTXPin, HardwareSerial* serial, const bool async, uint32_t pause, uint8_t core, uint32_t stackSize) {
  if (_enabled)
    return;

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

  _serial->begin((uint32_t)JSYBaudRate::BAUD_38400, SERIAL_8N1, _pinTX, _pinRX);
  while (!_serial)
    delay(max(portTICK_PERIOD_MS, _pause));

  _async = async;
  _pause = pause;
  _serial = serial;

  ESP_LOGI(TAG, "Enable JSY...");
  ESP_LOGD(TAG, "- JSY RX Pin: %u", _pinRX);
  ESP_LOGD(TAG, "- JSY TX Pin: %u", _pinTX);
  ESP_LOGD(TAG, "- Async: %s", _async ? "true" : "false");

  _baudRate = _detectBauds();

  if (_baudRate == JSYBaudRate::UNKNOWN) {
    ESP_LOGW(TAG, "Unable to read JSY at any supported speed. Trying to fix...");
    _serial->begin(4800, SERIAL_8N1, _pinTX, _pinRX);
    _requestedBaudRate = JSYBaudRate::BAUD_38400;
    _updateBaudRate();
    _baudRate = _detectBauds();
  }

  if (_baudRate == JSYBaudRate::UNKNOWN) {
    ESP_LOGE(TAG, "Unable to read JSY at any supported speed. Disabling JSY.");
    _serial->end();
    return;
  }

  ESP_LOGI(TAG, "Detected JSY speed: %u bauds", (uint32_t)_baudRate);

  assert(!_async || xTaskCreateUniversal(_jsyTask, "jsyTask", stackSize, this, 1, nullptr, core) == pdPASS);

  _enabled = true;
}

void Mycila::JSYClass::end() {
  if (_enabled) {
    ESP_LOGI(TAG, "Disable JSY...");
    _enabled = false;
    _baudRate = JSYBaudRate::UNKNOWN;
    while (_state != JSYState::IDLE)
      delay(JSY_MIN_PAUSE);
    current1 = 0;
    current2 = 0;
    energy1 = 0;
    energy2 = 0;
    energyReturned1 = 0;
    energyReturned2 = 0;
    frequency = 0;
    power1 = 0;
    power2 = 0;
    powerFactor1 = 0;
    powerFactor2 = 0;
    voltage1 = 0;
    voltage2 = 0;
    _serial->end();
  }
}

bool Mycila::JSYClass::read() {
  if (!_enabled)
    return false;

  if (_async)
    return false;

  return _read();
}

bool Mycila::JSYClass::resetEnergy() {
  if (!_enabled)
    return false;

  ESP_LOGI(TAG, "Reset JSY Energy data...");

  if (_async) {
    _request = JSYState::RESET;
    return true;
  }

  return _reset();
}

bool Mycila::JSYClass::updateBaudRate(const JSYBaudRate baudRate) {
  if (!_enabled)
    return false;

  if (baudRate == JSYBaudRate::UNKNOWN)
    return false;

  ESP_LOGI(TAG, "Update JSY baud rate to %u...", (uint32_t)baudRate);

  _requestedBaudRate = baudRate;

  if (_async) {
    _request = JSYState::BAUDS;
    return true;
  }

  return _updateBaudRate();
}

#ifdef MYCILA_JSY_JSON_SUPPORT
void Mycila::JSYClass::toJson(const JsonObject& root) const {
  root["current1"] = current1;
  root["current2"] = current2;
  root["enabled"] = _enabled;
  root["energy_returned1"] = energyReturned1;
  root["energy_returned2"] = energyReturned2;
  root["energy1"] = energy1;
  root["energy2"] = energy2;
  root["frequency"] = frequency;
  root["power_factor1"] = powerFactor1;
  root["power_factor2"] = powerFactor2;
  root["power1"] = power1;
  root["power2"] = power2;
  root["voltage1"] = voltage1;
  root["voltage2"] = voltage2;
}
#endif

bool Mycila::JSYClass::_readRetry(uint8_t maxCount) {
  while (maxCount > 0 && !_read()) {
    if (--maxCount == 0)
      return false;
    delay(JSY_MIN_PAUSE);
  }
  return true;
}

bool __attribute__((hot)) Mycila::JSYClass::_read() {
  if (_state != JSYState::IDLE)
    return false;

  _state = JSYState::READ;

  _serial->write(JSY_READ_MSG, 8);
  _serial->flush();

  uint8_t buffer[SUCCESSFUL_JSY_READ_COUNT];
  size_t count = 0;
  // Serial.printf("Read: ");
  while (count < SUCCESSFUL_JSY_READ_COUNT && _serial->available()) {
    buffer[count++] = _serial->read();
    // Serial.printf("%02X", buffer[count - 1]);
  }
  // Serial.printf(" (%d)\n", count);
  count += _drop();

  if (count != SUCCESSFUL_JSY_READ_COUNT || buffer[0] != 0x01) {
    _state = JSYState::IDLE;
    return false;
  }

  voltage1 = ((buffer[3] << 24) + (buffer[4] << 16) + (buffer[5] << 8) + buffer[6]) * 0.0001;
  current1 = ((buffer[7] << 24) + (buffer[8] << 16) + (buffer[9] << 8) + buffer[10]) * 0.0001;
  power1 = ((buffer[11] << 24) + (buffer[12] << 16) + (buffer[13] << 8) + buffer[14]) * (buffer[27] == 1 ? -0.0001 : 0.0001);
  energy1 = ((buffer[15] << 24) + (buffer[16] << 16) + (buffer[17] << 8) + buffer[18]) * 0.0001;
  powerFactor1 = ((buffer[19] << 24) + (buffer[20] << 16) + (buffer[21] << 8) + buffer[22]) * 0.001;
  energyReturned1 = ((buffer[23] << 24) + (buffer[24] << 16) + (buffer[25] << 8) + buffer[26]) * 0.0001;
  // buffer[27] is the sign of power1
  // buffer[28] is the sign of power2
  // buffer[29] unused
  // buffer[30] unused
  frequency = round(((buffer[31] << 24) + (buffer[32] << 16) + (buffer[33] << 8) + buffer[34]) * 0.01);
  voltage2 = ((buffer[35] << 24) + (buffer[36] << 16) + (buffer[37] << 8) + buffer[38]) * 0.0001;
  current2 = ((buffer[39] << 24) + (buffer[40] << 16) + (buffer[41] << 8) + buffer[42]) * 0.0001;
  power2 = ((buffer[43] << 24) + (buffer[44] << 16) + (buffer[45] << 8) + buffer[46]) * (buffer[28] == 1 ? -0.0001 : 0.0001);
  energy2 = ((buffer[47] << 24) + (buffer[48] << 16) + (buffer[49] << 8) + buffer[50]) * 0.0001;
  powerFactor2 = ((buffer[51] << 24) + (buffer[52] << 16) + (buffer[53] << 8) + buffer[54]) * 0.001;
  energyReturned2 = ((buffer[55] << 24) + (buffer[56] << 16) + (buffer[57] << 8) + buffer[58]) * 0.0001;

  _state = JSYState::IDLE;
  return true;
}

bool Mycila::JSYClass::_reset() {
  if (_state != JSYState::IDLE)
    return false;

  _state = JSYState::RESET;

  const uint8_t data[] = {0x01, 0x10, 0x00, 0x0C, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xFA};

  for (size_t i = 0; i < JSY_RESET_LOOP_COUNT; i++) {
    _serial->write(data, 13);
    _serial->flush();
    _drop();
    delay(JSY_MIN_PAUSE);
  }

  _state = JSYState::IDLE;
  return true;
}

bool Mycila::JSYClass::_updateBaudRate() {
  if (_requestedBaudRate == JSYBaudRate::UNKNOWN)
    return false;

  if (_state != JSYState::IDLE)
    return false;

  _state = JSYState::BAUDS;

  uint8_t data[] = {0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00};

  switch (_requestedBaudRate) {
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

  for (size_t i = 0; i < JSY_UPDATE_BAUDS_LOOP_COUNT; i++) {
    _serial->write(data, 11);
    _serial->flush();
    _drop();
    delay(JSY_MIN_PAUSE);
  }

  _state = JSYState::IDLE;
  _serial->begin((uint32_t)_requestedBaudRate, SERIAL_8N1, _pinTX, _pinRX);
  _serial->flush();
  _drop();
  const bool success = _readRetry(JSY_DETECTION_READ_COUNT);

  if (success) {
    // keep new baud rate
    _baudRate = _requestedBaudRate;

  } else {
    // rollback
    _serial->begin((uint32_t)_baudRate, SERIAL_8N1, _pinTX, _pinRX);
    _serial->flush();
    _drop();
  }

  _requestedBaudRate = JSYBaudRate::UNKNOWN;
  return success;
}

size_t Mycila::JSYClass::_drop() {
  size_t count = 0;
  // Serial.printf("Drop: ");
  while (_serial->available()) {
    // Serial.printf("%02X", _serial->read());
    _serial->read();
    count++;
  }
  // Serial.printf(" (%d)\n", count);
  return count;
}

Mycila::JSYBaudRate Mycila::JSYClass::_detectBauds() {
  const JSYBaudRate baudRates[] = {JSYBaudRate::BAUD_38400, JSYBaudRate::BAUD_19200, JSYBaudRate::BAUD_9600, JSYBaudRate::BAUD_4800};
  for (int i = 0; i < 4; i++) {
    ESP_LOGD(TAG, "Trying to read JSY at %u bauds...", (uint32_t)baudRates[i]);
    _serial->begin((uint32_t)baudRates[i], SERIAL_8N1, _pinTX, _pinRX);
    _serial->flush();
    _drop();
    if (_readRetry(JSY_DETECTION_READ_COUNT))
      return baudRates[i];
  }
  return JSYBaudRate::UNKNOWN;
}

void Mycila::JSYClass::_jsyTask(void* params) {
  // Serial.println("JSY async task started");
  // Serial.println(xPortGetCoreID());
  JSYClass* jsy = reinterpret_cast<JSYClass*>(params);
  while (jsy->_enabled) {
    switch (jsy->_request) {
      case JSYState::RESET:
        jsy->_reset();
        break;
      case JSYState::BAUDS:
        jsy->_updateBaudRate();
        break;
      default:
        jsy->_read();
        break;
    }
    jsy->_request = JSYState::IDLE;
    if (jsy->_enabled)
      // https://esp32developer.com/programming-in-c-c/tasks/tasks-vs-co-routines
      delay(max(portTICK_PERIOD_MS, jsy->_pause));
  }
  vTaskDelete(NULL);
}

namespace Mycila {
  JSYClass JSY;
} // namespace Mycila
