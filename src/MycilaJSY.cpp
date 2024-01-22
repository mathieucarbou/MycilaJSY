// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#include "MycilaJSY.h"
#include "MycilaLogger.h"

#include <algorithm>

#define TAG "JSY"

#define SUCCESSFUL_JSY_READ_COUNT 61

static const uint8_t JSY_READ_MSG[] = {0x01, 0x03, 0x00, 0x48, 0x00, 0x0E, 0x44, 0x18};

void Mycila::JSYClass::begin(const uint8_t jsyRXPin, const uint8_t jsyTXPin, const JSYBaudRate baudRate, HardwareSerial* serial, const bool async, uint32_t pause, uint8_t core, uint32_t stackSize) {
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

  _async = async;
  _pause = pause;
  _serial = serial;

  Logger.info(TAG, "Enable JSY...");
  Logger.debug(TAG, "- JSY RX Pin: %u", _pinRX);
  Logger.debug(TAG, "- JSY TX Pin: %u", _pinTX);
  Logger.debug(TAG, "- Async: %s", _async ? "true" : "false");
  Logger.debug(TAG, "- Baud Rate: %u bps", (uint32_t)baudRate);

  const JSYBaudRate baudRates[] = {JSYBaudRate::BAUD_4800, JSYBaudRate::BAUD_9600, JSYBaudRate::BAUD_19200, JSYBaudRate::BAUD_38400};
  bool ok = false;
  for (int i = 0; i < 4; i++) {
    _serial->begin((uint32_t)baudRates[i], SERIAL_8N1, _pinTX, _pinRX);
    while (!_serial)
      yield();
    if (_read(4)) {
      Logger.debug(TAG, "- Detected baud Rate: %u bps", (uint32_t)baudRates[i]);
      if (baudRates[i] == baudRate) {
        ok = true;
        break;
      }
      _setBaudRate(baudRate);
      _serial->end();
      _serial->begin((uint32_t)baudRate, SERIAL_8N1, _pinTX, _pinRX);
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

  if (_async && xTaskCreateUniversal(_jsyTask, "jsyTask", stackSize, this, 1, nullptr, core) != pdPASS) {
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
  _drop();

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

  uint8_t buffer[SUCCESSFUL_JSY_READ_COUNT];
  size_t count = 0;
  while (count < SUCCESSFUL_JSY_READ_COUNT && _serial->available())
    buffer[count++] = _serial->read();
  count += _drop();

  if (count != SUCCESSFUL_JSY_READ_COUNT || buffer[0] != 0x01) {
    _reading = false;
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
  _drop();

  Logger.debug(TAG, "JSY baud rate updated.");

  return true;
}

size_t Mycila::JSYClass::_drop() {
  size_t count = 0;
  while (_serial->available()) {
    _serial->read();
    count++;
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
      delay(max(portTICK_PERIOD_MS, jsy->_pause));
  }
  vTaskDelete(NULL);
}

namespace Mycila {
  JSYClass JSY;
} // namespace Mycila
