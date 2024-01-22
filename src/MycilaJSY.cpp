// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#include "MycilaJSY.h"
#include "MycilaLogger.h"

#include <algorithm>

#define TAG "JSY"
#define FLUSH_BUFFER_SIZE 256

static const uint8_t JSY_READ_MSG[] = {0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18};
static uint8_t BUFFER[FLUSH_BUFFER_SIZE];

void Mycila::JSYClass::begin(const uint8_t jsyRXPin, const uint8_t jsyTXPin, const bool async, const JSYBaudRate baudRate, HardwareSerial* serial) {
  if (_enabled)
    return;

  if (GPIO_IS_VALID_OUTPUT_GPIO(jsyRXPin)) {
    _pinRX = (gpio_num_t)jsyRXPin;
  } else {
    Logger.error(TAG, "Disable JSY: Invalid JSY RX pin: %u", _pinRX);
    _pinRX = GPIO_NUM_NC;
    return;
  }

  if (GPIO_IS_VALID_GPIO(jsyTXPin)) {
    _pinTX = (gpio_num_t)jsyTXPin;
  } else {
    Logger.error(TAG, "Disable JSY: Invalid JSY TX pin: %u", _pinTX);
    _pinTX = GPIO_NUM_NC;
    return;
  }

  _baudRate = baudRate;
  _async = async;
  _serial = serial;

  Logger.info(TAG, "Enable JSY...");
  Logger.debug(TAG, "- JSY RX Pin: %u", _pinRX);
  Logger.debug(TAG, "- JSY TX Pin: %u", _pinTX);
  Logger.debug(TAG, "- Async: %s", _async ? "true" : "false");
  Logger.debug(TAG, "- Baud Rate: %u bps", (uint32_t)_baudRate);

  const JSYBaudRate baudRates[] = {JSYBaudRate::BAUD_4800, JSYBaudRate::BAUD_9600, JSYBaudRate::BAUD_19200, JSYBaudRate::BAUD_38400};
  bool ok = false;
  for (int i = 0; i < 4; i++) {
    _serial->begin((uint32_t)baudRates[i], SERIAL_8N1, _pinTX, _pinRX);
    while (!_serial)
      yield();
    if (_read(4)) {
      Logger.debug(TAG, "- Detected baud Rate: %u bps", (uint32_t)baudRates[i]);
      if (baudRates[i] == _baudRate) {
        ok = true;
        break;
      }
      _setBaudRate(_baudRate);
      _serial->end();
      _serial->begin((uint32_t)_baudRate, SERIAL_8N1, _pinTX, _pinRX);
      while (!_serial)
        yield();
      if (!_read(4)) {
        Logger.error(TAG, "Unable to read JSY data after baud rate change");
        break;
      }
      ok = true;
      break;
    } else {
      _serial->end();
    }
  }

  if (!ok) {
    Logger.error(TAG, "Unable to read JSY data with any baud rate");
    _serial->end();
    return;
  }

  if (_async && xTaskCreateUniversal(_jsyTask, "jsyTask", MYCILA_JSY_ASYNC_STACK_SIZE, this, 1, nullptr, MYCILA_JSY_ASYNC_CORE) != pdPASS) {
    Logger.error(TAG, "Unable to create JSY async task");
    return;
  }

  _enabled = true;
}

void Mycila::JSYClass::end() {
  if (_disable()) {
    _serial->end();
  }
}

void Mycila::JSYClass::endAndResetEnergy() {
  _disable();

  Logger.warn(TAG, "Energy Reset...");

  const uint8_t data[] = {0x01, 0x10, 0x00, 0x0C, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xFA};
  _serial->write(data, 13);
  _serial->flush();
  _readSerial();

  // Note: do not end() _serial: ESP needs to restart right after sending the reset command
  Logger.debug(TAG, "Energy Reset done");
}

bool Mycila::JSYClass::read() {
  if (!_enabled)
    return false;
  if (_async)
    return false;
  return _read();
}

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

bool Mycila::JSYClass::_read(uint8_t maxCount) {
  while (maxCount > 0 && !_read()) {
    if (--maxCount == 0)
      return false;
    delay(60);
  }
  return true;
}

bool __attribute__((hot)) Mycila::JSYClass::_read() {
  if (_reading)
    return false;

  _reading = true;

  _serial->write(JSY_READ_MSG, 8);
  _serial->flush();
  size_t count = _readSerial();

  if (count != 61 || BUFFER[0] != 0x01) {
    _reading = false;
    return false;
  }

  voltage1 = ((BUFFER[3] << 24) + (BUFFER[4] << 16) + (BUFFER[5] << 8) + BUFFER[6]) * 0.0001;
  current1 = ((BUFFER[7] << 24) + (BUFFER[8] << 16) + (BUFFER[9] << 8) + BUFFER[10]) * 0.0001;
  power1 = ((BUFFER[11] << 24) + (BUFFER[12] << 16) + (BUFFER[13] << 8) + BUFFER[14]) * (BUFFER[27] == 1 ? -0.0001 : 0.0001);
  energy1 = ((BUFFER[15] << 24) + (BUFFER[16] << 16) + (BUFFER[17] << 8) + BUFFER[18]) * 0.0001;
  powerFactor1 = ((BUFFER[19] << 24) + (BUFFER[20] << 16) + (BUFFER[21] << 8) + BUFFER[22]) * 0.001;
  energyReturned1 = ((BUFFER[23] << 24) + (BUFFER[24] << 16) + (BUFFER[25] << 8) + BUFFER[26]) * 0.0001;
  // BUFFER[27] is the sign of power1
  // BUFFER[28] is the sign of power2
  // BUFFER[29] unused
  // BUFFER[30] unused
  frequency = round(((BUFFER[31] << 24) + (BUFFER[32] << 16) + (BUFFER[33] << 8) + BUFFER[34]) * 0.01);
  voltage2 = ((BUFFER[35] << 24) + (BUFFER[36] << 16) + (BUFFER[37] << 8) + BUFFER[38]) * 0.0001;
  current2 = ((BUFFER[39] << 24) + (BUFFER[40] << 16) + (BUFFER[41] << 8) + BUFFER[42]) * 0.0001;
  power2 = ((BUFFER[43] << 24) + (BUFFER[44] << 16) + (BUFFER[45] << 8) + BUFFER[46]) * (BUFFER[28] == 1 ? -0.0001 : 0.0001);
  energy2 = ((BUFFER[47] << 24) + (BUFFER[48] << 16) + (BUFFER[49] << 8) + BUFFER[50]) * 0.0001;
  powerFactor2 = ((BUFFER[51] << 24) + (BUFFER[52] << 16) + (BUFFER[53] << 8) + BUFFER[54]) * 0.001;
  energyReturned2 = ((BUFFER[55] << 24) + (BUFFER[56] << 16) + (BUFFER[57] << 8) + BUFFER[58]) * 0.0001;

  _reading = false;
  return true;
}

bool Mycila::JSYClass::_setBaudRate(JSYBaudRate baudRate) {
  Logger.debug(TAG, "Set JSY baud rate to %u bps...", (uint32_t)baudRate);

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

  _serial->write(data, 11);
  _serial->flush();
  _readSerial();

  Logger.debug(TAG, "JSY baud rate updated.");

  return true;
}

size_t Mycila::JSYClass::_readSerial() {
  size_t count = 0;
  while (const size_t n = _serial->available()) {
    const size_t pos = count % FLUSH_BUFFER_SIZE;
    count += _serial->readBytes(BUFFER + pos, min(n, FLUSH_BUFFER_SIZE - pos));
  }
  return count;
}

bool Mycila::JSYClass::_disable() {
  if (_enabled) {
    Logger.info(TAG, "Disable JSY...");
    _enabled = false;
    while (_reading)
      delay(100);
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
    return true;
  }
  return false;
}

void Mycila::JSYClass::_jsyTask(void* params) {
  // Serial.println("JSY async task started");
  // Serial.println(xPortGetCoreID());
  JSYClass* jsy = reinterpret_cast<JSYClass*>(params);
  while (jsy->_enabled) {
    jsy->_read();
    if (jsy->_enabled)
      // https://esp32developer.com/programming-in-c-c/tasks/tasks-vs-co-routines
      delay(max(portTICK_PERIOD_MS, static_cast<uint32_t>(MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS)));
  }
  vTaskDelete(NULL);
}

namespace Mycila {
  JSYClass JSY;
} // namespace Mycila
