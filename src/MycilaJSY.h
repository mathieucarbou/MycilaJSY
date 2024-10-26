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

#define MYCILA_JSY_VERSION          "11.0.0"
#define MYCILA_JSY_VERSION_MAJOR    11
#define MYCILA_JSY_VERSION_MINOR    0
#define MYCILA_JSY_VERSION_REVISION 0

// #define MYCILA_JSY_DEBUG 1

// broadcast address to send requests to all devices
#define MYCILA_JSY_ADDRESS_BROADCAST 0x00
// default factory JSY address
#define MYCILA_JSY_ADDRESS_DEFAULT 0x01
// constant returned when device address is unknown
#define MYCILA_JSY_ADDRESS_UNKNOWN 0x00

// Model value for unknown JSY device
#define MYCILA_JSY_MK_UNKNOWN 0x0000
// Model value for JSY-MK-163 family
#define MYCILA_JSY_MK_163 0x0163
// Model value for JSY-MK-194 family
#define MYCILA_JSY_MK_194 0x0194
// Model value for JSY-MK-333 family
#define MYCILA_JSY_MK_333 0x0333

#define MYCILA_JSY_MK_163_NAME "JSY-MK-163"
#define MYCILA_JSY_MK_194_NAME "JSY-MK-194"
#define MYCILA_JSY_MK_333_NAME "JSY-MK-333"

#ifndef MYCILA_JSY_ASYNC_CORE
  #define MYCILA_JSY_ASYNC_CORE 1
#endif

#ifndef MYCILA_JSY_ASYNC_PRIORITY
  #define MYCILA_JSY_ASYNC_PRIORITY 5
#endif

#ifndef MYCILA_JSY_ASYNC_STACK_SIZE
  #define MYCILA_JSY_ASYNC_STACK_SIZE 3072 // 512 * 6
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

      class Metrics {
        public:
          /**
           * @brief Voltage in volts (V).
           * @note JSY-MK-163, JSY-MK-194, JSY-MK-333
           */
          float voltage = NAN;

          /**
           * @brief Current in amperes (A).
           * @note JSY-MK-163, JSY-MK-194, JSY-MK-333
           */
          float current = NAN;

          /**
           * @brief Active power in watts (W).
           * @note JSY-MK-163, JSY-MK-194, JSY-MK-333
           */
          float activePower = NAN;

          /**
           * @brief Power factor
           * @note JSY-MK-163, JSY-MK-194, JSY-MK-333
           */
          float powerFactor = NAN;

          /**
           * @brief Apparent power in volt-amperes (VA).
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: computed
           */
          float apparentPower = NAN;

          /**
           * @brief Reactive power in volt-amperes reactive (VAr).
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: computed and always be positive since we do not know the phase shift angle (inductive or capacitive load)
           */
          float reactivePower = NAN;

          /**
           * @brief Active energy in kilowatt-hours (kWh).
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: computed, sum of activeEnergyImported and activeEnergyReturned
           */
          float activeEnergy = NAN;

          /**
           * @brief Active energy imported in kilowatt-hours (kWh), going to the load, when activePower > 0
           * @note JSY-MK-163, JSY-MK-194, JSY-MK-333: measured
           */
          float activeEnergyImported = NAN;

          /**
           * @brief Active energy returned in kilowatt-hours (kWh), coming from the load, when activePower < 0
           * @note JSY-MK-163, JSY-MK-194, JSY-MK-333: measured
           */
          float activeEnergyReturned = NAN;

          /**
           * @brief Reactive energy in kilovolt-amperes reactive-hours (kVArh).
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: not available
           */
          float reactiveEnergy = NAN;

          /**
           * @brief Reactive energy imported in kilovolt-amperes reactive-hours (kVArh), going to the load, when reactivePower > 0
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: not available
           */
          float reactiveEnergyImported = NAN;

          /**
           * @brief Reactive energy returned in kilovolt-amperes reactive-hours (kVArh), coming from the load, when reactivePower < 0
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: not available
           */
          float reactiveEnergyReturned = NAN;

          /**
           * @brief Apparent energy in kilovolt-amperes-hours (kVAh).
           * @note JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-194: not available
           */
          float apparentEnergy = NAN;

          /**
           * @brief Compute the total harmonic distortion of current (THDi).
           * See: https://fr.electrical-installation.org/frwiki/Indicateur_de_distorsion_harmonique_:_facteur_de_puissance
           * @param phi The phase shift angle in radians (1 for resistive load)
           * @return The total harmonic distortion of current (THDi)
           */
          float thdi(float phi = 1) const { return powerFactor == 0 ? NAN : sqrt(pow(cos(phi), 2) / pow(powerFactor, 2) - 1); }

          /**
           * @brief Compute the resistance of the load in ohms (R = P / I^2).
           */
          float resistance() const { return current == 0 ? NAN : abs(activePower / (current * current)); }

          /**
           * @brief Compute the dimmed voltage (V = P / I).
           * @note The dimmed voltage is the voltage that would be measured at the output of a TRIAC, SSR or voltage regulator device.
           */
          float dimmedVoltage() const { return current == 0 ? NAN : abs(activePower / current); }

          /**
           * @brief Compute the nominal power of the load in watts (P = V^2 / R).
           * @note The voltage is the nominal voltage measured by the JSY and R is the measured resistance of the load, which can be regulated by a TRIAC, SSR or voltage regulator.
           */
          float nominalPower() const { return activePower == 0 ? NAN : abs(voltage * voltage * current * current / activePower); }

          // clear all values
          void clear();

          // compare two metrics
          bool operator==(const Metrics& other) const;
          // compare two metrics
          bool operator!=(const Metrics& other) const { return !(*this == other); }
          // add two metrics, averaging the voltage
          Metrics& operator+=(const Metrics& other);
          // copy a metric
          void operator=(const Metrics& other);

#ifdef MYCILA_JSON_SUPPORT
          void toJson(const JsonObject& root) const;
#endif
      };

      class Data {
        public:
          uint8_t address = MYCILA_JSY_ADDRESS_UNKNOWN; // device address
          uint16_t model = MYCILA_JSY_MK_UNKNOWN;       // device model
          float frequency = 0;                          // Hz

          // For JSY-MK-163: aggregate == single()
          // For JSY-MK-194: aggregate == channel1() + channel2()
          // For JSY-MK-333: aggregate == phaseA() + phaseB() + phaseC()
          Metrics aggregate;

          // JSY-MK-163
          const Metrics& single() const { return _metrics[0]; }

          // JSY-MK-194 (channel 1)
          const Metrics& channel1() const { return _metrics[0]; }
          // JSY-MK-194 (channel 2)
          const Metrics& channel2() const { return _metrics[1]; }

          // JSY-MK-333 (phase A)
          const Metrics& phaseA() const { return _metrics[0]; }
          // JSY-MK-333 (phase B)
          const Metrics& phaseB() const { return _metrics[1]; }
          // JSY-MK-333 (phase C)
          const Metrics& phaseC() const { return _metrics[2]; }

          // clear all values
          void clear();

          // compare two data
          bool operator==(const Data& other) const;
          // compare two data
          bool operator!=(const Data& other) const { return !(*this == other); }
          // copy a data
          void operator=(const Data& other);

#ifdef MYCILA_JSON_SUPPORT
          void toJson(const JsonObject& root) const;
#endif

        private:
          friend class JSY;
          Metrics _metrics[3];
      };

      typedef std::function<void(const EventType eventType)> Callback;

      ~JSY() { end(); }

      /**
       * @brief Set the address used to send requests (1-255).
       * Default value is MYCILA_JSY_ADDRESS_BROADCAST (0), which means every device will receive the request.
       */
      void setDestinationAddress(const uint8_t address) { _destinationAddress = address; }

      /**
       * @brief Get the address used to send requests.
       * @return The address used to send requests (1-255) or MYCILA_JSY_ADDRESS_BROADCAST (0) for all devices.
       */
      uint8_t getDestinationAddress() const { return _destinationAddress; }

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
       * @note This function is blocking until the address is changed or the timeout is reached.
       */
      bool setDeviceAddress(const uint8_t newAddress) { return setDeviceAddress(_destinationAddress, newAddress); }

      /**
       * @brief Set a new address for a device.
       * @param address The address of the device to change (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for the broadcast address
       * @param newAddress The new address to set (1-255)
       * @return true if the address was changed
       * @note The destination address is updated by this function if it was set to the old address.
       * @note This function is blocking until the address is changed or the timeout is reached.
       */
      bool setDeviceAddress(const uint8_t address, const uint8_t newAddress);

      /**
       * @brief Reads the JSY model.
       * @return The JSY model or MYCILA_JSY_MK_UNKNOWN if the model cannot be read.
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      uint16_t readModel() { return readModel(_destinationAddress); }

      /**
       * @brief Reads the JSY model.
       * @param address The address of the device to read (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @return The JSY model or MYCILA_JSY_MK_UNKNOWN if the model cannot be read.
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      uint16_t readModel(const uint8_t address);

      /**
       * @brief Get the cached JSY model that was read during begin()
       * @return The JSY model or MYCILA_JSY_MK_UNKNOWN if the model cannot be read.
       */
      uint16_t getModel() const { return _model; }

      const char* getModelName() const { return getModelName(_model); }

      /**
       * @brief Get the name of the JSY model
       * @param model The JSY model
       * @return The name of the JSY model (MYCILA_JSY_MK_163_NAME, MYCILA_JSY_MK_194_NAME, MYCILA_JSY_MK_333_NAME) or an empty string if the model is unknown
       */
      static const char* getModelName(uint16_t model);

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
      bool isEnabled() const { return _enabled; }
      BaudRate getBaudRate() const { return _baudRate; }

      /**
       * @brief Get the address of the last device's response.
       * @return The address of the last device's response (1-255) or MYCILA_JSY_ADDRESS_UNKNOWN if no response was received.
       */
      uint8_t getLastAddress() const { return _lastAddress; }

      /**
       * @return The time in milliseconds of the last successful read
       */
      uint32_t getTime() const { return _time; }

      // check if the device is connected to the grid, meaning if last read was successful
      bool isConnected() const { return data.frequency > 0; }

      void setCallback(Callback callback) { _callback = callback; }

    public:
      /**
       * @brief Access the runtime JSY data.
       */
      Data data;

    private:
      Callback _callback = nullptr;
      gpio_num_t _pinRX = GPIO_NUM_NC;
      gpio_num_t _pinTX = GPIO_NUM_NC;
      HardwareSerial* _serial = nullptr;
      std::timed_mutex _mutex;
      TaskHandle_t _taskHandle;
      uint32_t _time = 0;
      uint32_t _pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS;
      uint8_t _buffer[64];
      uint8_t _destinationAddress = MYCILA_JSY_ADDRESS_BROADCAST;
      uint8_t _lastAddress = MYCILA_JSY_ADDRESS_UNKNOWN;
      BaudRate _baudRate = BaudRate::UNKNOWN;
      bool _enabled = false;
      uint16_t _model = MYCILA_JSY_MK_UNKNOWN;

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
