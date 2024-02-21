// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#pragma once

#include <HardwareSerial.h>

#include <mutex>

#ifdef MYCILA_JSY_JSON_SUPPORT
#include <ArduinoJson.h>
#endif

#define MYCILA_JSY_VERSION "5.0.0"
#define MYCILA_JSY_VERSION_MAJOR 5
#define MYCILA_JSY_VERSION_MINOR 0
#define MYCILA_JSY_VERSION_REVISION 0

#ifndef MYCILA_JSY_ASYNC_CORE
#define MYCILA_JSY_ASYNC_CORE 0
#endif

#ifndef MYCILA_JSY_ASYNC_STACK_SIZE
#define MYCILA_JSY_ASYNC_STACK_SIZE 256 * 6
#endif

#ifndef MYCILA_JSY_READ_TIMEOUT_MS
#define MYCILA_JSY_READ_TIMEOUT_MS 200
#endif

namespace Mycila {
  enum class JSYBaudRate {
    UNKNOWN = 0,
    BAUD_4800 = 4800,
    BAUD_9600 = 9600,
    BAUD_19200 = 19200,
    BAUD_38400 = 38400,
  };

  class JSY {
    public:
      ~JSY() { end(); }

      // jsyRXPin: pin connected to the RX of the JSY, jsyTXPin: pin connected to the TX of the JSY
      // The baud rate is automatically detected
      void begin(HardwareSerial* serial,
                 const uint8_t jsyRXPin,
                 const uint8_t jsyTXPin,
                 const bool async = false,
                 uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE);

      void end();

      // IMPORTANT: DO NOT CALL read() in async mode: it will have no effect and will return false.
      bool read();

      // Resets energy counters. Returns true if the reset was successful, or will be done asynchronously.
      bool resetEnergy();

      // Try to change the baud rate of the JSY. Returns true if the baud rate was changed, or will be done asynchronously.
      bool setBaudRate(const JSYBaudRate baudRate);

#ifdef MYCILA_JSY_JSON_SUPPORT
      void toJson(const JsonObject& root) const;
#endif

      gpio_num_t getRXPin() const { return _pinRX; }
      gpio_num_t getTXPin() const { return _pinTX; }
      JSYBaudRate getBaudRate() const { return _baudRate; }
      bool isEnabled() const { return _enabled; }

      float getCurrent1() const { return current1; }
      float getCurrent2() const { return current2; }
      float getEnergy1() const { return energy1; }
      float getEnergy2() const { return energy2; }
      float getEnergyReturned1() const { return energyReturned1; }
      float getEnergyReturned2() const { return energyReturned2; }
      float getFrequency() const { return frequency; }
      float getPower1() const { return power1; }
      float getPower2() const { return power2; }
      float getPowerFactor1() const { return powerFactor1; }
      float getPowerFactor2() const { return powerFactor2; }
      float getVoltage1() const { return voltage1; }
      float getVoltage2() const { return voltage2; }

      // get the uptime in milliseconds of the last successful read
      uint32_t getTime() const { return _lastReadSuccess; }

    private:
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
      HardwareSerial* _serial = nullptr;
      uint32_t _lastReadSuccess = 0;
      TaskHandle_t _taskHandle;
      volatile bool _enabled = false;
      volatile JSYBaudRate _baudRate = JSYBaudRate::UNKNOWN;
      std::mutex _mutex;

    private:
      void _openSerial(JSYBaudRate baudRate);
      size_t _timedRead(uint8_t* buffer, size_t length);
      size_t _drop();
      bool _canRead();
      JSYBaudRate _detectBauds();
      static void _jsyTask(void* pvParameters);
  };
} // namespace Mycila
