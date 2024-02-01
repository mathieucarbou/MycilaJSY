# MycilaJSY

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaJSY/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaJSY/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaJSY.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaJSY)

Arduino / ESP32 library for the JSY-MK-194T single-phase two-way electric energy metering module

- Sync mode and async mode (non-blocking)
- Core, stack size and interval can be configured
- Automatically detect baud rate
- Energy reset live at runtime
- Switch bauds rate to any supported speed live at runtime
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

### Blocking mode

```c++
void setup() {
  Mycila::JSY.begin(17, 16);
}

void loop() {
  Mycila::JSY.read();

  // access values

  delay(1000);
}
```

### Non-Blocking mode (async)

```c++
void setup() {
  Mycila::JSY.begin(17, 16, Mycila::JSYBaudRate::BAUD_38400, &MYCILA_JSY_SERIAL, true, 60, 0);
}

void loop() {
  // access values
}
```

### Energy reset

```c++
Mycila::JSY.resetEnergy();
```

### Update Baud rate (change speed)

```c++
if (Mycila::JSY.isEnabled() && Mycila::JSY.getBaudRate() != Mycila::JSYBaudRate::BAUD_38400) {
  if (Mycila::JSY.updateBaudRate(Mycila::JSYBaudRate::BAUD_38400)) {
    // speed changed and switched to new speed
  } else {
    // speed changed failed, keeping current speed
  }
  
}
```

## Reference material

- [JSY-MK-194T 1.pdf](JSY-MK-194T%201.pdf) (version 1)
- [JSY-MK-194T 2.pdf](JSY-MK-194T%202.pdf) (version 2)
- [JSY-MK-194T 3.pdf](JSY-MK-194T%203.pdf) (version 3)
- [JSY-MK-194T.exe](JSY-MK-194T.exe) (Utility to configure the JSY with a USB-TTL adapter)
- [ModbusMechanic](https://github.com/SciFiDryer/ModbusMechanic) (can connect and configure JSY with a USB-TTL adapter)
