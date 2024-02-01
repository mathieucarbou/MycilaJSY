// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#pragma once

#include <HardwareSerial.h>
#include <esp32-hal-gpio.h>

#ifdef MYCILA_JSY_JSON_SUPPORT
#include <ArduinoJson.h>
#endif

#define MYCILA_JSY_VERSION "3.2.1"
#define MYCILA_JSY_VERSION_MAJOR 3
#define MYCILA_JSY_VERSION_MINOR 2
#define MYCILA_JSY_VERSION_REVISION 1

#ifndef MYCILA_JSY_SERIAL
#define MYCILA_JSY_SERIAL Serial2
#endif

#ifndef MYCILA_JSY_ASYNC_CORE
#define MYCILA_JSY_ASYNC_CORE 0
#endif

#ifndef MYCILA_JSY_ASYNC_STACK_SIZE
#define MYCILA_JSY_ASYNC_STACK_SIZE 256 * 5
#endif

#ifndef MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS
#define MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS 60
#endif

namespace Mycila {
  enum class JSYBaudRate {
    UNKNOWN = 0,
    BAUD_4800 = 4800,
    BAUD_9600 = 9600,
    BAUD_19200 = 19200,
    BAUD_38400 = 38400,
  };

  enum class JSYState {
    IDLE = 0,
    READ = 1,
    RESET = 2,
    BAUDS = 3,
  };

  class JSYClass {
    public:
      // jsyRXPin: pin connected to the RX of the JSY, jsyTXPin: pin connected to the TX of the JSY
      // The baud rate is automatically detected
      void begin(const uint8_t jsyRXPin,
                 const uint8_t jsyTXPin,
                 HardwareSerial* serial = &MYCILA_JSY_SERIAL,
                 const bool async = false,
                 uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS,
                 uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE);

      void end();

      // IMPORTANT: DO NOT CALL read() in async mode: it will have no effect and will return false.
      bool read();

      // Resets energy counters. Returns true if the reset was successful, or will be done asynchronously.
      bool resetEnergy();

      // Try to change the baud rate of the JSY. Returns true if the baud rate was changed, or will be done asynchronously.
      bool updateBaudRate(const JSYBaudRate baudRate);

#ifdef MYCILA_JSY_JSON_SUPPORT
      void toJson(const JsonObject& root) const;
#endif

      gpio_num_t getRXPin() const { return _pinRX; }
      gpio_num_t getTXPin() const { return _pinTX; }
      JSYBaudRate getBaudRate() const { return _baudRate; }
      bool isEnabled() const { return _enabled; }

    public:
      volatile float current1 = 0;        // A
      volatile float current2 = 0;        // A
      volatile float energy1 = 0;         // kWh
      volatile float energy2 = 0;         // kWh
      volatile float energyReturned1 = 0; // kWh
      volatile float energyReturned2 = 0; // kWh
      volatile float frequency = 0;       // Hz
      volatile float power1 = 0;          // W
      volatile float power2 = 0;          // W
      volatile float powerFactor1 = 0;
      volatile float powerFactor2 = 0;
      volatile float voltage1 = 0; // V
      volatile float voltage2 = 0; // V

    private:
      gpio_num_t _pinRX = GPIO_NUM_NC;
      gpio_num_t _pinTX = GPIO_NUM_NC;
      HardwareSerial* _serial = &MYCILA_JSY_SERIAL;
      uint32_t _pause = MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS;
      volatile bool _async = false;
      volatile bool _enabled = false;
      volatile JSYBaudRate _baudRate = JSYBaudRate::UNKNOWN;
      volatile JSYState _state = JSYState::IDLE;
      volatile JSYState _request = JSYState::IDLE;
      volatile JSYBaudRate _requestedBaudRate = JSYBaudRate::UNKNOWN;

    private:
      bool _readRetry(uint8_t maxCount);
      bool _read();
      bool _reset();
      bool _updateBaudRate();
      size_t _drop();
      JSYBaudRate _detectBauds();
      static void _jsyTask(void* pvParameters);
  };

  extern JSYClass JSY;
} // namespace Mycila
