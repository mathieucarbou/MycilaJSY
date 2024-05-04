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

#define MYCILA_JSY_VERSION "7.0.1"
#define MYCILA_JSY_VERSION_MAJOR 7
#define MYCILA_JSY_VERSION_MINOR 0
#define MYCILA_JSY_VERSION_REVISION 1

#ifndef MYCILA_JSY_ASYNC_CORE
#define MYCILA_JSY_ASYNC_CORE 0
#endif

#ifndef MYCILA_JSY_ASYNC_PRIORITY
#define MYCILA_JSY_ASYNC_PRIORITY 1
#endif

#ifndef MYCILA_JSY_ASYNC_STACK_SIZE
#define MYCILA_JSY_ASYNC_STACK_SIZE 2048
#endif

#ifndef MYCILA_JSY_ASYNC_READ_PAUSE
#define MYCILA_JSY_ASYNC_READ_PAUSE 0
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

  typedef std::function<void()> JSYReadCallback;

  class JSY {
    public:
      ~JSY() { end(); }

      // - rxPin: RX board pin connected to the TX of the JSY
      // - txPin: TX board pin connected to the RX of the JSY
      // - pause: time in milliseconds to wait between each read in async mode
      // The baud rate is automatically detected
      void begin(HardwareSerial* serial,
                 const uint8_t rxPin,
                 const uint8_t txPin,
                 const bool async = false,
                 uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE);

      void end();

      // No need to call read in async mode
      bool read();

      // Resets energy counters. Returns true if the reset was successful.
      bool resetEnergy();

      // Try to change the baud rate of the JSY. Returns true if the baud rate was changed.
      bool setBaudRate(const JSYBaudRate baudRate);

#ifdef MYCILA_JSY_JSON_SUPPORT
      void toJson(const JsonObject& root) const;
#endif

      gpio_num_t getRXPin() const { return _pinRX; }
      gpio_num_t getTXPin() const { return _pinTX; }
      JSYBaudRate getBaudRate() const { return _baudRate; }
      bool isEnabled() const { return _enabled; }

      float getCurrent1() const { return _current1; }
      float getCurrent2() const { return _current2; }
      float getEnergy1() const { return _energy1; }
      float getEnergy2() const { return _energy2; }
      float getEnergyReturned1() const { return _energyReturned1; }
      float getEnergyReturned2() const { return _energyReturned2; }
      float getFrequency() const { return _frequency; }
      // active power, not apparent power
      float getPower1() const { return _power1; }
      float getPower2() const { return _power2; }
      float getPowerFactor1() const { return _powerFactor1; }
      float getPowerFactor2() const { return _powerFactor2; }
      float getVoltage1() const { return _voltage1; }
      float getVoltage2() const { return _voltage2; }
      // apparent power in VA
      float getApparentPower1() const { return _power1 / _powerFactor1; }
      float getApparentPower2() const { return _power2 / _powerFactor2; }

      // get the uptime in milliseconds of the last successful read
      uint32_t getTime() const { return _lastReadSuccess; }

      // check if the device is connected to the , meaning if last read was successful
      bool isConnected() const { return _frequency > 0; }

      void setReadCallback(JSYReadCallback callback) { _readCallback = callback; }

    private:
      volatile float _current1 = 0;        // A
      volatile float _current2 = 0;        // A
      volatile float _energy1 = 0;         // kWh
      volatile float _energy2 = 0;         // kWh
      volatile float _energyReturned1 = 0; // kWh
      volatile float _energyReturned2 = 0; // kWh
      volatile float _frequency = 0;       // Hz
      volatile float _power1 = 0;          // W
      volatile float _power2 = 0;          // W
      volatile float _powerFactor1 = 0;
      volatile float _powerFactor2 = 0;
      volatile float _voltage1 = 0; // V
      volatile float _voltage2 = 0; // V

    private:
      gpio_num_t _pinRX = GPIO_NUM_NC;
      gpio_num_t _pinTX = GPIO_NUM_NC;
      HardwareSerial* _serial = nullptr;
      uint32_t _pause = MYCILA_JSY_ASYNC_READ_PAUSE;
      uint32_t _lastReadSuccess = 0;
      TaskHandle_t _taskHandle;
      volatile bool _enabled = false;
      volatile JSYBaudRate _baudRate = JSYBaudRate::UNKNOWN;
      std::timed_mutex _mutex;
      JSYReadCallback _readCallback = nullptr;

    private:
      void _openSerial(JSYBaudRate baudRate);
      size_t _timedRead(uint8_t* buffer, size_t length);
      size_t _drop();
      bool _canRead();
      JSYBaudRate _detectBauds();
      static void _jsyTask(void* pvParameters);
  };
} // namespace Mycila
