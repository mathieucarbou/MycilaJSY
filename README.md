# MycilaJSY

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaJSY/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaJSY/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaJSY.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaJSY)

Arduino / ESP32 library for the JSY-MK-194T single-phase two-way electric energy metering module

- Sync mode and async mode
- Core, stack size and interval can be configured
- Energy reset
- Custom bauds rate
- Automatically detect and switch bauds rate
- Configurable Serial (Serial2 by default)
- Metrics:

```c++
    volatile float current1 = 0; // A
    volatile float current2 = 0; // A
    volatile float energy1 = 0; // kWh
    volatile float energy2 = 0; // kWh
    volatile float energyReturned1 = 0; // kWh
    volatile float energyReturned2 = 0; // kWh
    volatile uint8_t frequency = 0; // Hz
    volatile float power1 = 0; // W
    volatile float power2 = 0; // W
    volatile float powerFactor1 = 0;
    volatile float powerFactor2 = 0;
    volatile float voltage1 = 0; // V
    volatile float voltage2 = 0; // V
```

## Usage

See examples and API
