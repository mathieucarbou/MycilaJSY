// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaJSY.h"

#ifdef MYCILA_LOGGER_SUPPORT
  #include <MycilaLogger.h>
extern Mycila::Logger logger;
  #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
#else
  #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#endif

#ifndef GPIO_IS_VALID_OUTPUT_GPIO
  #define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) ((gpio_num >= 0) && \
                                               (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
#endif

#ifndef GPIO_IS_VALID_GPIO
  #define GPIO_IS_VALID_GPIO(gpio_num) ((gpio_num >= 0) && \
                                        (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))
#endif

#define LOBYTE(x) ((uint8_t)((x) & 0xFF))
#define HIBYTE(x) ((uint8_t)((x) >> 8))

#define TAG              "JSY"
#define JSY_LOCK_TIMEOUT 2000

///////////////////////////////////////////////////////////////////////////////
// JSY COMMON REGISTERS
///////////////////////////////////////////////////////////////////////////////

// system
#define JSY_REGISTER_MODEL1        0x0000 // RO - JSY Model
#define JSY_REGISTER_MODEL2        0x0001 // RO
#define JSY_REGISTER_VOLTAGE_RANGE 0x0002 // RO - Example: 250V
#define JSY_REGISTER_CURRENT_RANGE 0x0003 // RO - Example: 800 (800 / 10 == 80A)

// communication
#define JSY_REGISTER_ID_AND_BAUDS 0x0004 // RW - ID (high byte) and Bauds Rate (low byte)

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-194 REGISTERS
///////////////////////////////////////////////////////////////////////////////

// measurements (14 registers)
#define JSY_194_REGISTER_LEN                 4      // 4 bytes per register
#define JSY_194_REGISTER_CH1_VOLTAGE         0x0048 // RO
#define JSY_194_REGISTER_CH1_CURRENT         0x0049 // RO
#define JSY_194_REGISTER_CH1_POWER           0x004A // RO
#define JSY_194_REGISTER_CH1_ENERGY          0x004B // RW
#define JSY_194_REGISTER_CH1_PF              0x004C // RO
#define JSY_194_REGISTER_CH1_ENERGY_RETURNED 0x004D // RW
#define JSY_194_REGISTER_CH1_POWER_SIGN      0x004E // RO
#define JSY_194_REGISTER_FREQUENCY           0x004F // RO
#define JSY_194_REGISTER_CH2_VOLTAGE         0x0050 // RO
#define JSY_194_REGISTER_CH2_CURRENT         0x0051 // RO
#define JSY_194_REGISTER_CH2_POWER           0x0052 // RO
#define JSY_194_REGISTER_CH2_ENERGY          0x0053 // RW
#define JSY_194_REGISTER_CH2_PF              0x0054 // RO
#define JSY_194_REGISTER_CH2_ENERGY_RETURNED 0x0055 // RW

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-163 REGISTERS
///////////////////////////////////////////////////////////////////////////////

// measurements (10 registers)
#define JSY_163_REGISTER_LEN             2      // 2 bytes per register
#define JSY_163_REGISTER_VOLTAGE         0x0048 // RO
#define JSY_163_REGISTER_CURRENT         0x0049 // RO
#define JSY_163_REGISTER_POWER           0x004A // RO
#define JSY_163_REGISTER_ENERGY          0x004B // RO, + 0x004C, 4 bytes
#define JSY_163_REGISTER_PF              0x004D // RO
#define JSY_163_REGISTER_ENERGY_RETURNED 0x004E // RO, + 0x004F, 4 bytes
#define JSY_163_REGISTER_POWER_SIGN      0x0050 // RO
#define JSY_163_REGISTER_FREQUENCY       0x0051 // RO

///////////////////////////////////////////////////////////////////////////////
// JSY PROTOCOL
///////////////////////////////////////////////////////////////////////////////

// commands values
#define JSY_CMD_READ_REGISTERS  0x03
#define JSY_CMD_READ_RELAY1     0x01
#define JSY_CMD_WRITE_REGISTERS 0x10
#define JSY_CMD_WRITE_RELAY1    0x05

// request indexes
#define JSY_REQUEST_ADDRESS                  0
#define JSY_REQUEST_CMD                      1
#define JSY_REQUEST_READ_REGISTER_ADDR_HIGH  2
#define JSY_REQUEST_READ_REGISTER_ADDR_LOW   3
#define JSY_REQUEST_READ_REGISTER_COUNT_HIGH 4
#define JSY_REQUEST_READ_REGISTER_COUNT_LOW  5
#define JSY_REQUEST_SET_ADDRESS              7
#define JSY_REQUEST_SET_BAUDS                8

// response
#define JSY_RESPONSE_SIZE_READ         5                          // address(1), cmd(1), len(1), data(?), crc(2)
#define JSY_RESPONSE_SIZE_READ_MODEL   JSY_RESPONSE_SIZE_READ + 2 // address(1), cmd(1), len(1), data(2), crc(2)
#define JSY_RESPONSE_SIZE_RESET_ENERGY 8                          // address(1), cmd(1), len(1), register(2), count(2), crc(2)
#define JSY_RESPONSE_SIZE_SET_COM      8                          // address(1), cmd(1), data(4), crc(2)

// response indexes
#define JSY_RESPONSE_ADDRESS  0
#define JSY_RESPONSE_CMD      1
#define JSY_RESPONSE_DATA_LEN 2
#define JSY_RESPONSE_DATA     3

static constexpr uint8_t JSY_REQUEST_READ_REGISTERS[] PROGMEM = {
  MYCILA_JSY_ADDRESS_BROADCAST,
  JSY_CMD_READ_REGISTERS,
  0x00, // register start address (high byte)
  0x00, // register start address (low byte)
  0x00, // number of registers to read (high byte)
  0x00, // number of registers to read (low byte)
  0x00, // CRC (low)
  0x00  // CRC (high)
};
static constexpr size_t JSY_REQUEST_READ_REGISTERS_LEN = sizeof(JSY_REQUEST_READ_REGISTERS);

static constexpr uint8_t JSY_REQUEST_READ_MODEL[] PROGMEM = {
  MYCILA_JSY_ADDRESS_BROADCAST,
  JSY_CMD_READ_REGISTERS,
  HIBYTE(JSY_REGISTER_MODEL1),
  LOBYTE(JSY_REGISTER_MODEL1),
  0x00, // number of registers to read (high byte)
  0x01, // number of registers to read (low byte)
  0x00, // CRC (low)
  0x00  // CRC (high)
};
static constexpr size_t JSY_REQUEST_READ_MODEL_LEN = sizeof(JSY_REQUEST_READ_MODEL);

static constexpr uint8_t JSY_REQUEST_RESET_ENERGY[] PROGMEM = {
  MYCILA_JSY_ADDRESS_BROADCAST,
  JSY_CMD_WRITE_REGISTERS,
  0x00, // start address (high byte)
  0x0C, // start address (low byte)
  0x00, // number of registers to write (high byte)
  0x02, // number of registers to write (low byte)
  0x04, // number of bytes to follow
  0x00, // data
  0x00, // data
  0x00, // data
  0x00, // data
  0x00, // CRC (low)
  0x00  // CRC (high)
};
static constexpr size_t JSY_REQUEST_RESET_ENERGY_LEN = sizeof(JSY_REQUEST_RESET_ENERGY);

static constexpr uint8_t JSY_REQUEST_SET_COM[] PROGMEM = {
  MYCILA_JSY_ADDRESS_BROADCAST,
  JSY_CMD_WRITE_REGISTERS,
  HIBYTE(JSY_REGISTER_ID_AND_BAUDS),
  LOBYTE(JSY_REGISTER_ID_AND_BAUDS),
  0x00, // number of registers to write (high byte)
  0x01, // number of registers to write (low byte)
  0x02, // number of bytes to follow
  0x00, // new device address
  0x00, // BAUDS ID
  0x00, // CRC (low)
  0x00  // CRC (high)
};
static constexpr size_t JSY_REQUEST_SET_COM_LEN = sizeof(JSY_REQUEST_SET_COM);

///////////////////////////////////////////////////////////////////////////////
// Bauds
///////////////////////////////////////////////////////////////////////////////

static constexpr Mycila::JSY::BaudRate BAUD_RATES[] = {
  Mycila::JSY::BaudRate::BAUD_38400,
  Mycila::JSY::BaudRate::BAUD_19200,
  Mycila::JSY::BaudRate::BAUD_9600,
  Mycila::JSY::BaudRate::BAUD_4800,
  Mycila::JSY::BaudRate::BAUD_2400,
  Mycila::JSY::BaudRate::BAUD_1200};
static constexpr size_t BAUD_RATES_COUNT = 6;

///////////////////////////////////////////////////////////////////////////////
// CRC16 Table
///////////////////////////////////////////////////////////////////////////////

static constexpr uint16_t CRCTable[] PROGMEM = {
  0x0000,
  0xC0C1,
  0xC181,
  0x0140,
  0xC301,
  0x03C0,
  0x0280,
  0xC241,
  0xC601,
  0x06C0,
  0x0780,
  0xC741,
  0x0500,
  0xC5C1,
  0xC481,
  0x0440,
  0xCC01,
  0x0CC0,
  0x0D80,
  0xCD41,
  0x0F00,
  0xCFC1,
  0xCE81,
  0x0E40,
  0x0A00,
  0xCAC1,
  0xCB81,
  0x0B40,
  0xC901,
  0x09C0,
  0x0880,
  0xC841,
  0xD801,
  0x18C0,
  0x1980,
  0xD941,
  0x1B00,
  0xDBC1,
  0xDA81,
  0x1A40,
  0x1E00,
  0xDEC1,
  0xDF81,
  0x1F40,
  0xDD01,
  0x1DC0,
  0x1C80,
  0xDC41,
  0x1400,
  0xD4C1,
  0xD581,
  0x1540,
  0xD701,
  0x17C0,
  0x1680,
  0xD641,
  0xD201,
  0x12C0,
  0x1380,
  0xD341,
  0x1100,
  0xD1C1,
  0xD081,
  0x1040,
  0xF001,
  0x30C0,
  0x3180,
  0xF141,
  0x3300,
  0xF3C1,
  0xF281,
  0x3240,
  0x3600,
  0xF6C1,
  0xF781,
  0x3740,
  0xF501,
  0x35C0,
  0x3480,
  0xF441,
  0x3C00,
  0xFCC1,
  0xFD81,
  0x3D40,
  0xFF01,
  0x3FC0,
  0x3E80,
  0xFE41,
  0xFA01,
  0x3AC0,
  0x3B80,
  0xFB41,
  0x3900,
  0xF9C1,
  0xF881,
  0x3840,
  0x2800,
  0xE8C1,
  0xE981,
  0x2940,
  0xEB01,
  0x2BC0,
  0x2A80,
  0xEA41,
  0xEE01,
  0x2EC0,
  0x2F80,
  0xEF41,
  0x2D00,
  0xEDC1,
  0xEC81,
  0x2C40,
  0xE401,
  0x24C0,
  0x2580,
  0xE541,
  0x2700,
  0xE7C1,
  0xE681,
  0x2640,
  0x2200,
  0xE2C1,
  0xE381,
  0x2340,
  0xE101,
  0x21C0,
  0x2080,
  0xE041,
  0xA001,
  0x60C0,
  0x6180,
  0xA141,
  0x6300,
  0xA3C1,
  0xA281,
  0x6240,
  0x6600,
  0xA6C1,
  0xA781,
  0x6740,
  0xA501,
  0x65C0,
  0x6480,
  0xA441,
  0x6C00,
  0xACC1,
  0xAD81,
  0x6D40,
  0xAF01,
  0x6FC0,
  0x6E80,
  0xAE41,
  0xAA01,
  0x6AC0,
  0x6B80,
  0xAB41,
  0x6900,
  0xA9C1,
  0xA881,
  0x6840,
  0x7800,
  0xB8C1,
  0xB981,
  0x7940,
  0xBB01,
  0x7BC0,
  0x7A80,
  0xBA41,
  0xBE01,
  0x7EC0,
  0x7F80,
  0xBF41,
  0x7D00,
  0xBDC1,
  0xBC81,
  0x7C40,
  0xB401,
  0x74C0,
  0x7580,
  0xB541,
  0x7700,
  0xB7C1,
  0xB681,
  0x7640,
  0x7200,
  0xB2C1,
  0xB381,
  0x7340,
  0xB101,
  0x71C0,
  0x7080,
  0xB041,
  0x5000,
  0x90C1,
  0x9181,
  0x5140,
  0x9301,
  0x53C0,
  0x5280,
  0x9241,
  0x9601,
  0x56C0,
  0x5780,
  0x9741,
  0x5500,
  0x95C1,
  0x9481,
  0x5440,
  0x9C01,
  0x5CC0,
  0x5D80,
  0x9D41,
  0x5F00,
  0x9FC1,
  0x9E81,
  0x5E40,
  0x5A00,
  0x9AC1,
  0x9B81,
  0x5B40,
  0x9901,
  0x59C0,
  0x5880,
  0x9841,
  0x8801,
  0x48C0,
  0x4980,
  0x8941,
  0x4B00,
  0x8BC1,
  0x8A81,
  0x4A40,
  0x4E00,
  0x8EC1,
  0x8F81,
  0x4F40,
  0x8D01,
  0x4DC0,
  0x4C80,
  0x8C41,
  0x4400,
  0x84C1,
  0x8581,
  0x4540,
  0x8701,
  0x47C0,
  0x4680,
  0x8641,
  0x8201,
  0x42C0,
  0x4380,
  0x8341,
  0x4100,
  0x81C1,
  0x8081,
  0x4040};

///////////////////////////////////////////////////////////////////////////////
// begin / end
///////////////////////////////////////////////////////////////////////////////

void Mycila::JSY::begin(HardwareSerial& serial,
                        const int8_t rxPin,
                        const int8_t txPin,
                        const BaudRate baudRate,
                        const bool async,
                        const uint8_t core,
                        const uint32_t stackSize,
                        const uint32_t pause) {
  if (_enabled)
    return;

  if (GPIO_IS_VALID_GPIO(rxPin)) {
    _pinRX = (gpio_num_t)rxPin;
  } else {
    LOGE(TAG, "Disable JSY: Invalid Serial RX (JSY TX pin): %" PRId8, rxPin);
    _pinRX = GPIO_NUM_NC;
    return;
  }

  if (GPIO_IS_VALID_OUTPUT_GPIO(txPin)) {
    _pinTX = (gpio_num_t)txPin;
  } else {
    LOGE(TAG, "Disable JSY: Invalid Serial TX (JSY RX pin): %" PRId8, txPin);
    _pinTX = GPIO_NUM_NC;
    return;
  }

  LOGI(TAG, "Enable JSY @ 0x%02X on Serial RX (JSY TX Pin): %" PRId8 " and Serial TX (JSY RX Pin): %" PRId8, _destinationAddress, rxPin, txPin);

  _pause = pause;
  _serial = &serial;

  if (baudRate == BaudRate::UNKNOWN) {
    _baudRate = _detectBauds(_destinationAddress);

    if (_baudRate == BaudRate::UNKNOWN) {
      if (_lastAddress == MYCILA_JSY_ADDRESS_UNKNOWN)
        LOGE(TAG, "Unable to read any JSY @ 0x%02X at any supported speed.", _destinationAddress);
      else
        LOGE(TAG, "Unable to read any JSY @ 0x%02X at any supported speed but found one @ 0x%02X.", _destinationAddress, _lastAddress);

      _serial->end();
      return;
    }

  } else {
    LOGW(TAG, "JSY @ 0x%02X bauds detection skipped, forcing baud rate: %" PRIu32, _destinationAddress, static_cast<uint32_t>(_baudRate));
    _openSerial(baudRate);

    _baudRate = BaudRate::UNKNOWN;
    for (int j = 0; j < MYCILA_JSY_RETRY_COUNT; j++) {
      if (_canRead(_destinationAddress, baudRate)) {
        _baudRate = baudRate;
        break;
      }
    }

    if (_baudRate == BaudRate::UNKNOWN) {
      LOGE(TAG, "Unable to read any JSY @ 0x%02X at speed: %" PRIu32, _destinationAddress, static_cast<uint32_t>(baudRate));
      _serial->end();
      return;
    }
  }

  _enabled = true;

  _model = readModel(_destinationAddress);
  LOGI(TAG, "Detected JSY-MK-%s @ 0x%02X with speed %" PRIu32 " bauds", String(_model, HEX).c_str(), _lastAddress, static_cast<uint32_t>(_baudRate));

  if (_model != MYCILA_JSY_MK_194 && _model != MYCILA_JSY_MK_163) {
    LOGE(TAG, "Unsupported JSY model: JSY-MK-%s", String(_model, HEX).c_str());
    // unsupported
    _enabled = false;
    _serial->end();
    return;
  }

  assert(!async || xTaskCreateUniversal(_jsyTask, "jsyTask", stackSize, this, MYCILA_JSY_ASYNC_PRIORITY, &_taskHandle, core) == pdPASS);
}

void Mycila::JSY::end() {
  if (_enabled) {
    LOGI(TAG, "Disable JSY @ 0x%02X", _destinationAddress);
    _enabled = false;
    while (_taskHandle != NULL) {
      // JSY takes at least 40-160 ms to finish a read
      delay(50);
    }
    _baudRate = BaudRate::UNKNOWN;
    _lastAddress = MYCILA_JSY_ADDRESS_UNKNOWN;
    _model = MYCILA_JSY_MK_UNKNOWN;
    _clear();
    _serial->end();
  }
}

///////////////////////////////////////////////////////////////////////////////
// read
///////////////////////////////////////////////////////////////////////////////

bool Mycila::JSY::read(const uint8_t address) {
  if (!_enabled)
    return false;

  if (!_mutex.try_lock_for(std::chrono::milliseconds(JSY_LOCK_TIMEOUT))) {
    LOGW(TAG, "Unable to read @ 0x%02X: Serial is busy!", address);
    return false;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] read(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_READ_REGISTERS, JSY_REQUEST_READ_REGISTERS_LEN);

  // fill the request with the registers to read
  // this depends on the model
  uint16_t registerStart = 0;
  size_t registerCount = 0;
  size_t registerSize = 0;

  switch (_model) {
    case MYCILA_JSY_MK_163:
      registerStart = JSY_163_REGISTER_VOLTAGE;
      registerCount = JSY_163_REGISTER_FREQUENCY - JSY_163_REGISTER_VOLTAGE + 1;
      registerSize = JSY_163_REGISTER_LEN;
      break;

    case MYCILA_JSY_MK_194:
      registerStart = JSY_194_REGISTER_CH1_VOLTAGE;
      registerCount = JSY_194_REGISTER_CH2_ENERGY_RETURNED - JSY_194_REGISTER_CH1_VOLTAGE + 1;
      registerSize = JSY_194_REGISTER_LEN;
      break;

    case MYCILA_JSY_MK_333:
      assert(false);
      break;

    default:
      assert(false);
      break;
  }

  _buffer[JSY_REQUEST_READ_REGISTER_ADDR_HIGH] = HIBYTE(registerStart);
  _buffer[JSY_REQUEST_READ_REGISTER_ADDR_LOW] = LOBYTE(registerStart);
  _buffer[JSY_REQUEST_READ_REGISTER_COUNT_HIGH] = HIBYTE(registerCount);
  _buffer[JSY_REQUEST_READ_REGISTER_COUNT_LOW] = LOBYTE(registerCount);

  _send(address, JSY_REQUEST_READ_REGISTERS_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_READ + registerCount * registerSize, _baudRate);

  if (result == ReadResult::READ_TIMEOUT) {
    // reset live values in case of read timeout
    _clear();
    _mutex.unlock();
    if (_callback) {
      _callback(EventType::EVT_READ_TIMEOUT);
    }
    return false;
  }

  if (result == ReadResult::READ_ERROR_COUNT || result == ReadResult::READ_ERROR_CRC) {
    // reset live values in case of read failure
    _clear();
    _mutex.unlock();
    if (_callback) {
      _callback(EventType::EVT_READ_ERROR);
    }
    return false;
  }

  if (result == ReadResult::READ_ERROR_ADDRESS) {
    // we have set a destination address, but we read another device
    _mutex.unlock();
    if (_callback) {
      _callback(EventType::EVT_READ_ERROR);
    }
    return false;
  }

  assert(result == ReadResult::READ_SUCCESS);

  float voltage1 = 0;
  float current1 = 0;
  float power1 = 0;
  float energy1 = 0;
  float powerFactor1 = 0;
  float energyReturned1 = 0;
  float frequency = 0;
  float voltage2 = 0;
  float current2 = 0;
  float power2 = 0;
  float energy2 = 0;
  float powerFactor2 = 0;
  float energyReturned2 = 0;

  switch (_model) {
    case MYCILA_JSY_MK_163:
      voltage1 = ((_buffer[3] << 8) + _buffer[4]) / 100.0;
      current1 = ((_buffer[5] << 8) + _buffer[6]) / 100.0;
      power1 = ((_buffer[7] << 8) + _buffer[8]) / (_buffer[20] == 1 ? -1.0 : 1.0);
      energy1 = ((_buffer[9] << 24) + (_buffer[10] << 16) + (_buffer[11] << 8) + _buffer[12]) / 3200.0;
      powerFactor1 = ((_buffer[13] << 8) + _buffer[14]) / 1000.0;
      energyReturned1 = ((_buffer[15] << 24) + (_buffer[16] << 16) + (_buffer[17] << 8) + _buffer[18]) / 3200.0;
      // _buffer[19] unused
      // _buffer[20] is the sign of power1
      frequency = ((_buffer[21] << 8) + _buffer[22]);
      break;

    case MYCILA_JSY_MK_194:
      voltage1 = ((_buffer[3] << 24) + (_buffer[4] << 16) + (_buffer[5] << 8) + _buffer[6]) / 10000.0;
      current1 = ((_buffer[7] << 24) + (_buffer[8] << 16) + (_buffer[9] << 8) + _buffer[10]) / 10000.0;
      power1 = ((_buffer[11] << 24) + (_buffer[12] << 16) + (_buffer[13] << 8) + _buffer[14]) / (_buffer[27] == 1 ? -10000.0 : 10000.0);
      energy1 = ((_buffer[15] << 24) + (_buffer[16] << 16) + (_buffer[17] << 8) + _buffer[18]) / 10000.0;
      powerFactor1 = ((_buffer[19] << 24) + (_buffer[20] << 16) + (_buffer[21] << 8) + _buffer[22]) / 1000.0;
      energyReturned1 = ((_buffer[23] << 24) + (_buffer[24] << 16) + (_buffer[25] << 8) + _buffer[26]) / 10000.0;
      // _buffer[27] is the sign of power1
      // _buffer[28] is the sign of power2
      // _buffer[29] unused
      // _buffer[30] unused
      frequency = ((_buffer[31] << 24) + (_buffer[32] << 16) + (_buffer[33] << 8) + _buffer[34]) / 100.0;
      voltage2 = ((_buffer[35] << 24) + (_buffer[36] << 16) + (_buffer[37] << 8) + _buffer[38]) / 10000.0;
      current2 = ((_buffer[39] << 24) + (_buffer[40] << 16) + (_buffer[41] << 8) + _buffer[42]) / 10000.0;
      power2 = ((_buffer[43] << 24) + (_buffer[44] << 16) + (_buffer[45] << 8) + _buffer[46]) / (_buffer[28] == 1 ? -10000.0 : 10000.0);
      energy2 = ((_buffer[47] << 24) + (_buffer[48] << 16) + (_buffer[49] << 8) + _buffer[50]) / 10000.0;
      powerFactor2 = ((_buffer[51] << 24) + (_buffer[52] << 16) + (_buffer[53] << 8) + _buffer[54]) / 1000.0;
      energyReturned2 = ((_buffer[55] << 24) + (_buffer[56] << 16) + (_buffer[57] << 8) + _buffer[58]) / 10000.0;
      break;

    case MYCILA_JSY_MK_333:
      assert(false);
      break;

    default:
      assert(false);
      break;
  }

  bool changed = _buffer[JSY_RESPONSE_ADDRESS] != _data.address ||
                 voltage1 != _data.voltage1 ||
                 current1 != _data.current1 ||
                 power1 != _data.power1 ||
                 energy1 != _data.energy1 ||
                 powerFactor1 != _data.powerFactor1 ||
                 energyReturned1 != _data.energyReturned1 ||
                 frequency != _data.frequency ||
                 voltage2 != _data.voltage2 ||
                 current2 != _data.current2 ||
                 power2 != _data.power2 ||
                 energy2 != _data.energy2 ||
                 powerFactor2 != _data.powerFactor2 ||
                 energyReturned2 != _data.energyReturned2;

  if (changed) {
    _data.address = _buffer[JSY_RESPONSE_ADDRESS];
    _data.voltage1 = voltage1;
    _data.current1 = current1;
    _data.power1 = power1;
    _data.energy1 = energy1;
    _data.powerFactor1 = powerFactor1;
    _data.energyReturned1 = energyReturned1;
    _data.frequency = frequency;
    _data.voltage2 = voltage2;
    _data.current2 = current2;
    _data.power2 = power2;
    _data.energy2 = energy2;
    _data.powerFactor2 = powerFactor2;
    _data.energyReturned2 = energyReturned2;
  }

  _lastReadSuccess = millis();

  _mutex.unlock();

  if (_callback) {
    _callback(EventType::EVT_READ);
    if (changed) {
      _callback(EventType::EVT_CHANGE);
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// readModel
///////////////////////////////////////////////////////////////////////////////

uint16_t Mycila::JSY::readModel(const uint8_t address) {
  if (!_enabled)
    return MYCILA_JSY_MK_UNKNOWN;

  LOGD(TAG, "readModel(0x%02X)", address);

  if (!_mutex.try_lock_for(std::chrono::milliseconds(JSY_LOCK_TIMEOUT))) {
    LOGW(TAG, "Unable to read model @ 0x%02X: Serial is busy!", address);
    return MYCILA_JSY_MK_UNKNOWN;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] readModel(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_READ_MODEL, JSY_REQUEST_READ_MODEL_LEN);
  _send(address, JSY_REQUEST_READ_MODEL_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_READ_MODEL, _baudRate);

  if (result != ReadResult::READ_SUCCESS) {
    _mutex.unlock();
    return MYCILA_JSY_MK_UNKNOWN;
  }

  uint16_t model = (_buffer[JSY_RESPONSE_DATA] << 8) + _buffer[JSY_RESPONSE_DATA + 1];

  _mutex.unlock();

  return model;
}

///////////////////////////////////////////////////////////////////////////////
// resetEnergy
///////////////////////////////////////////////////////////////////////////////

bool Mycila::JSY::resetEnergy(const uint8_t address) {
  if (!_enabled)
    return false;

  LOGD(TAG, "resetEnergy(0x%02X)", address);

  if (!_mutex.try_lock_for(std::chrono::milliseconds(JSY_LOCK_TIMEOUT))) {
    LOGW(TAG, "Unable to reset @ 0x%02X: Serial is busy!", address);
    return false;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] resetEnergy(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_RESET_ENERGY, JSY_REQUEST_RESET_ENERGY_LEN);
  _send(address, JSY_REQUEST_RESET_ENERGY_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_RESET_ENERGY, _baudRate);

  _mutex.unlock();

  return result == ReadResult::READ_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// setBaudRate
///////////////////////////////////////////////////////////////////////////////

bool Mycila::JSY::setBaudRate(const uint8_t address, const BaudRate baudRate) {
  return _set(address, address ? address : (_lastAddress ? _lastAddress : MYCILA_JSY_ADDRESS_DEFAULT), baudRate);
}

bool Mycila::JSY::setDeviceAddress(const uint8_t address, const uint8_t newAddress) {
  return _set(address, newAddress, _baudRate);
}

bool Mycila::JSY::_set(const uint8_t address, const uint8_t newAddress, const BaudRate newBaudRate) {
  if (!_enabled)
    return false;

  if (newBaudRate == BaudRate::UNKNOWN)
    return false;

  if (newAddress == MYCILA_JSY_ADDRESS_UNKNOWN)
    return false;

  LOGD(TAG, "set(0x%02X) address=0x%02X, bauds=%" PRIu32, address, newAddress, static_cast<uint32_t>(newBaudRate));

  if (!_mutex.try_lock_for(std::chrono::milliseconds(JSY_LOCK_TIMEOUT))) {
    LOGW(TAG, "Unable to set @ 0x%02X: Serial is busy!", address);
    return false;
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] _set(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_SET_COM, JSY_REQUEST_SET_COM_LEN);

  // set address
  _buffer[JSY_REQUEST_SET_ADDRESS] = newAddress;

  // set baud rate ID
  switch (newBaudRate) {
    case BaudRate::BAUD_1200:
      _buffer[JSY_REQUEST_SET_BAUDS] = 0x03;
      break;
    case BaudRate::BAUD_2400:
      _buffer[JSY_REQUEST_SET_BAUDS] = 0x04;
      break;
    case BaudRate::BAUD_4800:
      _buffer[JSY_REQUEST_SET_BAUDS] = 0x05;
      break;
    case BaudRate::BAUD_9600:
      _buffer[JSY_REQUEST_SET_BAUDS] = 0x06;
      break;
    case BaudRate::BAUD_19200:
      _buffer[JSY_REQUEST_SET_BAUDS] = 0x07;
      break;
    case BaudRate::BAUD_38400:
      _buffer[JSY_REQUEST_SET_BAUDS] = 0x08;
      break;
    default:
      assert(false);
      break;
  }

  _send(address, JSY_REQUEST_SET_COM_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_SET_COM, _baudRate);

  // unexpected error ?
  if (result != ReadResult::READ_SUCCESS && result != ReadResult::READ_ERROR_ADDRESS) {
    _mutex.unlock();
    return false;
  }

  // response from unexpected address ?
  if (result == ReadResult::READ_ERROR_ADDRESS && _lastAddress != newAddress) {
    _mutex.unlock();
    return false;
  }

  _openSerial(newBaudRate);

  bool success = false;
  for (int i = 0; i < MYCILA_JSY_RETRY_COUNT; i++) {
    if (_canRead(address, newBaudRate)) {
      success = true;
      break;
    }
  }

  if (success) {
    // update baud rate
    _baudRate = newBaudRate;

    // update destination address if needed
    if (_destinationAddress != MYCILA_JSY_ADDRESS_BROADCAST && _destinationAddress == address) {
      _destinationAddress = newAddress;
    }

  } else {
    LOGE(TAG, "Unable to read JSY @ 0x%02X at speed: %" PRIu32, address, static_cast<uint32_t>(newBaudRate));
    if (_baudRate != BaudRate::UNKNOWN) {
      _openSerial(_baudRate);
    }
  }

  _mutex.unlock();

  return success;
}

///////////////////////////////////////////////////////////////////////////////
// toJson
///////////////////////////////////////////////////////////////////////////////

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::toJson(const JsonObject& root) const {
  root["enabled"] = _enabled;
  root["address"] = _data.address;
  root["speed"] = static_cast<uint32_t>(_baudRate);
  root["model"] = getModelName();
  root["time"] = _lastReadSuccess;
  root["current1"] = _data.current1;
  root["current2"] = _data.current2;
  root["energy_returned1"] = _data.energyReturned1;
  root["energy_returned2"] = _data.energyReturned2;
  root["energy1"] = _data.energy1;
  root["energy2"] = _data.energy2;
  root["frequency"] = _data.frequency;
  root["power_factor1"] = _data.powerFactor1;
  root["power_factor2"] = _data.powerFactor2;
  root["power1"] = _data.power1;
  root["power2"] = _data.power2;
  root["voltage1"] = _data.voltage1;
  root["voltage2"] = _data.voltage2;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// I/O
///////////////////////////////////////////////////////////////////////////////

bool Mycila::JSY::_canRead(const uint8_t address, BaudRate baudRate) {
#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] _canRead(0x%02X)\n", address);
#endif
  memcpy(_buffer, JSY_REQUEST_READ_MODEL, JSY_REQUEST_READ_MODEL_LEN);
  _send(address, JSY_REQUEST_READ_MODEL_LEN);
  return _timedRead(address, JSY_RESPONSE_SIZE_READ_MODEL, baudRate) == ReadResult::READ_SUCCESS;
}

Mycila::JSY::ReadResult Mycila::JSY::_timedRead(const uint8_t expectedAddress, const size_t expectedLen, const BaudRate baudRate) {
  size_t count = 0;
  while (count < expectedLen) {
    size_t read = _serial->readBytes(_buffer + count, expectedLen - count);
    if (read) {
      count += read;
    } else {
      break;
    }
  }

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] timedRead(0x%02X) %d < ", expectedAddress, count);
  for (size_t i = 0; i < count; i++) {
    Serial.printf("0x%02X ", _buffer[i]);
  }
  Serial.println();
#endif

  _drop();

  // timeout ?
  if (count == 0) {
    LOGD(TAG, "timedRead(0x%02X) timeout", expectedAddress);
    return ReadResult::READ_TIMEOUT;
  }

  // check length
  if (count != expectedLen) {
    LOGD(TAG, "timedRead(0x%02X) error: len %d != %d", expectedAddress, count, expectedLen);
    return ReadResult::READ_ERROR_COUNT;
  }

  // CRC check
  uint16_t crc = _crc16(_buffer, expectedLen - 2);
  if (_buffer[expectedLen - 2] != LOBYTE(crc) || _buffer[expectedLen - 1] != HIBYTE(crc)) {
    LOGD(TAG, "timedRead(0x%02X) error: bad CRC 0x%02X 0x%02X != 0x%02X 0x%02X", expectedAddress, _buffer[expectedLen - 2], _buffer[expectedLen - 1], LOBYTE(crc), HIBYTE(crc));
    return ReadResult::READ_ERROR_CRC;
  }

  _lastAddress = _buffer[JSY_RESPONSE_ADDRESS];

  // address check
  if (expectedAddress != MYCILA_JSY_ADDRESS_BROADCAST && expectedAddress != _lastAddress) {
    LOGD(TAG, "timedRead(0x%02X) error: wrong device address 0x%02X", expectedAddress, _lastAddress);
    return ReadResult::READ_ERROR_ADDRESS;
  }

  return ReadResult::READ_SUCCESS;
}

void Mycila::JSY::_send(const uint8_t address, const size_t len) {
  // set destination address
  _buffer[JSY_REQUEST_ADDRESS] = address;

  // crc16
  uint16_t crc = _crc16(_buffer, len - 2);
  _buffer[len - 2] = LOBYTE(crc);
  _buffer[len - 1] = HIBYTE(crc);

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] send(0x%02X) %d > ", address, len);
  for (size_t i = 0; i < len; i++) {
    Serial.printf("0x%02X ", _buffer[i]);
  }
  Serial.println();
#endif

  _serial->flush(false);
  _serial->write(_buffer, len);
}

size_t Mycila::JSY::_drop() {
  size_t count = 0;
  if (_serial->available()) {
#ifdef MYCILA_JSY_DEBUG
    Serial.printf("[JSY] drop < ");
#endif
    while (_serial->available()) {
#ifdef MYCILA_JSY_DEBUG
      Serial.printf("0x%02X ", _serial->read());
#else
      _serial->read();
#endif
      count++;
    }
#ifdef MYCILA_JSY_DEBUG
    Serial.println();
#endif
  }
  return count;
}

void Mycila::JSY::_openSerial(BaudRate baudRate) {
  LOGD(TAG, "openSerial(%" PRIu32 ")", static_cast<uint32_t>(baudRate));
  _serial->begin(static_cast<uint32_t>(baudRate), SERIAL_8N1, _pinRX, _pinTX);
  _serial->setTimeout(200);
  while (!_serial)
    yield();
  while (!_serial->availableForWrite())
    yield();
  _drop();
  _serial->flush(false);
}

Mycila::JSY::BaudRate Mycila::JSY::_detectBauds(const uint8_t address) {
  for (int i = 0; i < BAUD_RATES_COUNT * 2; i++) {
    BaudRate baudRate = BAUD_RATES[i % BAUD_RATES_COUNT];
    LOGD(TAG, "find(0x%02X) %" PRIu32 " bauds", address, static_cast<uint32_t>(baudRate));
    _openSerial(baudRate);
    for (int j = 0; j < MYCILA_JSY_RETRY_COUNT; j++) {
      if (_canRead(address, baudRate)) {
        return baudRate;
      }
    }
  }
  return BaudRate::UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////
// Utils
///////////////////////////////////////////////////////////////////////////////

// For CRC: https://crccalc.com
// Select CRC-16/MODBUS
uint16_t Mycila::JSY::_crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    uint8_t temp = *(data++) ^ LOBYTE(crc);
    crc = (crc >> 8) ^ pgm_read_word_near(CRCTable + temp);
  }
  return crc;
}

void Mycila::JSY::_clear() {
  _data.address = MYCILA_JSY_ADDRESS_UNKNOWN;
  _data.current1 = 0;
  _data.current2 = 0;
  _data.frequency = 0;
  _data.power1 = 0;
  _data.power2 = 0;
  _data.powerFactor1 = 0;
  _data.powerFactor2 = 0;
  _data.voltage1 = 0;
  _data.voltage2 = 0;
}

///////////////////////////////////////////////////////////////////////////////
// Async
///////////////////////////////////////////////////////////////////////////////

void Mycila::JSY::_jsyTask(void* params) {
  JSY* jsy = reinterpret_cast<JSY*>(params);
  while (jsy->_enabled) {
    if (jsy->read()) {
      if (jsy->_pause > 0) {
        delay(jsy->_pause);
      } else {
        yield();
      }
    } else if (jsy->_pause > 0) {
      delay(jsy->_pause);
    } else {
      delay(10);
    }
  }
  jsy->_taskHandle = NULL;
  vTaskDelete(NULL);
}
