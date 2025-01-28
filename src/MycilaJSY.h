// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#pragma once

#include <HardwareSerial.h>

#include <mutex>
#include <shared_mutex>

#ifdef MYCILA_JSON_SUPPORT
  #include <ArduinoJson.h>
#endif

#define MYCILA_JSY_VERSION          "14.0.0"
#define MYCILA_JSY_VERSION_MAJOR    14
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
// Model value for JSY1031 family
#define MYCILA_JSY_MK_1031 0x1031
// Model value for JSY-MK-163 family
#define MYCILA_JSY_MK_163 0x0163
// Model value for JSY-MK-193 family
#define MYCILA_JSY_MK_193 0x0193
// Model value for JSY-MK-194 family
#define MYCILA_JSY_MK_194 0x0194
// Model value for JSY-MK-227 family
#define MYCILA_JSY_MK_227 0x0227
// Model value for JSY-MK-229 family
#define MYCILA_JSY_MK_229 0x0229
// Model value for JSY-MK-333 family
#define MYCILA_JSY_MK_333 0x0333

#define MYCILA_JSY_MK_1031_NAME "JSY1031"
#define MYCILA_JSY_MK_163_NAME  "JSY-MK-163"
#define MYCILA_JSY_MK_193_NAME  "JSY-MK-193"
#define MYCILA_JSY_MK_194_NAME  "JSY-MK-194"
#define MYCILA_JSY_MK_227_NAME  "JSY-MK-227"
#define MYCILA_JSY_MK_229_NAME  "JSY-MK-229"
#define MYCILA_JSY_MK_333_NAME  "JSY-MK-333"

#ifndef MYCILA_JSY_ASYNC_CORE
  #define MYCILA_JSY_ASYNC_CORE 1
#endif

#ifndef MYCILA_JSY_ASYNC_PRIORITY
  #define MYCILA_JSY_ASYNC_PRIORITY 5
#endif

#ifndef MYCILA_JSY_ASYNC_STACK_SIZE
  #define MYCILA_JSY_ASYNC_STACK_SIZE 4094
#endif

// time in milliseconds to wait between each read in async mode
#ifndef MYCILA_JSY_ASYNC_READ_PAUSE_MS
  #define MYCILA_JSY_ASYNC_READ_PAUSE_MS 0
#endif

// Maximumtime in milliseconds to wait for bytes to arrive when reading a JSY response
// This value is conservative and work with all JSY.
// If you happen to only use specific JSY models, you can reduce this value to speed up the read process.
// Only JSY1031 are slow and require such a big timeout.
// For other models, the timeout can be decreased safely to 500 ms.
#ifndef MYCILA_JSY_READ_TIMEOUT_MS
  #define MYCILA_JSY_READ_TIMEOUT_MS 1000
#endif

#ifndef MYCILA_JSY_RETRY_COUNT
  #define MYCILA_JSY_RETRY_COUNT 3
#endif

namespace Mycila {
  class JSY {
    public:
      enum BaudRate : uint32_t {
        UNKNOWN = 0,
        // Supported by: JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229
        BAUD_1200 = 1200,
        // Supported by: JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229
        BAUD_2400 = 2400,
        // Default for: JSY-MK-163, JSY-MK-194
        // Supported by: JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
        BAUD_4800 = 4800,
        // Default for: JSY1031, JSY-MK-193, JSY-MK-227, JSY-MK-229, JSY-MK-333
        // Supported by: JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
        BAUD_9600 = 9600,
        // Supported by: JSY1031, JSY-MK-193, JSY-MK-194, JSY-MK-333
        BAUD_19200 = 19200,
        // Supported by: JSY-MK-193, JSY-MK-194
        BAUD_38400 = 38400,
      };

      enum class EventType {
        // JSY has successfully read the data
        EVT_READ,
        // JSY has successfully read the data and the values have changed
        EVT_CHANGE,
        // wrong data received when reading values
        EVT_READ_ERROR,
        // timeout reached when reading values
        EVT_READ_TIMEOUT,
        // wrong JSY device read
        EVT_READ_PEER
      };

      enum class Mode {
        UNKNOWN,
        AC,
        DC
      };

      class Metrics {
        public:
          /**
           * @brief Frequency in hertz (Hz).
           * @note JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
           */
          float frequency = NAN; // Hz

          /**
           * @brief Voltage in volts (V).
           * @note JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
           */
          float voltage = NAN;

          /**
           * @brief Current in amperes (A).
           * @note JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
           */
          float current = NAN;

          /**
           * @brief Active power in watts (W). Can be positive or negative.
           * @note JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
           */
          float activePower = NAN;

          /**
           * @brief Power factor. Positive value between 0 and 1.
           * @note JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333
           */
          float powerFactor = NAN;

          /**
           * @brief Apparent power in volt-amperes (VA). Always positive.
           * @note JSY1031, JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-193, JSY-MK-194: computed
           */
          float apparentPower = NAN;

          /**
           * @brief Reactive power in volt-amperes reactive (VAr). Can be positive or negative.
           * @note JSY1031, JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-193, JSY-MK-194: computed and always be positive since we do not know the phase shift angle (inductive or capacitive load)
           */
          float reactivePower = NAN;

          /**
           * @brief Active energy in watt-hours (Wh).
           * @note JSY1031, JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note JSY-MK-163, JSY-MK-193, JSY-MK-194: computed, sum of activeEnergyImported and activeEnergyReturned
           */
          uint32_t activeEnergy = 0;

          /**
           * @brief Active energy imported in watt-hours (Wh), going to the load, when activePower > 0
           * @note JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note: For DC boards, matches the positive energy
           */
          uint32_t activeEnergyImported = 0;

          /**
           * @brief Active energy returned in watt-hours (Wh), coming from the load, when activePower < 0
           * @note JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note: For DC boards, matches the negative energy
           */
          uint32_t activeEnergyReturned = 0;

          /**
           * @brief Reactive energy in volt-amperes reactive-hours (VArh).
           * @note JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           */
          uint32_t reactiveEnergy = 0;

          /**
           * @brief Reactive energy imported in volt-amperes reactive-hours (VArh), going to the load, when reactivePower > 0
           * @note JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note: For DC boards, matches the positive energy
           */
          uint32_t reactiveEnergyImported = 0;

          /**
           * @brief Reactive energy returned in volt-amperes reactive-hours (VArh), coming from the load, when reactivePower < 0
           * @note JSY-MK-227, JSY-MK-229, JSY-MK-333: measured
           * @note: For DC boards, matches the negative energy
           */
          uint32_t reactiveEnergyReturned = 0;

          /**
           * @brief Apparent energy in volt-amperes-hours (VAh).
           * @note JSY-MK-333: measured
           */
          uint32_t apparentEnergy = 0;

          /**
           * @brief voltage phase angle in degrees (°).
           * @note JSY-MK-333: measured
           */
          float phaseAngleU = NAN;

          /**
           * @brief current phase angle in degrees (°).
           * @note JSY-MK-333: measured
           */
          float phaseAngleI = NAN;

          /**
           * @brief voltage - current phase angle displacement in degrees (°).
           * @note JSY-MK-333: measured
           */
          float phaseAngleUI = NAN;

          /**
           * @brief total harmonic distortion of voltage (THDu), as a percentage (%).
           * @note JSY-MK-333: measured
           */
          float thdU = NAN;

          /**
           * @brief total harmonic distortion of current (THDi) as a percentage (%).
           * @note JSY-MK-333: measured
           */
          float thdI = NAN;

          /**
           * @brief Compute the total harmonic distortion percentage of current (THDi).
           * This assumes THDu = 0 (perfect voltage sin wave).
           * See: https://fr.electrical-installation.org/frwiki/Indicateur_de_distorsion_harmonique_:_facteur_de_puissance
           * @param phi The phase shift angle in degrees (°) (0 for resistive load)
           * @return The total harmonic distortion of current (THDi) as a percentage (%)
           */
          float thdi(float phi = 0) const;

          /**
           * @brief Compute the resistance of the load in ohms (R = P / I^2).
           */
          float resistance() const;

          /**
           * @brief Compute the dimmed voltage (V = P / I).
           * @note The dimmed voltage is the voltage that would be measured at the output of a TRIAC, SSR or voltage regulator device.
           */
          float dimmedVoltage() const;

          /**
           * @brief Compute the nominal power of the load in watts (P = V^2 / R).
           * @note The voltage is the nominal voltage measured by the JSY and R is the measured resistance of the load, which can be regulated by a TRIAC, SSR or voltage regulator.
           */
          float nominalPower() const;

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

          // For JSY1031: aggregate == single()
          // For JSY-MK-163: aggregate == single()
          // For JSY-MK-194: aggregate == channel1() + channel2()
          // For JSY-MK-333: aggregate == phaseA() + phaseB() + phaseC()
          Metrics aggregate;

          // JSY-MK-163 and JSY1031
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
          mutable std::shared_mutex _mutexData;
      };

      typedef std::function<void(EventType eventType)> Callback;

      ~JSY() { end(); }

      /**
       * @brief Initialize the JSY with the given RX and TX pins.
       * @param serial The serial port to use
       * @param rxPin RX board pin connected to the TX of the JSY
       * @param txPin TX board pin connected to the RX of the JSY
       * @param async If true, the JSY will be read in a separate task (default: false)
       * @param core The core to use for the async task (default: MYCILA_JSY_ASYNC_CORE)
       * @param stackSize The stack size of the async task (default: MYCILA_JSY_ASYNC_STACK_SIZE)
       * @param pause Time in milliseconds to wait between each read in async mode (default: MYCILA_JSY_ASYNC_READ_PAUSE_MS)
       * @note The baud rate and model is automatically detected.
       */
      void begin(HardwareSerial& serial, // NOLINT
                 int8_t rxPin,
                 int8_t txPin,
                 bool async,
                 uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS) {
        begin(serial, rxPin, txPin, BaudRate::UNKNOWN, MYCILA_JSY_ADDRESS_BROADCAST, MYCILA_JSY_MK_UNKNOWN, async, core, stackSize, pause);
      }

      /**
       * @brief Initialize the JSY with the given RX and TX pins.
       * @param serial The serial port to use
       * @param rxPin RX board pin connected to the TX of the JSY
       * @param txPin TX board pin connected to the RX of the JSY
       * @param baudRate The baud rate of the JSY. If set to BaudRate::UNKNOWN, the baud rate is automatically detected
       * @param destinationAddress The address of the device to communicate with (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @param model The model of the JSY. If set to MYCILA_JSY_MK_UNKNOWN, the model is automatically detected
       * @param async If true, the JSY will be read in a separate task (default: false)
       * @param core The core to use for the async task (default: MYCILA_JSY_ASYNC_CORE)
       * @param stackSize The stack size of the async task (default: MYCILA_JSY_ASYNC_STACK_SIZE)
       * @param pause Time in milliseconds to wait between each read in async mode (default: MYCILA_JSY_ASYNC_READ_PAUSE_MS)
       */
      void begin(HardwareSerial& serial, // NOLINT
                 int8_t rxPin,
                 int8_t txPin,
                 BaudRate baudRate = BaudRate::UNKNOWN,
                 uint8_t destinationAddress = MYCILA_JSY_ADDRESS_BROADCAST,
                 uint16_t model = MYCILA_JSY_MK_UNKNOWN,
                 bool async = false,
                 uint8_t core = MYCILA_JSY_ASYNC_CORE,
                 uint32_t stackSize = MYCILA_JSY_ASYNC_STACK_SIZE,
                 uint32_t pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS);

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
      bool setDeviceAddress(uint8_t newAddress) { return setDeviceAddress(_destinationAddress, newAddress); }

      /**
       * @brief Set a new address for a device.
       * @param address The address of the device to change (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for the broadcast address
       * @param newAddress The new address to set (1-255)
       * @return true if the address was changed
       * @note The destination address is updated by this function if it was set to the old address.
       * @note This function is blocking until the address is changed or the timeout is reached.
       */
      bool setDeviceAddress(uint8_t address, uint8_t newAddress);

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
      uint16_t readModel(uint8_t address);

      /**
       * @brief Get the cached JSY model that was read during begin()
       * @return The JSY model or MYCILA_JSY_MK_UNKNOWN if the model cannot be read.
       */
      uint16_t getModel() const { return _model; }

      const char* getModelName() const { return getModelName(_model); }

      /**
       * @brief Get the name of the JSY model
       * @param model The JSY model
       * @return The name of the JSY model (MYCILA_JSY_MK_163_NAME, MYCILA_JSY_MK_194_NAME, MYCILA_JSY_MK_333_NAME, MYCILA_JSY_MK_1031_NAME) or an empty string if the model is unknown
       */
      static const char* getModelName(uint16_t model);

      /**
       * @brief Reads the JSY mode (AC or DC). Some JSY are able to work with either AC or DC current.
       * @return The mode of the JSY, or Mode::UNKNOWN if there is an error mode cannot be read.
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      Mode readMode() { return _readMode(_destinationAddress, _model); }

      /**
       * @brief Reads the JSY mode (AC or DC). Some JSY are able to work with either AC or DC current.
       * @param address The address of the device to read (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @return The mode of the JSY, or Mode::UNKNOWN if there is an error mode cannot be read.
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      Mode readMode(uint8_t address) { return _readMode(address, readModel(address)); }

      /**
       * @brief Set the JSY mode (AC or DC). Some JSY are able to work with either AC or DC current.
       * @param mode The mode to set
       * @return true if the mode was set
       * @note This function is blocking until the mode is set or the timeout is reached.
       */
      bool setMode(Mode mode) { return _setMode(_destinationAddress, _model, mode); }

      /**
       * @brief Set the JSY mode (AC or DC). Some JSY are able to work with either AC or DC current.
       * @param address The address of the device to set (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @param mode The mode to set
       * @return true if the mode was set
       * @note This function is blocking until the mode is set or the timeout is reached.
       */
      bool setMode(uint8_t address, Mode mode) { return _setMode(address, readModel(address), mode); }

      /**
       * @brief Read the JSY values.
       * @return true if the read was successful
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      bool read() { return _read(_destinationAddress, _model); }

      /**
       * @brief Read the JSY values.
       * @param address The address of the device to read (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @return true if the read was successful
       * @note This function is blocking until the data is read or the timeout is reached.
       */
      bool read(uint8_t address) { return _read(address, readModel(address)); }

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
      bool resetEnergy(uint8_t address);

      // Try to change the baud rate of the JSY. Returns true if the baud rate was changed.
      // This function is blocking until the change is confirmed or the timeout is reached.

      /**
       * @brief Change the baud rate of the JSY.
       * @param baudRate The new baud rate to set
       * @return true if the baud rate was changed
       * @note This function is blocking until the change is confirmed or the timeout is reached.
       */
      bool setBaudRate(BaudRate baudRate) { return setBaudRate(_destinationAddress, baudRate); }

      /**
       * @brief Change the baud rate of the JSY.
       * @param address The address of the device to change (1-255) or MYCILA_JSY_ADDRESS_BROADCAST for all devices
       * @param baudRate The new baud rate to set
       * @return true if the baud rate was changed
       * @note This function is blocking until the change is confirmed or the timeout is reached.
       */
      bool setBaudRate(uint8_t address, BaudRate baudRate);

#ifdef MYCILA_JSON_SUPPORT
      void toJson(const JsonObject& root) const;
#endif

      gpio_num_t getRXPin() const { return _pinRX; }
      gpio_num_t getTXPin() const { return _pinTX; }
      bool isEnabled() const { return _enabled; }
      BaudRate getBaudRate() const { return _baudRate; }

      /**
       * @brief Get the address used to send requests.
       * @return The address used to send requests (1-255) or MYCILA_JSY_ADDRESS_BROADCAST (0) for all devices.
       */
      uint8_t getDestinationAddress() const { return _destinationAddress; }

      BaudRate getMinAvailableBaudRate() const;
      static BaudRate getMinAvailableBaudRate(uint16_t model);

      /**
       * @brief Get the maximum available baud rate supported by the current JSY model connected
       * @return The maximum available baud rate or BaudRate::UNKNOWN if no supported JSY connected
       */
      BaudRate getMaxAvailableBaudRate() const;
      static BaudRate getMaxAvailableBaudRate(uint16_t model);

      /**
       * @brief Check if a baud rate is supported by the current JSY model connected
       * @return true if the baud rate is supported, false if not or if no supported JSY connected
       */
      bool isBaudRateSupported(BaudRate baudRate) const;
      static bool isBaudRateSupported(uint16_t model, BaudRate baudRate);

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
      bool isConnected() const { return data.aggregate.frequency > 0; }

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
      std::timed_mutex _mutexOp;
      TaskHandle_t _taskHandle;
      uint32_t _time = 0;
      uint32_t _pause = MYCILA_JSY_ASYNC_READ_PAUSE_MS;
      uint8_t _destinationAddress = MYCILA_JSY_ADDRESS_BROADCAST;
      uint8_t _lastAddress = MYCILA_JSY_ADDRESS_UNKNOWN;
      BaudRate _baudRate = BaudRate::UNKNOWN;
      bool _enabled = false;
      uint16_t _model = MYCILA_JSY_MK_UNKNOWN;
      // buffer to read/write data
      // biggest need is for JSY-MK-333: 102 registers of 2 bytes each + 5 bytes for the response: 209 bytes
      // we use 14 blocks of 16 bytes: 224 bytes
      uint8_t _buffer[224];

    private:
      enum class ReadResult {
        READ_SUCCESS = 0,
        READ_TIMEOUT,
        READ_ERROR_COUNT,
        READ_ERROR_CRC,
        READ_ERROR_ADDRESS,
      };

      bool _set(uint8_t address, uint8_t newAddress, BaudRate newBaudRate);
      bool _read(uint8_t address, uint16_t model);
      Mode _readMode(uint8_t address, uint16_t model);
      bool _setMode(uint8_t address, uint16_t model, Mode mode);

      bool _canRead(uint8_t address, BaudRate baudRate);
      ReadResult _timedRead(uint8_t expectedAddress, size_t expectedLen, BaudRate baudRate);
      void _send(uint8_t address, size_t len);
      size_t _drop();
      void _openSerial(BaudRate baudRate);
      BaudRate _detectBauds(uint8_t address);

      static uint16_t _crc16(const uint8_t* buffer, size_t len);
      static uint8_t _register8(const uint8_t* buffer, uint16_t registerStart, uint16_t registerSize, uint16_t registerAddress, uint8_t index = 0);
      static uint16_t _register16(const uint8_t* buffer, uint16_t registerStart, uint16_t registerSize, uint16_t registerAddress);
      static uint32_t _register32(const uint8_t* buffer, uint16_t registerStart, uint16_t registerSize, uint16_t registerAddress);
      static void _jsyTask(void* pvParameters);
  };
} // namespace Mycila
