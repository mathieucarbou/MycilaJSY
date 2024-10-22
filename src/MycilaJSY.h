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

#define MYCILA_JSY_VERSION          "10.0.1"
#define MYCILA_JSY_VERSION_MAJOR    10
#define MYCILA_JSY_VERSION_MINOR    0
#define MYCILA_JSY_VERSION_REVISION 1

// #define MYCILA_JSY_DEBUG 1

// broadcast address to send requests to all devices
#define MYCILA_JSY_ADDRESS_BROADCAST 0x00

// default factory JSY address
#define MYCILA_JSY_ADDRESS_DEFAULT 0x01

// constant returned when device address is unknown
#define MYCILA_JSY_ADDRESS_UNKNOWN 0x00

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

namespace Mycila {
  class JSY {
    public:
      enum class BaudRate {
        UNKNOWN = 0,
        BAUD_1200 = 1200,
        BAUD_2400 = 2400,
        BAUD_4800 = 4800,
        BAUD_9600 = 9600,
        BAUD_19200 = 19200,
        BAUD_38400 = 38400,
      };

      enum class EventType {
        // JSY has successfully read the data
        EVT_READ = 0,
        // JSY has successfully read the data and the values have changed
        EVT_CHANGE,
        // wrong data received when reading values
        EVT_READ_ERROR,
        // timeout reached when reading values
        EVT_READ_TIMEOUT,
        // wrong JSY device read
        EVT_READ_PEER
      };

      typedef struct {
          uint8_t address = MYCILA_JSY_ADDRESS_UNKNOWN; // device address
          float current1 = 0;                           // A
          float current2 = 0;                           // A
          float energy1 = 0;                            // kWh
          float energy2 = 0;                            // kWh
          float energyReturned1 = 0;                    // kWh
          float energyReturned2 = 0;                    // kWh
          float frequency = 0;                          // Hz
          float power1 = 0;                             // W
          float power2 = 0;                             // W
          float powerFactor1 = 0;
          float powerFactor2 = 0;
          float voltage1 = 0; // V
          float voltage2 = 0; // V
      } Data;

      typedef std::function<void(const EventType eventType)> Callback;

      ~JSY() { end(); }

      /**
       * @brief Set the address used to send requests (1-255).
       * Default value is MYCILA_JSY_ADDRESS_BROADCAST (0), which means every device will receive the request.
       */
      void setDestinationAddress(const uint8_t address) { _destinationAddress = address; }

      /**
       * @brief Initialize the JSY with the given RX and TX pins.
       * @param serial The serial port to use
       * @param rxPin RX board pin connected to the TX of the JSY
       * @param txPin TX board pin connected to the RX of the JSY
       * @param async If true, the JSY will be read in a separate task
       * @param core The core to use for the async task
       * @param stackSize The stack size of the async task
       * @param pause Time in milliseconds to wait between each read in async mode
       * @note The baud rate is automatically detected.
       */
      void begin(HardwareSerial& serial, // NOLINT
                 const int8_t rxPin,
                 const int8_t txPin,
                 const bool async = false,
                 const uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 const uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 const uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS) {
        begin(serial, rxPin, txPin, BaudRate::UNKNOWN, async, core, stackSize, pause);
      }

      /**
       * @brief Initialize the JSY with the given RX and TX pins.
       * @param serial The serial port to use
       * @param rxPin RX board pin connected to the TX of the JSY
       * @param txPin TX board pin connected to the RX of the JSY
       * @param baudRate The baud rate of the JSY. If set to BaudRate::UNKNOWN, the baud rate is automatically detected
       * @param async If true, the JSY will be read in a separate task
       * @param core The core to use for the async task
       * @param stackSize The stack size of the async task
       * @param pause Time in milliseconds to wait between each read in async mode
       */
      void begin(HardwareSerial& serial, // NOLINT
                 const int8_t rxPin,
                 const int8_t txPin,
                 const BaudRate baudRate,
                 const bool async = false,
                 const uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 const uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 const uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS);

      /**
       * @brief Ends the JSY communication.
       */
      void end();

      /**
       * @brief Set a new address for a device.
       * @param newAddress The new address to set (1-255)
       * @return true if the address was changed
       * @note The destination address is updated by this function if it was set to the old address.
       */
      bool setDeviceAddress(const uint8_t newAddress) { return setDeviceAddress(_destinationAddress, newAddress); }

      /**
       * @brief Set a new address for a device.
       * @param address The address of the device to change (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for the broadcast address
       * @param newAddress The new address to set (1-255)
       * @return true if the address was changed
       * @note The destination address is updated by this function if it was set to the old address.
       */
      bool setDeviceAddress(const uint8_t address, const uint8_t newAddress);

      /**
       * @brief Read the JSY values.
       * @return true if the read was successful
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      bool read() { return read(_destinationAddress); }

      /**
       * @brief Read the JSY values.
       * @param address The address of the device to read (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @return true if the read was successful
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      bool read(const uint8_t address);

      /**
       * @brief Reset the energy counters of the JSY.
       * @return true if the reset was successful
       * @note This function is blocking until the reset is confirmed or the timeout is reached.
       */
      bool resetEnergy() { return resetEnergy(_destinationAddress); }

      /**
       * @brief Reset the energy counters of the JSY.
       * @param address The address of the device to reset (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @return true if the reset was successful
       * @note This function is blocking until the reset is confirmed or the timeout is reached.
       */
      bool resetEnergy(const uint8_t address);

      // Try to change the baud rate of the JSY. Returns true if the baud rate was changed.
      // This function is blocking until the change is confirmed or the timeout is reached.

      /**
       * @brief Change the baud rate of the JSY.
       * @param baudRate The new baud rate to set
       * @return true if the baud rate was changed
       * @note This function is blocking until the change is confirmed or the timeout is reached.
       */
      bool setBaudRate(const BaudRate baudRate) { return setBaudRate(_destinationAddress, baudRate); }

      /**
       * @brief Change the baud rate of the JSY.
       * @param address The address of the device to change (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @param baudRate The new baud rate to set
       * @return true if the baud rate was changed
       * @note This function is blocking until the change is confirmed or the timeout is reached.
       */
      bool setBaudRate(const uint8_t address, const BaudRate baudRate);

#ifdef MYCILA_JSON_SUPPORT
      void toJson(const JsonObject& root) const;
#endif

      gpio_num_t getRXPin() const { return _pinRX; }
      gpio_num_t getTXPin() const { return _pinTX; }
      BaudRate getBaudRate() const { return _baudRate; }
      // the address of the last device's response
      uint8_t getLastAddress() const { return _lastAddress; }
      bool isEnabled() const { return _enabled; }

      /**
       * @brief Get device address from which the last data was read.
       */
      uint8_t getAddress() const { return _address; }

      float getCurrent1() const { return _current1; }
      float getCurrent2() const { return _current2; }
      float getEnergy1() const { return _energy1; }
      float getEnergy2() const { return _energy2; }
      float getEnergyReturned1() const { return _energyReturned1; }
      float getEnergyReturned2() const { return _energyReturned2; }
      float getFrequency() const { return _frequency; }
      float getActivePower1() const { return _power1; }
      float getActivePower2() const { return _power2; }
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

      void getData(Data& data) const { // NOLINT
        data.address = _address;
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

      void setCallback(Callback callback) { _callback = callback; }

    private:
      volatile uint8_t _address = MYCILA_JSY_ADDRESS_UNKNOWN;
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
      volatile BaudRate _baudRate = BaudRate::UNKNOWN;
      std::timed_mutex _mutex;
      Callback _callback = nullptr;
      uint8_t _destinationAddress = MYCILA_JSY_ADDRESS_BROADCAST;
      uint8_t _lastAddress = MYCILA_JSY_ADDRESS_UNKNOWN;
      uint8_t _buffer[64];

    private:
      enum class ReadResult {
        READ_SUCCESS = 0,
        READ_TIMEOUT,
        READ_ERROR_COUNT,
        READ_ERROR_CRC,
        READ_ERROR_ADDRESS,
      };

      bool _set(const uint8_t address, const uint8_t newAddress, const BaudRate newBaudRate);

      bool _canRead(const uint8_t address, BaudRate baudRate);
      ReadResult _timedRead(const uint8_t expectedAddress, const size_t expectedLen, const BaudRate baudRate);
      void _send(const uint8_t address, const size_t len);

      size_t _drop();
      void _openSerial(BaudRate baudRate);
      BaudRate _detectBauds(const uint8_t address);
      uint16_t _crc16(const uint8_t* buffer, size_t len);

      static void _jsyTask(void* pvParameters);
  };
} // namespace Mycila
