// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#pragma once

#include <ArduinoJson.h>
#include <esp32-hal-gpio.h>

#define MYCILA_JSY_VERSION "2.1.1"
#define MYCILA_JSY_VERSION_MAJOR 2
#define MYCILA_JSY_VERSION_MINOR 1
#define MYCILA_JSY_VERSION_REVISION 1

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
    BAUD_4800 = 4800,
    BAUD_9600 = 9600,
    BAUD_19200 = 19200,
    BAUD_38400 = 38400,
  };

  class JSYClass {
    public:
      // jsyRXPin: pin connected to the RX of the JSY, jsyTXPin: pin connected to the TX of the JSY
      void begin(const uint8_t jsyRXPin,
                 const uint8_t jsyTXPin,
                 const JSYBaudRate baudRate = JSYBaudRate::BAUD_38400,
                 HardwareSerial* serial = &Serial2,
                 const bool async = false,
                 uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS,
                 uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE);

      void end();

      // ends the JSY reading and resets
      // note that after this call, I need to restart the ESP so that the reset can be achieved.
      // TODO: check if we can instead do a voltage cycle (LOW - HIGH) on the pins and restart Serial2
      void endAndResetEnergy();

      gpio_num_t getRXPin() const { return _pinRX; }
      gpio_num_t getTXPin() const { return _pinTX; }
      bool isEnabled() const { return _enabled; }

      void toJson(const JsonObject& root) const;

      // IMPORTANT: DO NOT CALL read() in async mode: it will have no effect and will return false
      bool read();

    public:
      volatile float current1 = 0;        // A
      volatile float current2 = 0;        // A
      volatile float energy1 = 0;         // kWh
      volatile float energy2 = 0;         // kWh
      volatile float energyReturned1 = 0; // kWh
      volatile float energyReturned2 = 0; // kWh
      volatile uint8_t frequency = 0;     // Hz
      volatile float power1 = 0;          // W
      volatile float power2 = 0;          // W
      volatile float powerFactor1 = 0;
      volatile float powerFactor2 = 0;
      volatile float voltage1 = 0; // V
      volatile float voltage2 = 0; // V

    private:
      gpio_num_t _pinRX = GPIO_NUM_NC;
      gpio_num_t _pinTX = GPIO_NUM_NC;
      HardwareSerial* _serial = &Serial2;
      uint32_t _pause = MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS;
      volatile bool _async = false;
      volatile bool _enabled = false;
      volatile bool _reading = false;

    private:
      bool _disable();
      bool _read();
      bool _read(uint8_t maxCount);
      bool _setBaudRate(JSYBaudRate baudRate);
      size_t _drop();
      static void _jsyTask(void* pvParameters);
  };

  extern JSYClass JSY;
} // namespace Mycila
