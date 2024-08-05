// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#pragma once

#include <HardwareSerial.h>

#include <mutex>

#ifdef MYCILA_JSON_SUPPORT
  #include <ArduinoJson.h>
#endif

#define MYCILA_JSY_VERSION          "9.1.6"
#define MYCILA_JSY_VERSION_MAJOR    9
#define MYCILA_JSY_VERSION_MINOR    1
#define MYCILA_JSY_VERSION_REVISION 6

#ifndef MYCILA_JSY_ASYNC_CORE
  #define MYCILA_JSY_ASYNC_CORE 0
#endif

#ifndef MYCILA_JSY_ASYNC_PRIORITY
  #define MYCILA_JSY_ASYNC_PRIORITY 1
#endif

#ifndef MYCILA_JSY_ASYNC_STACK_SIZE
  #define MYCILA_JSY_ASYNC_STACK_SIZE 2048
#endif

// time in milliseconds to wait between each read in async mode
#ifndef MYCILA_JSY_ASYNC_READ_PAUSE_MS
  #define MYCILA_JSY_ASYNC_READ_PAUSE_MS 0
#endif

#ifndef MYCILA_JSY_RETRY_COUNT
  #define MYCILA_JSY_RETRY_COUNT 4
#endif

// #define MYCILA_JSY_DEBUG 1

namespace Mycila {
  enum class JSYBaudRate {
    UNKNOWN = 0,
    BAUD_1200 = 1200,
    BAUD_2400 = 2400,
    BAUD_4800 = 4800,
    BAUD_9600 = 9600,
    BAUD_19200 = 19200,
    BAUD_38400 = 38400,
  };

  enum class JSYEventType {
    // JSY has successfully read the data
    EVT_READ = 0,
    // JSY has successfully read the data and the values have changed
    EVT_CHANGE,
    // wrong data received when reading values
    EVT_READ_ERROR,
    // timeout reached when reading values
    EVT_READ_TIMEOUT
  };

  typedef struct {
      float current1 = 0;        // A
      float current2 = 0;        // A
      float energy1 = 0;         // kWh
      float energy2 = 0;         // kWh
      float energyReturned1 = 0; // kWh
      float energyReturned2 = 0; // kWh
      float frequency = 0;       // Hz
      float power1 = 0;          // W
      float power2 = 0;          // W
      float powerFactor1 = 0;
      float powerFactor2 = 0;
      float voltage1 = 0; // V
      float voltage2 = 0; // V
  } JSYData;

  typedef std::function<void(const JSYEventType eventType)> JSYCallback;

  class JSY {
    public:
      ~JSY() { end(); }

      // - rxPin: RX board pin connected to the TX of the JSY
      // - txPin: TX board pin connected to the RX of the JSY
      // - pause: time in milliseconds to wait between each read in async mode
      // The baud rate is automatically detected
      void begin(HardwareSerial& serial, // NOLINT
                 const int8_t rxPin,
                 const int8_t txPin,
                 const bool async = false,
                 const uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 const uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 const uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS) {
        begin(serial, rxPin, txPin, JSYBaudRate::UNKNOWN, async, core, stackSize, pause);
      }

      // - rxPin: RX board pin connected to the TX of the JSY
      // - txPin: TX board pin connected to the RX of the JSY
      // - baudRate: the baud rate of the JSY. If set to JSYBaudRate::UNKNOWN, the baud rate is automatically detected
      // - pause: time in milliseconds to wait between each read in async mode
      // The baud rate is automatically detected
      void begin(HardwareSerial& serial, // NOLINT
                 const int8_t rxPin,
                 const int8_t txPin,
                 const JSYBaudRate baudRate,
                 const bool async = false,
                 const uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 const uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 const uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS);

      void end();

      // Read the JSY values. Returns true if the read was successful.
      // This function is blocking until the data is read or the timeout is reached.
      // No need to call read in async mode
      bool read();

      // Resets energy counters. Returns true if the reset was successful.
      // This function is blocking until the reset is confirmed or the timeout is reached.
      bool resetEnergy();

      // Try to change the baud rate of the JSY. Returns true if the baud rate was changed.
      // This function is blocking until the change is confirmed or the timeout is reached.
      bool setBaudRate(const JSYBaudRate baudRate);

#ifdef MYCILA_JSON_SUPPORT
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
      float getResistance1() const { return _current1 == 0 ? 0 : abs(_power1 / (_current1 * _current1)); }
      float getResistance2() const { return _current2 == 0 ? 0 : abs(_power2 / (_current2 * _current2)); }
      float getVoltage1() const { return _voltage1; }
      float getVoltage2() const { return _voltage2; }
      float getDimmedVoltage1() const { return _current1 == 0 || _power1 < 0 ? 0 : _power1 / _current1; }
      float getDimmedVoltage2() const { return _current2 == 0 || _power2 < 0 ? 0 : _power2 / _current2; }
      float getNominalPower1() const {
        float r1 = getResistance1();
        return r1 == 0 ? 0 : _voltage1 * _voltage1 / r1;
      }
      float getNominalPower2() const {
        float r2 = getResistance2();
        return r2 == 0 ? 0 : _voltage2 * _voltage2 / r2;
      }
      // apparent power in VA
      float getApparentPower1() const { return _powerFactor1 == 0 ? 0 : _power1 / _powerFactor1; }
      float getApparentPower2() const { return _powerFactor2 == 0 ? 0 : _power2 / _powerFactor2; }
      // THDi computation based on a phi "shift" angle
      // // https://fr.electrical-installation.org/frwiki/Indicateur_de_distorsion_harmonique_:_facteur_de_puissance
      float getTHDi1(float phi) const { return _powerFactor1 == 0 ? 0 : sqrt(pow(cos(phi), 2) / pow(_powerFactor1, 2) - 1); }
      float getTHDi2(float phi) const { return _powerFactor2 == 0 ? 0 : sqrt(pow(cos(phi), 2) / pow(_powerFactor2, 2) - 1); }

      // get the uptime in milliseconds of the last successful read
      uint32_t getTime() const { return _lastReadSuccess; }

      void getData(JSYData& data) const { // NOLINT
        data.current1 = _current1;
        data.current2 = _current2;
        data.energy1 = _energy1;
        data.energy2 = _energy2;
        data.energyReturned1 = _energyReturned1;
        data.energyReturned2 = _energyReturned2;
        data.frequency = _frequency;
        data.power1 = _power1;
        data.power2 = _power2;
        data.powerFactor1 = _powerFactor1;
        data.powerFactor2 = _powerFactor2;
        data.voltage1 = _voltage1;
        data.voltage2 = _voltage2;
      }

      // check if the device is connected to the grid, meaning if last read was successful
      bool isConnected() const { return _frequency > 0; }

      void setCallback(JSYCallback callback) { _callback = callback; }

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
      uint32_t _pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS;
      uint32_t _lastReadSuccess = 0;
      TaskHandle_t _taskHandle;
      volatile bool _enabled = false;
      volatile JSYBaudRate _baudRate = JSYBaudRate::UNKNOWN;
      std::timed_mutex _mutex;
      JSYCallback _callback = nullptr;

    private:
      enum class ReadResult {
        READ_SUCCESS = 0,
        READ_TIMEOUT,
        READ_ERROR_COUNT,
        READ_ERROR_CRC
      };
      void _openSerial(JSYBaudRate baudRate);
      ReadResult _timedRead(uint8_t* buffer, const size_t length, const JSYBaudRate baudRate);
      size_t _drop();
      bool _canRead(JSYBaudRate baudRate);
      JSYBaudRate _detectBauds();
      static void _jsyTask(void* pvParameters);
  };
} // namespace Mycila
