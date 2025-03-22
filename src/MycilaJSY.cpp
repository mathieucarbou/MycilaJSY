// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include "MycilaJSY.h"

#include <algorithm>

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
#define JSY_REGISTER_MODEL2        0x0001 // RO - JSY Mode and Version
#define JSY_REGISTER_VOLTAGE_RANGE 0x0002 // RO - Example: 250V
#define JSY_REGISTER_CURRENT_RANGE 0x0003 // RO - Example: 800 (800 / 10 == 80A)

// communication
#define JSY_REGISTER_ID_AND_BAUDS 0x0004 // RW - ID (high byte) and Bauds Rate (low byte)
#define JSY_REGISTER_SWITCH_MODE  0x0005 // RW - Switch Mode (AC/DC)
#define JSY_MODE_AC               0x01   // AC Mode
#define JSY_MODE_DC               0x02   // DC Mode

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-1031 REGISTERS
///////////////////////////////////////////////////////////////////////////////

#define JSY_1031_REGISTER_VOLTAGE        0x0048 // RO
#define JSY_1031_REGISTER_CURRENT        0x0049 // RO, + 0x004A
#define JSY_1031_REGISTER_ACTIVE_POWER   0x004B // RO, + 0x004C
#define JSY_1031_REGISTER_ACTIVE_ENERGY  0x004D // RO, + 0x004E
#define JSY_1031_REGISTER_POWER_FACTOR   0x004F // RO
#define JSY_1031_REGISTER_FREQUENCY      0x0050 // RO
#define JSY_1031_REGISTER_CO2            0x0051 // RO, + 0x0052
#define JSY_1031_REGISTER_RESERVED       0x0053 // RO, + 0x0054, + 0x0055, + 0x0056
#define JSY_1031_REGISTER_APPARENT_POWER 0x0057 // RO. + 0x0058
#define JSY_1031_REGISTER_REACTIVE_POWER 0x0059 // RO, + 0x005A
#define JSY_1031_REGISTER_PHASE_ANGLE    0x005B // RO

#define JSY_1031_REGISTER_LEN   2  // 2 bytes per register
#define JSY_1031_REGISTER_COUNT 19 // 19 registers
#define JSY_1031_REGISTER_START JSY_1031_REGISTER_VOLTAGE

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-163 REGISTERS
///////////////////////////////////////////////////////////////////////////////

#define JSY_163_REGISTER_VOLTAGE                0x0048 // RO
#define JSY_163_REGISTER_CURRENT                0x0049 // RO
#define JSY_163_REGISTER_ACTIVE_POWER           0x004A // RO
#define JSY_163_REGISTER_ACTIVE_ENERGY_IMPORTED 0x004B // RO, + 0x004C
#define JSY_163_REGISTER_POWER_FACTOR           0x004D // RO
#define JSY_163_REGISTER_ACTIVE_ENERGY_RETURNED 0x004E // RO, + 0x004F
#define JSY_163_REGISTER_ACTIVE_POWER_SIGN      0x0050 // RO
#define JSY_163_REGISTER_FREQUENCY              0x0051 // RO

#define JSY_163_REGISTER_LEN   2  // 2 bytes per register
#define JSY_163_REGISTER_COUNT 10 // 10 registers
#define JSY_163_REGISTER_START JSY_163_REGISTER_VOLTAGE

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-193 REGISTERS
///////////////////////////////////////////////////////////////////////////////

#define JSY_193_REGISTER_CH1_VOLTAGE                0x0100 // RO
#define JSY_193_REGISTER_CH1_CURRENT                0x0101 // RO
#define JSY_193_REGISTER_CH1_ACTIVE_POWER           0x0102 // RO
#define JSY_193_REGISTER_CH1_ACTIVE_POWER_SIGN      0x0103 // RO
#define JSY_193_REGISTER_CH1_ACTIVE_ENERGY_POSITIVE 0x0104 // RO, + 0x0105
#define JSY_193_REGISTER_CH1_ACTIVE_ENERGY_NEGATIVE 0x0106 // RO, + 0x0107
#define JSY_193_REGISTER_CH1_POWER_FACTOR           0x0108 // RO
#define JSY_193_REGISTER_CH1_FREQUENCY              0x0109 // RO
#define JSY_193_REGISTER_CH2_VOLTAGE                0x010A // RO
#define JSY_193_REGISTER_CH2_CURRENT                0x010B // RO
#define JSY_193_REGISTER_CH2_ACTIVE_POWER           0x010C // RO
#define JSY_193_REGISTER_CH2_ACTIVE_POWER_SIGN      0x010D // RO
#define JSY_193_REGISTER_CH2_ACTIVE_ENERGY_POSITIVE 0x010E // RO, + 0x010F
#define JSY_193_REGISTER_CH2_ACTIVE_ENERGY_NEGATIVE 0x0110 // RO, + 0x0111
#define JSY_193_REGISTER_CH2_POWER_FACTOR           0x0112 // RO
#define JSY_193_REGISTER_CH2_FREQUENCY              0x0113 // RO

#define JSY_193_REGISTER_LEN   2  // 2 bytes per register
#define JSY_193_REGISTER_COUNT 20 // 20 registers
#define JSY_193_REGISTER_START JSY_193_REGISTER_CH1_VOLTAGE

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-194 REGISTERS
///////////////////////////////////////////////////////////////////////////////

#define JSY_194_REGISTER_CH1_VOLTAGE                0x0048 // RO
#define JSY_194_REGISTER_CH1_CURRENT                0x0049 // RO
#define JSY_194_REGISTER_CH1_ACTIVE_POWER           0x004A // RO
#define JSY_194_REGISTER_CH1_ACTIVE_ENERGY_IMPORTED 0x004B // RO
#define JSY_194_REGISTER_CH1_POWER_FACTOR           0x004C // RO
#define JSY_194_REGISTER_CH1_ACTIVE_ENERGY_RETURNED 0x004D // RO
#define JSY_194_REGISTER_ACTIVE_POWER_SIGNS         0x004E // RO
#define JSY_194_REGISTER_FREQUENCY                  0x004F // RO
#define JSY_194_REGISTER_CH2_VOLTAGE                0x0050 // RO
#define JSY_194_REGISTER_CH2_CURRENT                0x0051 // RO
#define JSY_194_REGISTER_CH2_ACTIVE_POWER           0x0052 // RO
#define JSY_194_REGISTER_CH2_ACTIVE_ENERGY_IMPORTED 0x0053 // RO
#define JSY_194_REGISTER_CH2_POWER_FACTOR           0x0054 // RO
#define JSY_194_REGISTER_CH2_ACTIVE_ENERGY_RETURNED 0x0055 // RO

#define JSY_194_REGISTER_LEN   4  // 4 bytes per register
#define JSY_194_REGISTER_COUNT 14 // 14 registers
#define JSY_194_REGISTER_START JSY_194_REGISTER_CH1_VOLTAGE

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-22x REGISTERS (227, 229)
///////////////////////////////////////////////////////////////////////////////

#define JSY_22x_REGISTER_VOLTAGE                  0x0100 // RO, + 0x0101
#define JSY_22x_REGISTER_CURRENT                  0x0102 // RO, + 0x0103
#define JSY_22x_REGISTER_ACTIVE_POWER             0x0104 // RO, + 0x0105
#define JSY_22x_REGISTER_REACTIVE_POWER           0x0106 // RO, + 0x0107
#define JSY_22x_REGISTER_APPARENT_POWER           0x0108 // RO, + 0x0109
#define JSY_22x_REGISTER_POWER_FACTOR             0x010A // RO, + 0x010B
#define JSY_22x_REGISTER_FREQUENCY                0x010C // RO, + 0x010D
#define JSY_22x_REGISTER_ACTIVE_ENERGY            0x010E // RO, + 0x010F
#define JSY_22x_REGISTER_REACTIVE_ENERGY          0x0110 // RO, + 0x0111
#define JSY_22x_REGISTER_POWER_SUPPLY             0x0112 // RO, + 0x0113
#define JSY_22x_REGISTER_ACTIVE_POWER_SIGN        0x0114 // RO
#define JSY_22x_REGISTER_REACTIVE_POWER_SIGN      0x0115 // RO
#define JSY_22x_REGISTER_ACTIVE_ENERGY_POSITIVE   0x0116 // RO, + 0x0117
#define JSY_22x_REGISTER_ACTIVE_ENERGY_NEGATIVE   0x0118 // RO, + 0x0119
#define JSY_22x_REGISTER_REACTIVE_ENERGY_POSITIVE 0x011A // RO, + 0x011B
#define JSY_22x_REGISTER_REACTIVE_ENERGY_NEGATIVE 0x011C // RO, + 0x011D

#define JSY_22x_REGISTER_LEN   2  // 2 bytes per register
#define JSY_22x_REGISTER_COUNT 30 // 20 registers
#define JSY_22x_REGISTER_START JSY_22x_REGISTER_VOLTAGE

///////////////////////////////////////////////////////////////////////////////
// JSY-MK-333 REGISTERS
///////////////////////////////////////////////////////////////////////////////

#define JSY_333_REGISTER_PHASE_A_VOLTAGE                  0x0100 // RO
#define JSY_333_REGISTER_PHASE_B_VOLTAGE                  0x0101 // RO
#define JSY_333_REGISTER_PHASE_C_VOLTAGE                  0x0102 // RO
#define JSY_333_REGISTER_PHASE_A_CURRENT                  0x0103 // RO
#define JSY_333_REGISTER_PHASE_B_CURRENT                  0x0104 // RO
#define JSY_333_REGISTER_PHASE_C_CURRENT                  0x0105 // RO
#define JSY_333_REGISTER_PHASE_A_ACTIVE_POWER             0x0106 // RO
#define JSY_333_REGISTER_PHASE_B_ACTIVE_POWER             0x0107 // RO
#define JSY_333_REGISTER_PHASE_C_ACTIVE_POWER             0x0108 // RO
#define JSY_333_REGISTER_TOTAL_ACTIVE_POWER               0x0109 // RO + 0x010A
#define JSY_333_REGISTER_PHASE_A_REACTIVE_POWER           0x010B // RO
#define JSY_333_REGISTER_PHASE_B_REACTIVE_POWER           0x010C // RO
#define JSY_333_REGISTER_PHASE_C_REACTIVE_POWER           0x010D // RO
#define JSY_333_REGISTER_TOTAL_REACTIVE_POWER             0x010E // RO + 0x010F
#define JSY_333_REGISTER_PHASE_A_APPARENT_POWER           0x0110 // RO
#define JSY_333_REGISTER_PHASE_B_APPARENT_POWER           0x0111 // RO
#define JSY_333_REGISTER_PHASE_C_APPARENT_POWER           0x0112 // RO
#define JSY_333_REGISTER_TOTAL_APPARENT_POWER             0x0113 // RO + 0x0114
#define JSY_333_REGISTER_FREQUENCY                        0x0115 // RO
#define JSY_333_REGISTER_PHASE_A_POWER_FACTOR             0x0116 // RO
#define JSY_333_REGISTER_PHASE_B_POWER_FACTOR             0x0117 // RO
#define JSY_333_REGISTER_PHASE_C_POWER_FACTOR             0x0118 // RO
#define JSY_333_REGISTER_TOTAL_POWER_FACTOR               0x0119 // RO
#define JSY_333_REGISTER_PHASE_A_ACTIVE_ENERGY            0x011A // RO + 0x011B
#define JSY_333_REGISTER_PHASE_B_ACTIVE_ENERGY            0x011C // RO + 0x011D
#define JSY_333_REGISTER_PHASE_C_ACTIVE_ENERGY            0x011E // RO + 0x011F
#define JSY_333_REGISTER_TOTAL_ACTIVE_ENERGY              0x0120 // RO + 0x0121
#define JSY_333_REGISTER_PHASE_A_REACTIVE_ENERGY          0x0122 // RO + 0x0123
#define JSY_333_REGISTER_PHASE_B_REACTIVE_ENERGY          0x0124 // RO + 0x0125
#define JSY_333_REGISTER_PHASE_C_REACTIVE_ENERGY          0x0126 // RO + 0x0127
#define JSY_333_REGISTER_TOTAL_REACTIVE_ENERGY            0x0128 // RO + 0x0129
#define JSY_333_REGISTER_PHASE_A_APPARENT_ENERGY          0x012A // RO + 0x012B
#define JSY_333_REGISTER_PHASE_B_APPARENT_ENERGY          0x012C // RO + 0x012D
#define JSY_333_REGISTER_PHASE_C_APPARENT_ENERGY          0x012E // RO + 0x012F
#define JSY_333_REGISTER_TOTAL_APPARENT_ENERGY            0x0130 // RO + 0x0131
#define JSY_333_REGISTER_POWER_SIGNS                      0x0132 // RO
#define JSY_333_REGISTER_ALARMS                           0x0133 // RO
#define JSY_333_REGISTER_PHASE_A_ACTIVE_ENERGY_IMPORTED   0x0134 // RO + 0x0135
#define JSY_333_REGISTER_PHASE_B_ACTIVE_ENERGY_IMPORTED   0x0136 // RO + 0x0137
#define JSY_333_REGISTER_PHASE_C_ACTIVE_ENERGY_IMPORTED   0x0138 // RO + 0x0139
#define JSY_333_REGISTER_TOTAL_ACTIVE_ENERGY_IMPORTED     0x013A // RO + 0x013B
#define JSY_333_REGISTER_PHASE_A_ACTIVE_ENERGY_RETURNED   0x013C // RO + 0x013D
#define JSY_333_REGISTER_PHASE_B_ACTIVE_ENERGY_RETURNED   0x013E // RO + 0x013F
#define JSY_333_REGISTER_PHASE_C_ACTIVE_ENERGY_RETURNED   0x0140 // RO + 0x0141
#define JSY_333_REGISTER_TOTAL_ACTIVE_ENERGY_RETURNED     0x0142 // RO + 0x0143
#define JSY_333_REGISTER_PHASE_A_REACTIVE_ENERGY_IMPORTED 0x0144 // RO + 0x0145
#define JSY_333_REGISTER_PHASE_B_REACTIVE_ENERGY_IMPORTED 0x0146 // RO + 0x0147
#define JSY_333_REGISTER_PHASE_C_REACTIVE_ENERGY_IMPORTED 0x0148 // RO + 0x0149
#define JSY_333_REGISTER_TOTAL_REACTIVE_ENERGY_IMPORTED   0x014A // RO + 0x014B
#define JSY_333_REGISTER_PHASE_A_REACTIVE_ENERGY_RETURNED 0x014C // RO + 0x014D
#define JSY_333_REGISTER_PHASE_B_REACTIVE_ENERGY_RETURNED 0x014E // RO + 0x014F
#define JSY_333_REGISTER_PHASE_C_REACTIVE_ENERGY_RETURNED 0x0150 // RO + 0x0151
#define JSY_333_REGISTER_TOTAL_REACTIVE_ENERGY_RETURNED   0x0152 // RO + 0x0153
#define JSY_333_REGISTER_PHASE_A_L_U                      0x0154 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_B_L_U                      0x0155 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_C_L_U                      0x0156 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_A_PHASE_ANGLE_U            0x0157 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_B_PHASE_ANGLE_U            0x0158 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_C_PHASE_ANGLE_U            0x0159 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_A_PHASE_ANGLE_I            0x015A // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_B_PHASE_ANGLE_I            0x015B // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_C_PHASE_ANGLE_I            0x015C // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_A_PHASE_ANGLE_UI           0x015D // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_B_PHASE_ANGLE_UI           0x015E // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_C_PHASE_ANGLE_UI           0x015F // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_A_THD_U                    0x0160 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_B_THD_U                    0x0161 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_C_THD_U                    0x0162 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_A_THD_I                    0x0163 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_B_THD_I                    0x0164 // RO -- WARNING: UNDOCUMENTED
#define JSY_333_REGISTER_PHASE_C_THD_I                    0x0165 // RO -- WARNING: UNDOCUMENTED

#define JSY_333_REGISTER_LEN   2   // 2 bytes per register
#define JSY_333_REGISTER_COUNT 102 // registers
#define JSY_333_REGISTER_START JSY_333_REGISTER_PHASE_A_VOLTAGE

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
#define JSY_REQUEST_SET_MODE                 8

// response indexes
#define JSY_RESPONSE_ADDRESS  0
#define JSY_RESPONSE_CMD      1
#define JSY_RESPONSE_DATA_LEN 2
#define JSY_RESPONSE_DATA     3

// response
#define JSY_RESPONSE_SIZE_READ         5                          // address(1), cmd(1), len(1), data(?), crc(2)
#define JSY_RESPONSE_SIZE_READ_MODEL   JSY_RESPONSE_SIZE_READ + 2 // address(1), cmd(1), len(1), data(2), crc(2)
#define JSY_RESPONSE_SIZE_READ_MODE    JSY_RESPONSE_SIZE_READ_MODEL
#define JSY_RESPONSE_SIZE_RESET_ENERGY 8 // address(1), cmd(1), len(1), register(2), count(2), crc(2)
#define JSY_RESPONSE_SIZE_SWITCH_MODE  8 // address(1), cmd(1), register(2), data(2), crc(2)
#define JSY_RESPONSE_SIZE_SET_COM      8 // address(1), cmd(1), data(4), crc(2)

static constexpr uint8_t JSY_REQUEST_READ_REGISTERS[] = {
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

static constexpr uint8_t JSY_REQUEST_READ_MODEL[] = {
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

static constexpr uint8_t JSY_REQUEST_READ_MODE[] = {
  MYCILA_JSY_ADDRESS_BROADCAST,
  JSY_CMD_READ_REGISTERS,
  HIBYTE(JSY_REGISTER_MODEL2),
  LOBYTE(JSY_REGISTER_MODEL2),
  0x00, // number of registers to read (high byte)
  0x01, // number of registers to read (low byte)
  0x00, // CRC (low)
  0x00  // CRC (high)
};
static constexpr size_t JSY_REQUEST_READ_MODE_LEN = sizeof(JSY_REQUEST_READ_MODE);

static constexpr uint8_t JSY_REQUEST_RESET_ENERGY[] = {
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

static constexpr uint8_t JSY_REQUEST_SET_COM[] = {
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

static constexpr uint8_t JSY_REQUEST_SWITCH_MODE[] = {
  MYCILA_JSY_ADDRESS_BROADCAST,
  JSY_CMD_WRITE_REGISTERS,
  HIBYTE(JSY_REGISTER_SWITCH_MODE),
  LOBYTE(JSY_REGISTER_SWITCH_MODE),
  0x00, // number of registers to write (high byte)
  0x01, // number of registers to write (low byte)
  0x02, // number of bytes to follow
  0x00, // switch signal (high byte)
  0x00, // switch signal (low byte)
  0x00, // CRC (low)
  0x00  // CRC (high)
};
static constexpr size_t JSY_REQUEST_SWITCH_MODE_LEN = sizeof(JSY_REQUEST_SWITCH_MODE);

///////////////////////////////////////////////////////////////////////////////
// Bauds
///////////////////////////////////////////////////////////////////////////////

static constexpr Mycila::JSY::BaudRate AUTO_DETECT_BAUD_RATES[] = {
  Mycila::JSY::BaudRate::BAUD_4800,  // default value for some JSY
  Mycila::JSY::BaudRate::BAUD_9600,  // default value for some JSY
  Mycila::JSY::BaudRate::BAUD_19200, // supported speed for some JSY
  Mycila::JSY::BaudRate::BAUD_38400, // supported speed for some JSY
  Mycila::JSY::BaudRate::BAUD_1200,  // supported speed for some JSY but slower - will probably never be used
  Mycila::JSY::BaudRate::BAUD_2400,  // supported speed for some JSY but slower - will probably never be used
};
static constexpr size_t AUTO_DETECT_BAUD_RATES_COUNT = 6;

///////////////////////////////////////////////////////////////////////////////
// CRC16 Table
///////////////////////////////////////////////////////////////////////////////

static constexpr uint16_t CRCTable[] = {
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
                        const uint8_t destinationAddress,
                        const uint16_t model,
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

  LOGI(TAG, "Enable JSY @ 0x%02X on Serial RX (JSY TX Pin): %" PRId8 " and Serial TX (JSY RX Pin): %" PRId8, destinationAddress, rxPin, txPin);

  _pause = pause;
  _serial = &serial;

  if (baudRate == BaudRate::UNKNOWN) {
    _baudRate = _detectBauds(destinationAddress);

    if (_baudRate == BaudRate::UNKNOWN) {
      if (_lastAddress == MYCILA_JSY_ADDRESS_UNKNOWN)
        LOGE(TAG, "Unable to read any JSY @ 0x%02X at any supported speed.", destinationAddress);
      else
        LOGE(TAG, "Unable to read any JSY @ 0x%02X at any supported speed but found one @ 0x%02X.", destinationAddress, _lastAddress);

      _serial->end();
      return;
    }

  } else {
    LOGW(TAG, "JSY @ 0x%02X bauds detection skipped, forcing baud rate: %" PRIu32, destinationAddress, _baudRate);
    _openSerial(baudRate);

    _baudRate = BaudRate::UNKNOWN;
    for (int j = 0; j < MYCILA_JSY_RETRY_COUNT; j++) {
      if (_canRead(destinationAddress, baudRate)) {
        _baudRate = baudRate;
        break;
      }
    }

    if (_baudRate == BaudRate::UNKNOWN) {
      LOGE(TAG, "Unable to read any JSY @ 0x%02X at speed: %" PRIu32, destinationAddress, baudRate);
      _serial->end();
      return;
    }
  }

  _enabled = true;
  _model = model ? model : readModel(destinationAddress);

  if (_model != MYCILA_JSY_MK_1031 &&
      _model != MYCILA_JSY_MK_163 &&
      _model != MYCILA_JSY_MK_193 &&
      _model != MYCILA_JSY_MK_194 &&
      _model != MYCILA_JSY_MK_227 &&
      _model != MYCILA_JSY_MK_229 &&
      _model != MYCILA_JSY_MK_333) {
    LOGE(TAG, "Unsupported JSY model: JSY-MK-%X", _model);
    // unsupported
    _enabled = false;
    _serial->end();
    return;
  }

  _destinationAddress = destinationAddress;
  LOGI(TAG, "Detected JSY-MK-%X @ 0x%02X with speed %" PRIu32 " bauds", _model, _lastAddress, _baudRate);

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
    _data.clear();
    _serial->end();
  }
}

///////////////////////////////////////////////////////////////////////////////
// read
///////////////////////////////////////////////////////////////////////////////

bool Mycila::JSY::_read(const uint8_t address, uint16_t model) {
  if (!_enabled)
    return false;

  std::lock_guard<std::mutex> lock(_mutex);

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] read(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_READ_REGISTERS, JSY_REQUEST_READ_REGISTERS_LEN);

  // fill the request with the registers to read
  // this depends on the model
  uint16_t registerStart = 0;
  uint16_t registerCount = 0;
  uint8_t registerSize = 0;

  switch (model) {
    case MYCILA_JSY_MK_1031:
      registerSize = JSY_1031_REGISTER_LEN;
      registerStart = JSY_1031_REGISTER_START;
      registerCount = JSY_1031_REGISTER_COUNT;
      break;

    case MYCILA_JSY_MK_163:
      registerSize = JSY_163_REGISTER_LEN;
      registerStart = JSY_163_REGISTER_START;
      registerCount = JSY_163_REGISTER_COUNT;
      break;

    case MYCILA_JSY_MK_193:
      registerSize = JSY_193_REGISTER_LEN;
      registerStart = JSY_193_REGISTER_START;
      registerCount = JSY_193_REGISTER_COUNT;
      break;

    case MYCILA_JSY_MK_194:
      registerSize = JSY_194_REGISTER_LEN;
      registerStart = JSY_194_REGISTER_START;
      registerCount = JSY_194_REGISTER_COUNT;
      break;

    case MYCILA_JSY_MK_227:
    case MYCILA_JSY_MK_229:
      registerSize = JSY_22x_REGISTER_LEN;
      registerStart = JSY_22x_REGISTER_START;
      registerCount = JSY_22x_REGISTER_COUNT;
      break;

    case MYCILA_JSY_MK_333:
      registerSize = JSY_333_REGISTER_LEN;
      registerStart = JSY_333_REGISTER_START;
      registerCount = JSY_333_REGISTER_COUNT;
      break;

    default:
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
    _data.clear();
    if (_callback) {
      _callback(EventType::EVT_READ_TIMEOUT, _data);
    }
    return false;
  }

  if (result == ReadResult::READ_ERROR_COUNT || result == ReadResult::READ_ERROR_CRC) {
    // reset live values in case of read failure
    _data.clear();
    if (_callback) {
      _callback(EventType::EVT_READ_ERROR, _data);
    }
    return false;
  }

  if (result == ReadResult::READ_ERROR_ADDRESS) {
    // we have set a destination address, but we read another device
    if (_callback) {
      _callback(EventType::EVT_READ_ERROR, _data);
    }
    return false;
  }

  assert(result == ReadResult::READ_SUCCESS);

  _data.address = _buffer[JSY_RESPONSE_ADDRESS];
  _data.model = model;

  switch (model) {
    case MYCILA_JSY_MK_1031: {
      // single channel
      _data._metrics[0].frequency = _register16(_buffer, registerStart, registerSize, JSY_1031_REGISTER_FREQUENCY) * 0.01f;
      _data._metrics[0].voltage = _register16(_buffer, registerStart, registerSize, JSY_1031_REGISTER_VOLTAGE) * 0.01f;
      _data._metrics[0].current = _register32(_buffer, registerStart, registerSize, JSY_1031_REGISTER_CURRENT) * 0.0001f;
      _data._metrics[0].activePower = _register32(_buffer, registerStart, registerSize, JSY_1031_REGISTER_ACTIVE_POWER) * 0.0001f; // note: spec says /100 but in reality this is /10000
      _data._metrics[0].activeEnergy = _register32(_buffer, registerStart, registerSize, JSY_1031_REGISTER_ACTIVE_ENERGY) * 10;
      _data._metrics[0].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_1031_REGISTER_POWER_FACTOR) * 0.001f;
      _data._metrics[0].apparentPower = _register32(_buffer, registerStart, registerSize, JSY_1031_REGISTER_APPARENT_POWER) * 0.0001f; // note: spec says /100 but in reality this is /10000
      _data._metrics[0].reactivePower = _register32(_buffer, registerStart, registerSize, JSY_1031_REGISTER_REACTIVE_POWER) * 0.0001f; // note: spec says /100 but in reality this is /10000

      // aggregate
      _data.aggregate = _data._metrics[0];

      break;
    }

    case MYCILA_JSY_MK_163: {
      // signs
      // _buffer[19] unused
      // _buffer[20] is the sign of power
      uint8_t sign = _register8(_buffer, registerStart, registerSize, JSY_163_REGISTER_ACTIVE_POWER_SIGN, 1);

      // single channel
      _data._metrics[0].frequency = _register16(_buffer, registerStart, registerSize, JSY_163_REGISTER_FREQUENCY) * 0.01f;
      _data._metrics[0].voltage = _register16(_buffer, registerStart, registerSize, JSY_163_REGISTER_VOLTAGE) * 0.01f;
      _data._metrics[0].current = _register16(_buffer, registerStart, registerSize, JSY_163_REGISTER_CURRENT) * 0.01f;
      _data._metrics[0].activePower = _register16(_buffer, registerStart, registerSize, JSY_163_REGISTER_ACTIVE_POWER) * (sign ? -1.0f : 1.0f);
      _data._metrics[0].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_163_REGISTER_ACTIVE_ENERGY_IMPORTED) * 5.0f / 16.0f;
      _data._metrics[0].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_163_REGISTER_POWER_FACTOR) * 0.001f;
      _data._metrics[0].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_163_REGISTER_ACTIVE_ENERGY_RETURNED) * 5.0f / 16.0f;

      // calculate remaining metrics
      // S = P / PF
      _data._metrics[0].apparentPower = _data._metrics[0].powerFactor == 0 ? 0 : std::abs(_data._metrics[0].activePower / _data._metrics[0].powerFactor);
      // Q = std::sqrt(S^2 - P^2)
      _data._metrics[0].reactivePower = std::sqrt(_data._metrics[0].apparentPower * _data._metrics[0].apparentPower - _data._metrics[0].activePower * _data._metrics[0].activePower);
      // E = Ei + Er
      _data._metrics[0].activeEnergy = _data._metrics[0].activeEnergyImported + _data._metrics[0].activeEnergyReturned;

      // aggregate
      _data.aggregate = _data._metrics[0];

      break;
    }

    case MYCILA_JSY_MK_193: {
      // channel 1
      _data._metrics[0].voltage = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_VOLTAGE) * 0.01f;
      _data._metrics[0].current = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_CURRENT) * 0.01f;
      _data._metrics[0].activePower = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_ACTIVE_POWER) * (_register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_ACTIVE_POWER_SIGN) ? -1.0f : 1.0f);
      _data._metrics[0].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_ACTIVE_ENERGY_POSITIVE) * 10;
      _data._metrics[0].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_ACTIVE_ENERGY_NEGATIVE) * 10;
      _data._metrics[0].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_POWER_FACTOR) * 0.001f;
      _data._metrics[0].frequency = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH1_FREQUENCY) * 0.01f;

      // channel 2
      _data._metrics[1].voltage = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_VOLTAGE) * 0.01f;
      _data._metrics[1].current = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_CURRENT) * 0.01f;
      _data._metrics[1].activePower = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_ACTIVE_POWER) * (_register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_ACTIVE_POWER_SIGN) ? -1.0f : 1.0f);
      _data._metrics[1].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_ACTIVE_ENERGY_POSITIVE) * 10;
      _data._metrics[1].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_ACTIVE_ENERGY_NEGATIVE) * 10;
      _data._metrics[1].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_POWER_FACTOR) * 0.001f;
      _data._metrics[1].frequency = _register16(_buffer, registerStart, registerSize, JSY_193_REGISTER_CH2_FREQUENCY) * 0.01f;

      // calculate remaining metrics
      // S = P / PF
      _data._metrics[0].apparentPower = _data._metrics[0].powerFactor == 0 ? 0 : std::abs(_data._metrics[0].activePower / _data._metrics[0].powerFactor);
      _data._metrics[1].apparentPower = _data._metrics[1].powerFactor == 0 ? 0 : std::abs(_data._metrics[1].activePower / _data._metrics[1].powerFactor);
      // Q = std::sqrt(S^2 - P^2)
      _data._metrics[0].reactivePower = std::sqrt(_data._metrics[0].apparentPower * _data._metrics[0].apparentPower - _data._metrics[0].activePower * _data._metrics[0].activePower);
      _data._metrics[1].reactivePower = std::sqrt(_data._metrics[1].apparentPower * _data._metrics[1].apparentPower - _data._metrics[1].activePower * _data._metrics[1].activePower);
      // E = Ei + Er
      _data._metrics[0].activeEnergy = _data._metrics[0].activeEnergyImported + _data._metrics[0].activeEnergyReturned;
      _data._metrics[1].activeEnergy = _data._metrics[1].activeEnergyImported + _data._metrics[1].activeEnergyReturned;

      // aggregate
      _data.aggregate = _data._metrics[0];
      _data.aggregate += _data._metrics[1];
      _data.aggregate.voltage = std::max(_data._metrics[0].voltage, _data._metrics[1].voltage);
      _data.aggregate.frequency = std::max(_data._metrics[0].frequency, _data._metrics[1].frequency);

      break;
    }

    case MYCILA_JSY_MK_194: {
      // signs
      // _buffer[27] is the sign of power1
      // _buffer[28] is the sign of power2
      // _buffer[29] unused
      // _buffer[30] unused
      uint8_t sign0 = _register8(_buffer, registerStart, registerSize, JSY_194_REGISTER_ACTIVE_POWER_SIGNS, 0);
      uint8_t sign1 = _register8(_buffer, registerStart, registerSize, JSY_194_REGISTER_ACTIVE_POWER_SIGNS, 1);

      // frequency
      float frequency = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_FREQUENCY) * 0.01f;

      // channel 1
      _data._metrics[0].frequency = frequency;
      _data._metrics[0].voltage = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH1_VOLTAGE) * 0.0001f;
      _data._metrics[0].current = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH1_CURRENT) * 0.0001f;
      _data._metrics[0].activePower = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH1_ACTIVE_POWER) * (sign0 ? -0.0001f : 0.0001f);
      _data._metrics[0].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH1_ACTIVE_ENERGY_IMPORTED) * 0.1f;
      _data._metrics[0].powerFactor = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH1_POWER_FACTOR) * 0.001f;
      _data._metrics[0].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH1_ACTIVE_ENERGY_RETURNED) * 0.1f;

      // channel 2
      _data._metrics[1].frequency = frequency;
      _data._metrics[1].voltage = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH2_VOLTAGE) * 0.0001f;
      _data._metrics[1].current = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH2_CURRENT) * 0.0001f;
      _data._metrics[1].activePower = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH2_ACTIVE_POWER) * (sign1 ? -0.0001f : 0.0001f);
      _data._metrics[1].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH2_ACTIVE_ENERGY_IMPORTED) * 0.1f;
      _data._metrics[1].powerFactor = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH2_POWER_FACTOR) * 0.001f;
      _data._metrics[1].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_194_REGISTER_CH2_ACTIVE_ENERGY_RETURNED) * 0.1f;

      // calculate remaining metrics
      // S = P / PF
      _data._metrics[0].apparentPower = _data._metrics[0].powerFactor == 0 ? 0 : std::abs(_data._metrics[0].activePower / _data._metrics[0].powerFactor);
      _data._metrics[1].apparentPower = _data._metrics[1].powerFactor == 0 ? 0 : std::abs(_data._metrics[1].activePower / _data._metrics[1].powerFactor);
      // Q = std::sqrt(S^2 - P^2)
      _data._metrics[0].reactivePower = std::sqrt(_data._metrics[0].apparentPower * _data._metrics[0].apparentPower - _data._metrics[0].activePower * _data._metrics[0].activePower);
      _data._metrics[1].reactivePower = std::sqrt(_data._metrics[1].apparentPower * _data._metrics[1].apparentPower - _data._metrics[1].activePower * _data._metrics[1].activePower);
      // E = Ei + Er
      _data._metrics[0].activeEnergy = _data._metrics[0].activeEnergyImported + _data._metrics[0].activeEnergyReturned;
      _data._metrics[1].activeEnergy = _data._metrics[1].activeEnergyImported + _data._metrics[1].activeEnergyReturned;

      // aggregate
      _data.aggregate = _data._metrics[0];
      _data.aggregate += _data._metrics[1];
      _data.aggregate.frequency = frequency;
      _data.aggregate.voltage = std::max(_data._metrics[0].voltage, _data._metrics[1].voltage);

      break;
    }

    case MYCILA_JSY_MK_227:
    case MYCILA_JSY_MK_229: {
      // single channel
      _data._metrics[0].voltage = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_VOLTAGE) * 0.0001f;
      _data._metrics[0].current = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_CURRENT) * 0.0001f;
      _data._metrics[0].activePower = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_ACTIVE_POWER) * (_register16(_buffer, registerStart, registerSize, JSY_22x_REGISTER_ACTIVE_POWER_SIGN) ? -0.0001f : 0.0001f);
      _data._metrics[0].reactivePower = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_REACTIVE_POWER) * (_register16(_buffer, registerStart, registerSize, JSY_22x_REGISTER_REACTIVE_POWER_SIGN) ? -0.0001f : 0.0001f);
      _data._metrics[0].apparentPower = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_APPARENT_POWER) * 0.0001f;
      _data._metrics[0].powerFactor = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_POWER_FACTOR) * 0.001f;
      _data._metrics[0].frequency = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_FREQUENCY) * 0.01f;
      _data._metrics[0].activeEnergy = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_ACTIVE_ENERGY);
      _data._metrics[0].reactiveEnergy = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_REACTIVE_ENERGY);
      _data._metrics[0].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_ACTIVE_ENERGY_POSITIVE);
      _data._metrics[0].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_ACTIVE_ENERGY_NEGATIVE);
      _data._metrics[0].reactiveEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_REACTIVE_ENERGY_POSITIVE);
      _data._metrics[0].reactiveEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_22x_REGISTER_REACTIVE_ENERGY_NEGATIVE);

      // aggregate
      _data.aggregate = _data._metrics[0];

      break;
    }

    case MYCILA_JSY_MK_333: {
      // signs
      // _buffer[203] unused
      // _buffer[204] bit 7: sign of total reactive power
      // _buffer[204] bit 6: sign of phase C reactive power
      // _buffer[204] bit 5: sign of phase B reactive power
      // _buffer[204] bit 4: sign of phase A reactive power
      // _buffer[204] bit 3: sign of total active power
      // _buffer[204] bit 2: sign of phase C active power
      // _buffer[204] bit 1: sign of phase B active power
      // _buffer[204] bit 0: sign of phase A active power
      uint8_t sign7 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x80;
      uint8_t sign6 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x40;
      uint8_t sign5 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x20;
      uint8_t sign4 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x10;
      uint8_t sign3 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x08;
      uint8_t sign2 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x04;
      uint8_t sign1 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x02;
      uint8_t sign0 = _register8(_buffer, registerStart, registerSize, JSY_333_REGISTER_POWER_SIGNS, 1) & 0x01;

      // frequency
      float frequency = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_FREQUENCY) * 0.01f;

      // phase A
      _data._metrics[0].frequency = frequency;
      _data._metrics[0].voltage = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_VOLTAGE) * 0.01f;
      _data._metrics[0].current = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_CURRENT) * 0.01f;
      _data._metrics[0].activePower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_ACTIVE_POWER) * (sign0 ? -1.0f : 1.0f);
      _data._metrics[0].reactivePower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_REACTIVE_POWER) * (sign4 ? -1.0f : 1.0f);
      _data._metrics[0].apparentPower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_APPARENT_POWER);
      _data._metrics[0].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_POWER_FACTOR) * 0.001f;
      _data._metrics[0].activeEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_ACTIVE_ENERGY) * 10;
      _data._metrics[0].reactiveEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_REACTIVE_ENERGY) * 10;
      _data._metrics[0].apparentEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_APPARENT_ENERGY) * 10;
      _data._metrics[0].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_ACTIVE_ENERGY_IMPORTED) * 10;
      _data._metrics[0].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_ACTIVE_ENERGY_RETURNED) * 10;
      _data._metrics[0].reactiveEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_REACTIVE_ENERGY_IMPORTED) * 10;
      _data._metrics[0].reactiveEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_REACTIVE_ENERGY_RETURNED) * 10;
      _data._metrics[0].phaseAngleU = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_PHASE_ANGLE_U) * 0.01f;
      _data._metrics[0].phaseAngleI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_PHASE_ANGLE_I) * 0.01f;
      _data._metrics[0].phaseAngleUI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_PHASE_ANGLE_UI) * 0.01f;
      _data._metrics[0].thdU = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_THD_U) * 0.01f;
      _data._metrics[0].thdI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_A_THD_I) * 0.01f;

      // phase B
      _data._metrics[1].frequency = frequency;
      _data._metrics[1].voltage = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_VOLTAGE) * 0.01f;
      _data._metrics[1].current = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_CURRENT) * 0.01f;
      _data._metrics[1].activePower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_ACTIVE_POWER) * (sign1 ? -1.0f : 1.0f);
      _data._metrics[1].reactivePower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_REACTIVE_POWER) * (sign5 ? -1.0f : 1.0f);
      _data._metrics[1].apparentPower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_APPARENT_POWER);
      _data._metrics[1].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_POWER_FACTOR) * 0.001f;
      _data._metrics[1].activeEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_ACTIVE_ENERGY) * 0.01f;
      _data._metrics[1].reactiveEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_REACTIVE_ENERGY) * 10;
      _data._metrics[1].apparentEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_APPARENT_ENERGY) * 10;
      _data._metrics[1].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_ACTIVE_ENERGY_IMPORTED) * 10;
      _data._metrics[1].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_ACTIVE_ENERGY_RETURNED) * 10;
      _data._metrics[1].reactiveEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_REACTIVE_ENERGY_IMPORTED) * 10;
      _data._metrics[1].reactiveEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_REACTIVE_ENERGY_RETURNED) * 10;
      _data._metrics[1].phaseAngleU = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_PHASE_ANGLE_U) * 0.01f;
      _data._metrics[1].phaseAngleI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_PHASE_ANGLE_I) * 0.01f;
      _data._metrics[1].phaseAngleUI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_PHASE_ANGLE_UI) * 0.01f;
      _data._metrics[1].thdU = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_THD_U) * 0.01f;
      _data._metrics[1].thdI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_B_THD_I) * 0.01f;

      // phase C
      _data._metrics[2].frequency = frequency;
      _data._metrics[2].voltage = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_VOLTAGE) * 0.01f;
      _data._metrics[2].current = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_CURRENT) * 0.01f;
      _data._metrics[2].activePower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_ACTIVE_POWER) * (sign2 ? -1.0f : 1.0f);
      _data._metrics[2].reactivePower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_REACTIVE_POWER) * (sign6 ? -1.0f : 1.0f);
      _data._metrics[2].apparentPower = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_APPARENT_POWER);
      _data._metrics[2].powerFactor = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_POWER_FACTOR) * 0.001f;
      _data._metrics[2].activeEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_ACTIVE_ENERGY) * 10;
      _data._metrics[2].reactiveEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_REACTIVE_ENERGY) * 10;
      _data._metrics[2].apparentEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_APPARENT_ENERGY) * 10;
      _data._metrics[2].activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_ACTIVE_ENERGY_IMPORTED) * 10;
      _data._metrics[2].activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_ACTIVE_ENERGY_RETURNED) * 10;
      _data._metrics[2].reactiveEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_REACTIVE_ENERGY_IMPORTED) * 10;
      _data._metrics[2].reactiveEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_REACTIVE_ENERGY_RETURNED) * 10;
      _data._metrics[2].phaseAngleU = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_PHASE_ANGLE_U) * 0.01f;
      _data._metrics[2].phaseAngleI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_PHASE_ANGLE_I) * 0.01f;
      _data._metrics[2].phaseAngleUI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_PHASE_ANGLE_UI) * 0.01f;
      _data._metrics[2].thdU = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_THD_U) * 0.01f;
      _data._metrics[2].thdI = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_PHASE_C_THD_I) * 0.01f;

      // aggregate
      _data.aggregate.frequency = frequency;
      _data.aggregate.activePower = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_ACTIVE_POWER) * (sign3 ? -1.0f : 1.0f);
      _data.aggregate.reactivePower = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_REACTIVE_POWER) * (sign7 ? -1.0f : 1.0f);
      _data.aggregate.apparentPower = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_APPARENT_POWER);
      _data.aggregate.powerFactor = _register16(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_POWER_FACTOR) * 0.001f;
      _data.aggregate.activeEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_ACTIVE_ENERGY) * 10;
      _data.aggregate.reactiveEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_REACTIVE_ENERGY) * 10;
      _data.aggregate.apparentEnergy = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_APPARENT_ENERGY) * 10;
      _data.aggregate.activeEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_ACTIVE_ENERGY_IMPORTED) * 10;
      _data.aggregate.activeEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_ACTIVE_ENERGY_RETURNED) * 10;
      _data.aggregate.reactiveEnergyImported = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_REACTIVE_ENERGY_IMPORTED) * 10;
      _data.aggregate.reactiveEnergyReturned = _register32(_buffer, registerStart, registerSize, JSY_333_REGISTER_TOTAL_REACTIVE_ENERGY_RETURNED) * 10;
      _data.aggregate.current = _data._metrics[0].current + _data._metrics[1].current + _data._metrics[2].current;
      _data.aggregate.voltage = _data.aggregate.current == 0 ? NAN : _data.aggregate.apparentPower / _data.aggregate.current;

      break;
    }

    default:
      break;
  }

  _time = millis();

  if (_callback) {
    _callback(EventType::EVT_READ, _data);
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

  std::lock_guard<std::mutex> lock(_mutex);

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] readModel(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_READ_MODEL, JSY_REQUEST_READ_MODEL_LEN);
  _send(address, JSY_REQUEST_READ_MODEL_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_READ_MODEL, _baudRate);

  if (result != ReadResult::READ_SUCCESS) {
    return MYCILA_JSY_MK_UNKNOWN;
  }

  return (_buffer[JSY_RESPONSE_DATA] << 8) + _buffer[JSY_RESPONSE_DATA + 1];
}

///////////////////////////////////////////////////////////////////////////////
// readMode / setMode
///////////////////////////////////////////////////////////////////////////////

Mycila::JSY::Mode Mycila::JSY::_readMode(const uint8_t address, const uint16_t model) {
  if (!_enabled)
    return Mode::UNKNOWN;

  switch (model) {
    case MYCILA_JSY_MK_163:
    case MYCILA_JSY_MK_193:
    case MYCILA_JSY_MK_194:
    case MYCILA_JSY_MK_333:
      return Mode::AC;
    case MYCILA_JSY_MK_227:
    case MYCILA_JSY_MK_229:
      return Mode::DC;
    case MYCILA_JSY_MK_1031:
      break; // reads the mode just after
    default:
      return Mode::UNKNOWN;
  }

  LOGD(TAG, "readMode(0x%02X)", address);

  std::lock_guard<std::mutex> lock(_mutex);

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] readMode(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_READ_MODE, JSY_REQUEST_READ_MODE_LEN);
  _send(address, JSY_REQUEST_READ_MODE_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_READ_MODE, _baudRate);

  if (result != ReadResult::READ_SUCCESS) {
    return Mode::UNKNOWN;
  }

  switch (_buffer[JSY_RESPONSE_DATA]) {
    case JSY_MODE_AC:
      return Mode::AC;
    case JSY_MODE_DC:
      return Mode::DC;
    default:
      return Mode::UNKNOWN;
  }
}

bool Mycila::JSY::_setMode(const uint8_t address, const uint16_t model, const Mode mode) {
  if (!_enabled)
    return false;

  if (mode == Mode::UNKNOWN)
    return false;

  switch (model) {
    case MYCILA_JSY_MK_163:
    case MYCILA_JSY_MK_193:
    case MYCILA_JSY_MK_194:
    case MYCILA_JSY_MK_333:
      return mode == Mode::AC;
    case MYCILA_JSY_MK_227:
    case MYCILA_JSY_MK_229:
      return mode == Mode::DC;
    case MYCILA_JSY_MK_1031:
      break; // sets the mode just after
    default:
      return false;
  }

  LOGD(TAG, "setMode(0x%02X) mode=%s", address, mode == Mode::AC ? "AC" : "DC");

  std::lock_guard<std::mutex> lock(_mutex);

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] setMode(0x%02X, %s)\n", address, mode == Mode::AC ? "AC" : "DC");
#endif

  memcpy(_buffer, JSY_REQUEST_SWITCH_MODE, JSY_REQUEST_SWITCH_MODE_LEN);

  _buffer[JSY_REQUEST_SET_MODE] = mode == Mode::AC ? JSY_MODE_AC : JSY_MODE_DC;

  _send(address, JSY_REQUEST_SWITCH_MODE_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_SWITCH_MODE, _baudRate);

  return result == ReadResult::READ_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// resetEnergy
///////////////////////////////////////////////////////////////////////////////

bool Mycila::JSY::resetEnergy(const uint8_t address) {
  if (!_enabled)
    return false;

  LOGD(TAG, "resetEnergy(0x%02X)", address);

  std::lock_guard<std::mutex> lock(_mutex);

#ifdef MYCILA_JSY_DEBUG
  Serial.printf("[JSY] resetEnergy(0x%02X)\n", address);
#endif

  memcpy(_buffer, JSY_REQUEST_RESET_ENERGY, JSY_REQUEST_RESET_ENERGY_LEN);
  _send(address, JSY_REQUEST_RESET_ENERGY_LEN);
  ReadResult result = _timedRead(address, JSY_RESPONSE_SIZE_RESET_ENERGY, _baudRate);

  return result == ReadResult::READ_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// settings
///////////////////////////////////////////////////////////////////////////////

Mycila::JSY::BaudRate Mycila::JSY::getMinAvailableBaudRate() const {
  if (!_enabled)
    return BaudRate::UNKNOWN;
  return getMinAvailableBaudRate(_model);
}

Mycila::JSY::BaudRate Mycila::JSY::getMinAvailableBaudRate(uint16_t model) {
  switch (model) {
    case MYCILA_JSY_MK_163:
    case MYCILA_JSY_MK_193:
    case MYCILA_JSY_MK_194:
    case MYCILA_JSY_MK_227:
    case MYCILA_JSY_MK_229:
      return BaudRate::BAUD_1200;
      return BaudRate::BAUD_1200;
    case MYCILA_JSY_MK_1031:
    case MYCILA_JSY_MK_333:
      return BaudRate::BAUD_4800;
    default:
      return BaudRate::UNKNOWN;
  }
}

Mycila::JSY::BaudRate Mycila::JSY::getMaxAvailableBaudRate() const {
  if (!_enabled)
    return BaudRate::UNKNOWN;
  return getMaxAvailableBaudRate(_model);
}

Mycila::JSY::BaudRate Mycila::JSY::getMaxAvailableBaudRate(uint16_t model) {
  switch (model) {
    case MYCILA_JSY_MK_163:
    case MYCILA_JSY_MK_227:
    case MYCILA_JSY_MK_229:
      return BaudRate::BAUD_9600;
    case MYCILA_JSY_MK_1031:
    case MYCILA_JSY_MK_333:
      return BaudRate::BAUD_19200;
    case MYCILA_JSY_MK_193:
    case MYCILA_JSY_MK_194:
      return BaudRate::BAUD_38400;
    default:
      return BaudRate::UNKNOWN;
  }
}

bool Mycila::JSY::isBaudRateSupported(Mycila::JSY::BaudRate baudRate) const {
  if (!_enabled)
    return false;
  return isBaudRateSupported(_model, baudRate);
}

bool Mycila::JSY::isBaudRateSupported(uint16_t model, Mycila::JSY::BaudRate baudRate) {
  return getMinAvailableBaudRate(model) <= baudRate && baudRate <= getMaxAvailableBaudRate(model);
}

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

  LOGD(TAG, "set(0x%02X) address=0x%02X, bauds=%" PRIu32, address, newAddress, newBaudRate);

  std::lock_guard<std::mutex> lock(_mutex);

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
    return false;
  }

  // response from unexpected address ?
  if (result == ReadResult::READ_ERROR_ADDRESS && _lastAddress != newAddress) {
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
    LOGE(TAG, "Unable to read JSY @ 0x%02X at speed: %" PRIu32, address, newBaudRate);
    if (_baudRate != BaudRate::UNKNOWN) {
      _openSerial(_baudRate);
    }
  }

  return success;
}

///////////////////////////////////////////////////////////////////////////////
// toJson
///////////////////////////////////////////////////////////////////////////////

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::toJson(const JsonObject& root) const {
  root["enabled"] = _enabled;
  root["time"] = _time;
  root["speed"] = _baudRate;
  _data.toJson(root);
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
  LOGD(TAG, "openSerial(%" PRIu32 ")", baudRate);
  _serial->begin(baudRate, SERIAL_8N1, _pinRX, _pinTX);
  _serial->setTimeout(MYCILA_JSY_READ_TIMEOUT_MS);
  while (!_serial)
    yield();
  while (!_serial->availableForWrite())
    yield();
  _drop();
  _serial->flush(false);
}

Mycila::JSY::BaudRate Mycila::JSY::_detectBauds(const uint8_t address) {
  for (int i = 0; i < AUTO_DETECT_BAUD_RATES_COUNT * 2; i++) {
    BaudRate baudRate = AUTO_DETECT_BAUD_RATES[i % AUTO_DETECT_BAUD_RATES_COUNT];
    LOGD(TAG, "find(0x%02X) %" PRIu32 " bauds", address, baudRate);
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
// static
///////////////////////////////////////////////////////////////////////////////

// For CRC: https://crccalc.com
// Select CRC-16/MODBUS
inline uint16_t Mycila::JSY::_crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    uint8_t temp = *(data++) ^ LOBYTE(crc);
    crc = (crc >> 8) ^ pgm_read_word_near(CRCTable + temp);
  }
  return crc;
}

uint8_t Mycila::JSY::_register8(const uint8_t* buffer, const uint16_t registerStart, const uint16_t registerSize, const uint16_t registerAddress, uint8_t index) {
  return buffer[JSY_RESPONSE_DATA + (registerAddress - registerStart) * registerSize + index];
}

uint16_t Mycila::JSY::_register16(const uint8_t* buffer, const uint16_t registerStart, const uint16_t registerSize, const uint16_t registerAddress) {
  const size_t start = JSY_RESPONSE_DATA + (registerAddress - registerStart) * registerSize;
  return (buffer[start] << 8) + buffer[start + 1];
}

uint32_t Mycila::JSY::_register32(const uint8_t* buffer, const uint16_t registerStart, const uint16_t registerSize, const uint16_t registerAddress) {
  const size_t start = JSY_RESPONSE_DATA + (registerAddress - registerStart) * registerSize;
  return (buffer[start] << 24) +
         (buffer[start + 1] << 16) +
         (buffer[start + 2] << 8) +
         (buffer[start + 3]);
}

const char* Mycila::JSY::getModelName(uint16_t model) {
  switch (model) {
    case MYCILA_JSY_MK_1031:
      return MYCILA_JSY_MK_1031_NAME;
    case MYCILA_JSY_MK_163:
      return MYCILA_JSY_MK_163_NAME;
    case MYCILA_JSY_MK_193:
      return MYCILA_JSY_MK_193_NAME;
    case MYCILA_JSY_MK_194:
      return MYCILA_JSY_MK_194_NAME;
    case MYCILA_JSY_MK_227:
      return MYCILA_JSY_MK_227_NAME;
    case MYCILA_JSY_MK_229:
      return MYCILA_JSY_MK_229_NAME;
    case MYCILA_JSY_MK_333:
      return MYCILA_JSY_MK_333_NAME;
    default:
      return emptyString.c_str();
  }
}

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
