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
- Focus on speed
- Metrics:

```c++
    // tore or first clamp
    volatile float current1 = 0; // A
    volatile float energy1 = 0; // kWh
    volatile float energyReturned1 = 0; // kWh
    volatile float power1 = 0; // W
    volatile float powerFactor1 = 0;
    volatile float voltage1 = 0; // V

    // clamp (or second clamp)
    volatile float current2 = 0; // A
    volatile float energy2 = 0; // kWh
    volatile float energyReturned2 = 0; // kWh
    volatile float power2 = 0; // W
    volatile float powerFactor2 = 0;
    volatile float voltage2 = 0; // V

    volatile uint8_t frequency = 0; // Hz
```

## Usage

Have a look at all the examples in the [examples](examples) folder.

There is a getter for each metric.

### Blocking mode

```c++
Mycila::JSY jsy;

void setup() {
  jsy.begin(&Serial2, 16, 17);
}

void loop() {
  jsy.read();

  // access values

  delay(1000);
}
```

### Non-Blocking mode (async)

```c++
Mycila::JSY jsy;

void setup() {
  jsy.begin(&Serial2, 16, 17, true, 0);
}

void loop() {
  // access values
}
```

### Energy reset

```c++
jsy.resetEnergy();
```

### Update Baud rate (change speed)

```c++
if (jsy.isEnabled() && jsy.getBaudRate() != Mycila::JSYBaudRate::BAUD_38400) {
  if (jsy.setBaudRate(Mycila::JSYBaudRate::BAUD_38400)) {
    // speed changed and switched to new speed
  } else {
    // speed changed failed, keeping current speed
  }

}
```

## Performance tests

Here are below some test results for the JSY at different baud rates for 50 consecutive reads on a nominal load of about 650W, controlled with a random SSR relay (0-100%).

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

You can run the `PerfTests` from the examples.
The numbers might change a little (be more accurate for example for 19200 bauds) but the order of magnitude should stay the same.

**What these results mean ?**

- Speed up the JSY allows to **read data faster** and consequently block less the program if not using any dedicated task to read the JSY

- It takes **more than 300 ms** for the JSY to report a change of a load appearing on the wire.
  So if you read the JSY each 300 ms, it can take up to 600 ms to see a change if you miss it.
  Reading the JSY more frequently (in a async loop) and faster will allow a change to be seen as fast as possible, but it will still take at least 300 ms for the change to be seen.
  **The best reactivity is achieved when reading the JSY in a loop at 38400 bauds, when the detection time is around 330 ms and quite consistent.**

- It takes **more than 1 second** for the JSY to report a load value which is stable and according to what we expect.
  This duration contains the duration for the load to reach its nominal power, plus the duration it takes for the JSY to stabilize its measurements (I think the JSY is using some kind of filtering or averaging).

## Reference material

- [JSY-MK-194T 1.pdf](https://oss.carbou.me/MycilaJSY/JSY-MK-194T%201.pdf) (version 1)
- [JSY-MK-194T 2.pdf](https://oss.carbou.me/MycilaJSY/JSY-MK-194T%202.pdf) (version 2)
- [JSY-MK-194T 3.pdf](https://oss.carbou.me/MycilaJSY/JSY-MK-194T%203.pdf) (version 3)
- [JSY-MK-194T.exe](https://oss.carbou.me/MycilaJSY/JSY-MK-194T.exe) (Utility to configure the JSY with a USB-TTL adapter)
- [ModbusMechanic](https://github.com/SciFiDryer/ModbusMechanic) (can connect and configure JSY with a USB-TTL adapter)
