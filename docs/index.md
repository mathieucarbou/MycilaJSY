# MycilaJSY

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaJSY.svg)](https://GitHub.com/mathieucarbou/MycilaJSY/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaJSY.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaJSY)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![Build](https://github.com/mathieucarbou/MycilaJSY/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaJSY/actions/workflows/ci.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaJSY)](https://GitHub.com/mathieucarbou/MycilaJSY/commit/)
[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/mathieucarbou/MycilaJSY)

Arduino / ESP32 library for the JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333 families single-phase and three-phase AC bidirectional meters from [Shenzhen Jiansiyan Technologies Co, Ltd.](https://www.jsypowermeter.com)

- [API Documentation](#api-documentation)
- [Features](#features)
- [Supported models](#supported-models)
  - [JSY1031 (TTL)](#jsy1031-ttl)
  - [JSY-MK-163T (TTL)](#jsy-mk-163t-ttl)
  - [JSY-MK-193 (RS485)](#jsy-mk-193-rs485)
  - [JSY-MK-194T (TTL) and JSY-MK-194G (TTL)](#jsy-mk-194t-ttl-and-jsy-mk-194g-ttl)
  - [JSY-MK-227 (RS485) and JSY-MK-229 (RS485)](#jsy-mk-227-rs485-and-jsy-mk-229-rs485)
  - [JSY-MK-333 (TTL) and JSY-MK-333G (RS485)](#jsy-mk-333-ttl-and-jsy-mk-333g-rs485)
- [Usage](#usage)
  - [Baud rate detection / forcing a baud rate](#baud-rate-detection--forcing-a-baud-rate)
  - [Destination Address](#destination-address)
  - [Model detection / forcing a model](#model-detection--forcing-a-model)
  - [Blocking mode](#blocking-mode)
  - [Non-Blocking mode (async)](#non-blocking-mode-async)
  - [Energy reset](#energy-reset)
  - [Update Baud rate (change speed)](#update-baud-rate-change-speed)
  - [Change device address](#change-device-address)
  - [Switch AC/DC mode](#switch-acdc-mode)
  - [JSON Support](#json-support)
  - [Debugging](#debugging)
  - [Callbacks](#callbacks)
- [Remote JSY](#remote-jsy)
- [Zero-Cross Detection](#zero-cross-detection)
- [Boxes and 3D models](#boxes-and-3d-models)
- [Performance tests](#performance-tests)
- [Reference material](#reference-material)

## API Documentation

Go to: [https://mathieu.carbou.me/MycilaJSY/api/index.html](https://mathieu.carbou.me/MycilaJSY/api/index.html)

## Features

- AC/DC mode switch (JSY1031)
- Async and blocking mode (configurable core, stack and pause interval)
- Automatic baud rate detection
- Automatic device address detection
- Automatic JSY model detection
- Device address: support for multiple devices on the same bus
- Energy reset live at runtime
- Focus on speed and reactivity with a callback mechanism
- Remote support with [UDP sender](#remote-jsy)
- Support any Serial / TTL (RX/TX port)
- Switch bauds rate to any supported speed live at runtime
- Zero-Cross detection support with JSY-MK-194G and [MycilaPulseAnalyzer](https://github.com/mathieucarbou/MycilaPulseAnalyzer)

Also read the blog article: **[Everything on le JSY](https://yasolr.carbou.me/blog/2024-06-26)**

## Supported models

- [JSY1031 (TTL)](#jsy1031-ttl)
- [JSY-MK-163T (TTL)](#jsy-mk-163t-ttl)
- [JSY-MK-193 (RS485)](#jsy-mk-193-rs485)
- [JSY-MK-194T (TTL) and JSY-MK-194G (TTL)](#jsy-mk-194t-ttl-and-jsy-mk-194g-ttl)
- [JSY-MK-227 (RS485) and JSY-MK-229 (RS485)](#jsy-mk-227-rs485-and-jsy-mk-229-rs485)
- [JSY-MK-333 (TTL) and JSY-MK-333G (RS485)](#jsy-mk-333-ttl-and-jsy-mk-333g-rs485)

### JSY1031 (TTL)

- `activeEnergy`
- `activePower`
- `apparentPower`
- `current`
- `frequency`
- `powerFactor`
- `reactivePower`
- `voltage`

### JSY-MK-163T (TTL)

- `activeEnergy` (sum of `activeEnergyImported` and `activeEnergyReturned`)
- `activeEnergyImported`
- `activeEnergyReturned`
- `activePower`
- `apparentPower` (calculated)
- `current`
- `frequency`
- `powerFactor`
- `reactivePower` (calculate and positive since we do not know the phase shift angle: inductive or capacitive load)
- `voltage`

### JSY-MK-193 (RS485)

For each channel (1 and 2):

- `activeEnergy` (sum of `activeEnergyImported` and `activeEnergyReturned`)
- `activeEnergyImported`
- `activeEnergyReturned`
- `activePower`
- `apparentPower` (calculated)
- `current`
- `frequency`
- `powerFactor`
- `reactivePower` (calculate and positive since we do not know the phase shift angle: inductive or capacitive load)
- `voltage`

### JSY-MK-194T (TTL) and JSY-MK-194G (TTL)

- `frequency`

For each channel (1 and 2):

- `activeEnergy` (sum of `activeEnergyImported` and `activeEnergyReturned`)
- `activeEnergyImported`
- `activeEnergyReturned`
- `activePower`
- `apparentPower` (calculated)
- `current`
- `powerFactor`
- `reactivePower` (calculate and positive since we do not know the phase shift angle: inductive or capacitive load)
- `voltage`

### JSY-MK-227 (RS485) and JSY-MK-229 (RS485)

- `activeEnergy`
- `activeEnergyImported` (positive energy)
- `activeEnergyReturned` (negative energy)
- `activePower`
- `apparentPower`
- `current`
- `frequency`
- `powerFactor`
- `reactiveEnergy`
- `reactivePower`
- `reactiveEnergyImported` (positive energy)
- `reactiveEnergyReturned` (negative energy)
- `voltage`

### JSY-MK-333 (TTL) and JSY-MK-333G (RS485)

- `frequency`

For each phase (A, B and C):

- `voltage`
- `current`
- `activePower`
- `powerFactor`
- `apparentPower`
- `reactivePower`
- `activeEnergy`
- `activeEnergyImported`
- `activeEnergyReturned`
- `reactiveEnergy`
- `reactiveEnergyImported`
- `reactiveEnergyReturned`
- `apparentEnergy`

## Usage

**PIO file:**

Make sure to use that in your `platformio.ini`:

```ini
; Arduino 3.0.x
; platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.05/platform-espressif32.zip
; Arduino 3.1.x
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10-rc3/platform-espressif32.zip

build_flags =
  -std=c++17
  -std=gnu++17
  -Wall -Wextra

build_unflags =
    -std=gnu++11
```

1. make sure to use `pioarduino` platform because PlatformIO provided platforms are not compatible with Arduino Core 3.
2. C++ 17 is required for mutex

Arduino IDE also has a place to set compilation flags.

**Sample code:**

```c++
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  jsy.begin(Serial2, RX2, TX2);
}

void loop() {
  if (jsy.read()) {
    float v = jsy.data.single().voltage; // for JSY1031, JSY-163, JSY-227, JSY-229

    float v1 = jsy.data.channel1().voltage; // for JSY-193 and JSY-194
    float v2 = jsy.data.channel2().voltage; // for JSY-193 and JSY-194

    float vA = jsy.data.phaseA().voltage; // for JSY-333
    float vB = jsy.data.phaseB().voltage; // for JSY-333
    float vC = jsy.data.phaseC().voltage; // for JSY-333
    float p = jsy.data.aggregate().activePower; // for JSY-333
  }
  delay(4000);
}
```

- Have a look at the examples in the [examples](examples) folder
- Read

### Baud rate detection / forcing a baud rate

```c++
// by default, will try to detect the baud rate
jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::UNKNOWN);

// Skips baud rate detection and use the given baud rate
jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::BAUD_38400);
```

### Destination Address

```c++
// by default, will send to any device (broadcast)
jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::UNKNOWN, MYCILA_JSY_ADDRESS_BROADCAST);

// Use the given address
jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::UNKNOWN, 0x01);
```

### Model detection / forcing a model

```c++
// by default, will try to detect the model
jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::UNKNOWN, MYCILA_JSY_ADDRESS_BROADCAST, MYCILA_JSY_MK_UNKNOWN);

// Skips model detection and use the given model
jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::UNKNOWN, MYCILA_JSY_MK_227, MYCILA_JSY_MK_194);
```

### Blocking mode

```c++
Mycila::JSY jsy;

void setup() {
  jsy.begin(Serial2, RX2, TX2);
}

void loop() {
  jsy.read();

  // access values
  float v = jsy.data.single().voltage;

  delay(1000);
}
```

### Non-Blocking mode (async)

```c++
Mycila::JSY jsy;

void setup() {
  jsy.begin(Serial2, RX2, TX2, true);
}

void loop() {
  // access values
  float v = jsy.data.single().voltage;
}
```

### Energy reset

```c++
jsy.resetEnergy();
```

### Update Baud rate (change speed)

```c++
jsy.setBaudRate(Mycila::JSY::BaudRate::BAUD_38400);
```

### Change device address

```c++
jsy.setDeviceAddress(0x02);
```

### Switch AC/DC mode

Only for JSY1031:

```c++
jsy.setMode(Mycila::JSY::Mode::DC);
```

### JSON Support

You can activate JSON support by defining `-D MYCILA_JSON_SUPPORT` in your project and add the `ArduinoJson` library.

### Debugging

Set the flag: `-D MYCILA_JSY_DEBUG` and you will see all the JSY requests and responses.

### Callbacks

- `Callback`: called when the JSY has read the data and when a change for any of the metric is detected by the JSY.
  This is useful to be notified exactly when required.
  You must check the event type

**Example:**

Reading a load for 2 second after it is turned on:

```c++
  jsy.setCallback([](const Mycila::JSY::EventType eventType) {
    int64_t now = esp_timer_get_time();
    switch (eventType) {
      case Mycila::JSY::EventType::EVT_READ:
        Serial.printf(" - %" PRId64 " EVT_READ\n", now);
        break;
      case Mycila::JSY::EventType::EVT_CHANGE:
        Serial.printf(" - %" PRId64 " EVT_CHANGE: %f W\n", now, jsy.getActivePower2());
        break;
      default:
        Serial.printf(" - %" PRId64 " ERR\n", now);
        break;
    }
  });
```

**Output example:**

```
jsy.read() at 4800 bauds:
 - 4406487 EVT_READ
 - 4406577 EVT_CHANGE: 0.000000 W
 - 4576361 EVT_READ
 - 4576452 EVT_CHANGE: 0.567600 W
 - 4746390 EVT_READ
 - 4918004 EVT_READ
 - 4918078 EVT_CHANGE: 238.452896 W
 - 5088016 EVT_READ
 - 5257985 EVT_READ
 - 5258059 EVT_CHANGE: 501.204590 W
 - 5428006 EVT_READ
 - 5597980 EVT_READ
 - 5598055 EVT_CHANGE: 509.064789 W
 - 5767989 EVT_READ
 - 5937958 EVT_READ
 - 5938032 EVT_CHANGE: 507.976410 W
 - 6107979 EVT_READ
 - 6279620 EVT_READ
 - 6279694 EVT_CHANGE: 507.588196 W

jsy.read() at 9600 bauds:
 - 7266640 EVT_READ
 - 7266727 EVT_CHANGE: 21.132200 W
 - 7366557 EVT_READ
 - 7474854 EVT_READ
 - 7474928 EVT_CHANGE: 0.847700 W
 - 7577383 EVT_READ
 - 7677342 EVT_READ
 - 7777330 EVT_READ
 - 7877334 EVT_READ
 - 7877408 EVT_CHANGE: 497.609192 W
 - 7977363 EVT_READ
 - 8077326 EVT_READ
 - 8177318 EVT_READ
 - 8177392 EVT_CHANGE: 506.894501 W
 - 8277345 EVT_READ
 - 8377319 EVT_READ
 - 8478147 EVT_READ
 - 8478222 EVT_CHANGE: 506.710205 W
 - 8578164 EVT_READ
 - 8678138 EVT_READ
 - 8778135 EVT_READ
 - 8878132 EVT_READ
 - 8878207 EVT_CHANGE: 506.827393 W
 - 8978150 EVT_READ
 - 9078122 EVT_READ
 - 9178953 EVT_READ
 - 9179027 EVT_CHANGE: 506.932892 W

jsy.read() at 19200 bauds:
 - 9820293 EVT_READ
 - 9820383 EVT_CHANGE: 269.228302 W
 - 9880195 EVT_READ
 - 9940570 EVT_READ
 - 10000562 EVT_READ
 - 10060548 EVT_READ
 - 10120538 EVT_READ
 - 10120613 EVT_CHANGE: 19.392099 W
 - 10180976 EVT_READ
 - 10240944 EVT_READ
 - 10300926 EVT_READ
 - 10360912 EVT_READ
 - 10421327 EVT_READ
 - 10421402 EVT_CHANGE: 287.241699 W
 - 10481350 EVT_READ
 - 10541308 EVT_READ
 - 10601290 EVT_READ
 - 10661285 EVT_READ
 - 10721280 EVT_READ
 - 10781686 EVT_READ
 - 10781769 EVT_CHANGE: 506.096008 W
 - 10841700 EVT_READ
 - 10901667 EVT_READ
 - 10961658 EVT_READ
 - 11022060 EVT_READ
 - 11082050 EVT_READ
 - 11082125 EVT_CHANGE: 505.992493 W
 - 11142072 EVT_READ
 - 11202031 EVT_READ
 - 11262031 EVT_READ
 - 11322425 EVT_READ
 - 11382419 EVT_READ
 - 11442409 EVT_READ
 - 11442484 EVT_CHANGE: 505.818298 W
 - 11502441 EVT_READ
 - 11562390 EVT_READ
 - 11622804 EVT_READ
 - 11682791 EVT_READ
 - 11742799 EVT_READ
 - 11742874 EVT_CHANGE: 505.483002 W
 - 11802804 EVT_READ

jsy.read() at 38400 bauds:
 - 13017424 EVT_READ
 - 13017515 EVT_CHANGE: 11.180500 W
 - 13067332 EVT_READ
 - 13067423 EVT_CHANGE: 0.000000 W
 - 13117320 EVT_READ
 - 13157496 EVT_READ
 - 13207482 EVT_READ
 - 13247472 EVT_READ
 - 13287678 EVT_READ
 - 13337675 EVT_READ
 - 13377664 EVT_READ
 - 13417866 EVT_READ
 - 13417941 EVT_CHANGE: 320.179688 W
 - 13467890 EVT_READ
 - 13517859 EVT_READ
 - 13557844 EVT_READ
 - 13598042 EVT_READ
 - 13648037 EVT_READ
 - 13688036 EVT_READ
 - 13728237 EVT_READ
 - 13728312 EVT_CHANGE: 503.697113 W
 - 13778253 EVT_READ
 - 13828221 EVT_READ
 - 13868215 EVT_READ
 - 13908412 EVT_READ
 - 13958400 EVT_READ
 - 13998398 EVT_READ
 - 14046098 EVT_READ
 - 14046173 EVT_CHANGE: 507.100891 W
 - 14088424 EVT_READ
 - 14128377 EVT_READ
 - 14168579 EVT_READ
 - 14208568 EVT_READ
 - 14248575 EVT_READ
 - 14288771 EVT_READ
 - 14328769 EVT_READ
 - 14376462 EVT_READ
 - 14376537 EVT_CHANGE: 507.294708 W
 - 14418996 EVT_READ
 - 14468947 EVT_READ
 - 14508941 EVT_READ
 - 14548930 EVT_READ
 - 14589151 EVT_READ
 - 14629129 EVT_READ
 - 14669129 EVT_READ
 - 14709325 EVT_READ
 - 14709400 EVT_CHANGE: 507.153198 W
 - 14759359 EVT_READ
 - 14799311 EVT_READ
 - 14839513 EVT_READ
 - 14889501 EVT_READ
 - 14929499 EVT_READ
 - 14969706 EVT_READ
```

## Remote JSY

The JSY can be used connected to an ESP32 to send the JSY data several times per second to a remote server through UDP.
Both devices needs to be connected to the same network and UDP packets must be allowed.

Screenshot of the ESP32 running the JSY app called the `Sender`:

![](https://github.com/mathieucarbou/MycilaJSY/assets/61346/3066bf12-31d5-45de-9303-d810f14731d0)

[Click here for the download and installation instructions of the JSY Remote UDP Sender](https://github.com/mathieucarbou/MycilaJSY/tree/main/examples/RemoteUDP)

## Zero-Cross Detection

The JSY-MK-194G has a zero-cross detection feature which signals on the Zx pin when the current crosses zero.
This feature is really useful to create Solr Routers like [YaSolR](https://yasolr.carbou.me).

The [MycilaPulseAnalyzer](https://github.com/mathieucarbou/MycilaPulseAnalyzer) library supports many ZC detection methods including the JSY-MK-194G.

[![](https://mathieu.carbou.me/MycilaPulseAnalyzer/assets/Oscillo_JSY-MK-194G_ZC_5ms.png)](https://mathieu.carbou.me/MycilaPulseAnalyzer/assets/Oscillo_JSY-MK-194G_ZC_5ms.png)

## Boxes and 3D models

You can make (print) a box or buy one, and then append a SSR Din Rail clip at the bottom:

- [https://fr.aliexpress.com/item/1005005490499647.html](https://fr.aliexpress.com/item/1005005490499647.html)

**Cases for the JSY-MK-194 T**

- [https://www.thingiverse.com/thing:6003867](https://www.thingiverse.com/thing:6003867)

**Cases for the JSY-MK-194 G**

- [https://fr.aliexpress.com/item/1005002467813588.html](https://fr.aliexpress.com/item/1005002467813588.html)
- [https://www.printables.com/model/72839-customizable-parametric-stable-and-waterproof-elec](https://www.printables.com/model/72839-customizable-parametric-stable-and-waterproof-elec)
- [https://www.thingiverse.com/thing:4921568](https://www.thingiverse.com/thing:4921568)
- [https://www.thingiverse.com/thing:2252270](https://www.thingiverse.com/thing:2252270)

Here is the STL file I use for the JSY-MK-194G box mounted on a DIN rail: [JSY-MK-194G.stl](https://mathieu.carbou.me/MycilaJSY/JSY-MK-194G.stl) which was created thanks to [this parametric model](https://www.printables.com/model/72839-customizable-parametric-stable-and-waterproof-elec) with these parameters:

```ini
ShowBottom                = true;
ShowTop                   = true;
DistanceBetweenObjects    = 10;
ShowCaseAssembled         = false;

Caselength                = 90;
CaseWidth                 = 60;
CaseHeight                = 42;

BottomTopThickness        = 2.0;

CountersinkScrew          = 3.0;

ShowDeviceHolder         = true;
ScrewHoleDiameter        = 2.6;
ScrewCylinderDiameter    = 7;
ScrewCylinderHeight      = 7.0;
DeviceHolder_X_Distance  = 58;
DeviceHolder_y_Distance  = 42;
Offset_X                 = 0;
Offset_Y                 = 0;
ShowSideWallHoles        = true;
SideWallHolesOn_X        = false;
CountOfSideWallHoles     = 2;
SideWallHoleDiameter     = 20;
SiedWallHoleOffset_Z     = 0;
```

| [![](https://mathieu.carbou.me/MycilaJSY/jsy-box-1.jpeg)](https://mathieu.carbou.me/MycilaJSY/jsy-box-1.jpeg) | [![](https://mathieu.carbou.me/MycilaJSY/jsy-box-2.jpeg)](https://mathieu.carbou.me/MycilaJSY/jsy-box-2.jpeg) | [![](https://mathieu.carbou.me/MycilaJSY/jsy-box-3.jpeg)](https://mathieu.carbou.me/MycilaJSY/jsy-box-3.jpeg) |

## Performance tests

Here are below some test results for the JSY at different baud rates for 50 consecutive reads on a nominal load of about 650W, controlled with a random SSR relay (0-100%).

You can run the `PerfTests` from the examples.

**PerfTest1**

```
jsy.read() at 4800 bauds:
 - Errors: 0
 - Average read time: 170583 us
 - Min read time: 168330 us
 - Max read time: 178322 us

jsy.read() at 9600 bauds:
 - Errors: 0
 - Average read time: 100525 us
 - Min read time: 99897 us
 - Max read time: 108333 us

jsy.read() at 19200 bauds:
 - Errors: 0
 - Average read time: 60078 us
 - Min read time: 59568 us
 - Max read time: 60413 us

jsy.read() at 38400 bauds:
 - Errors: 0
 - Average read time: 41248 us
 - Min read time: 39894 us
 - Max read time: 49936 us

```

**PerfTest2**

```
jsy.read() at 4800 bauds:
 - ROUND: 1
   * Load Detection time: 169670 us (1 reads)
   * Ramp up time: 347890 us (2 reads)
   * Ramp down time: 1363108 us (8 reads)
 - ROUND: 2
   * Load Detection time: 679860 us (4 reads)
   * Ramp up time: 1191511 us (7 reads)
   * Ramp down time: 1531476 us (9 reads)
 - ROUND: 3
   * Load Detection time: 679867 us (4 reads)
   * Ramp up time: 1198192 us (7 reads)
   * Ramp down time: 1363164 us (8 reads)
 - ROUND: 4
   * Load Detection time: 681522 us (4 reads)
   * Ramp up time: 1191498 us (7 reads)
   * Ramp down time: 1531493 us (9 reads)

jsy.read() at 9600 bauds:
 - ROUND: 1
   * Load Detection time: 299799 us (3 reads)
   * Ramp up time: 1100600 us (11 reads)
   * Ramp down time: 1501484 us (15 reads)
 - ROUND: 2
   * Load Detection time: 700694 us (7 reads)
   * Ramp up time: 1101510 us (11 reads)
   * Ramp down time: 1501474 us (15 reads)
 - ROUND: 3
   * Load Detection time: 400709 us (4 reads)
   * Ramp up time: 800693 us (8 reads)
   * Ramp down time: 1201502 us (12 reads)
 - ROUND: 4
   * Load Detection time: 700710 us (7 reads)
   * Ramp up time: 800693 us (8 reads)
   * Ramp down time: 1200678 us (12 reads)

jsy.read() at 19200 bauds:
 - ROUND: 1
   * Load Detection time: 362264 us (6 reads)
   * Ramp up time: 1082982 us (18 reads)
   * Ramp down time: 1261343 us (21 reads)
 - ROUND: 2
   * Load Detection time: 300247 us (5 reads)
   * Ramp up time: 1020957 us (17 reads)
   * Ramp down time: 1261351 us (21 reads)
 - ROUND: 3
   * Load Detection time: 360253 us (6 reads)
   * Ramp up time: 1081379 us (18 reads)
   * Ramp down time: 1260926 us (21 reads)
 - ROUND: 4
   * Load Detection time: 300273 us (5 reads)
   * Ramp up time: 1020982 us (17 reads)
   * Ramp down time: 1261351 us (21 reads)

jsy.read() at 38400 bauds:
 - ROUND: 1
   * Load Detection time: 468076 us (11 reads)
   * Ramp up time: 840729 us (20 reads)
   * Ramp down time: 1291332 us (32 reads)
 - ROUND: 2
   * Load Detection time: 330244 us (8 reads)
   * Ramp up time: 1020971 us (25 reads)
   * Ramp down time: 1281353 us (32 reads)
 - ROUND: 3
   * Load Detection time: 330047 us (8 reads)
   * Ramp up time: 1030773 us (25 reads)
   * Ramp down time: 1301323 us (32 reads)
 - ROUND: 4
   * Load Detection time: 330047 us (8 reads)
   * Ramp up time: 1010776 us (25 reads)
   * Ramp down time: 1281343 us (32 reads)
```

The "Load Detection time" is the time it takes for the JSY to see a load `> 0` just after the relay was closed on a constant resistive load.

The "Ramp up time" is the time it takes for the JSY to stabilize (with a maximum delta of 1% from last measurement) and see an expected load value.

The "Ramp down time" is the time it takes for the JSY to return to 0W after the relay was opened.

**What these results mean ?**

- Speed up the JSY allows to **read data faster** and consequently block less the program if not using any dedicated task to read the JSY

- It takes **more than 300 ms** for the JSY to report a change of a load appearing on the wire.
  So if you read the JSY each 300 ms, it can take up to 600 ms to see a change if you miss it.
  Reading the JSY more frequently (in a async loop) and faster will allow a change to be seen as fast as possible, but it will still take at least 300 ms for the change to be seen.
  **The best reactivity is achieved when reading the JSY in a loop at 38400 bauds, when the detection time is around 330 ms and quite consistent.**

- It takes **more than 1 second** for the JSY to report a load value which is stable and according to what we expect.
  This duration contains the duration for the load to reach its nominal power, plus the duration it takes for the JSY to stabilize its measurements (I think the JSY is using some kind of filtering or averaging).

- Reading the JSY too frequently will lead to the same results, so an improvement could be to have the JSY read in a dedicated task asynchronously and use the callback mechanism to be called as soon as the JSY sees a change

## Reference material

- [JSY1031.pdf](https://mathieu.carbou.me/MycilaJSY/JSY1031.pdf)
- [JSY-MK-163T.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-163T.pdf)
- [JSY-MK-193.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-193.pdf)
- [JSY-MK-194T.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-194T.pdf)
- [JSY-MK-194T_Manual.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-194T_Manual.pdf)
- [JSY-MK-194G.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-194G.pdf)
- [JSY-MK-227.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-227.pdf)
- [JSY-MK-229.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-229.pdf)
- [JSY-MK-333.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-333.pdf)
- [JSY-MK-333G.pdf](https://mathieu.carbou.me/MycilaJSY/JSY-MK-333G.pdf)
- [RENERGY RN8209G](https://mathieu.carbou.me/MycilaJSY/RENERGY-RN8209G.pdf)
- [ModbusMechanic](https://github.com/SciFiDryer/ModbusMechanic) (can connect and configure JSY with a USB-TTL adapter)
