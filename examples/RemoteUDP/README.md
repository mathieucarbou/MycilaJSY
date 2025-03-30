# Remote JSY with UDP

- [Downloads](#downloads)
- [Installation](#installation)
  - [Firmware First Time Installation](#firmware-first-time-installation)
  - [Firmware Update](#firmware-update)
- [Wiring](#wiring)
- [How to build your own version](#how-to-build-your-own-version)
- [Usage](#usage)
- [For developers](#for-developers)
  - [Compilation with PlatformIO](#compilation-with-platformio)
  - [Compilation with Arduino IDE](#compilation-with-arduino-ide)

## Supported models

- JSY-MK-163T
- JSY-MK-194T
- JSY-MK-194G
- JSY-MK-333

The JSY can be used connected to an ESP32 to send the JSY data several times per second to a remote server through UDP.
Both devices needs to be connected to the same WiFi network and UDP packets must be allowed.

Ethernet is also supported.

Screenshot of the ESP32 running the JSY: the `Sender`

![](https://github.com/mathieucarbou/MycilaJSY/assets/61346/3066bf12-31d5-45de-9303-d810f14731d0)

The `Listener` is the same app but will display the data received through UDP.
This examples shows you how to received the JSY statistics from the `Sender` through UDP.

**Speed**

The JSY Remote through UDP is nearly as fast as having the JSY wired to the ESP.
All changes to the JSY are immediately sent through UDP to the listener at a rate of about **3 messages per second** as soon as the JSY registers are updated.
Local JSY readings are made at a rate of 20-25 reads per second.

3 messages per sec is the rate at which the JSY usually updates its data.

## Downloads

Please look at the release section to find the firmwares.

👉 Releases: [https://github.com/mathieucarbou/MycilaJSY/releases](https://github.com/mathieucarbou/MycilaJSY/releases)

**Make sure to download the firmware matching your board.**

Firmware files are named as follow:

- `JSYRemoteUDPSender-BOARD.OTA.bin`: This firmware is used to update through the Web OTA interface
- `JSYRemoteUDPSender-BOARD.FACTORY.bin`: This firmware is used for a first ESP installation, or wen doing a factory reset through USB flashing

## Installation

### Firmware First Time Installation

**The firmware file which must be used for a first installation is the one ending with `.FACTORY.bin`.**

**With `esptool.py` (Linux / MacOS):**

First erase the memory (including the user data):

```bash
esptool.py erase_flash
```

The flash the complete `FACTORY` firmware file:

```bash
esptool.py write_flash 0x0 JSYRemoteUDPSender-BOARD.FACTORY.bin
```

**With [Espressif Flash Tool](https://www.espressif.com/en/support/download/other-tools) (Windows):**

Be careful to not forget the `0`!

![Espressif Flash Tool](https://yasolr.carbou.me/assets/img/screenshots/Espressif_Flash_Tool.png)

### Firmware Update

To update the firmware through OTA, please follow these steps:

1. First [download the new firmware](download). The firmware file which must be used is the one ending with `.OTA.bin` (`JSYRemoteUDPSender-BOARD.OTA.bin`)

2. Go to the update page at `http://<device-ip>/update`

3. Upload the new OTA firmware

4. The device will reboot

## Wiring

After the application is flashed, it will restart and you will see a log line in the Serial console telling you the RX and TX pins used for the JSY.

The default GPIO are:

| Board               | Serial2 RX (JSY TX) | Serial2 TX (JSY RX) |
| :------------------ | :-----------------: | :-----------------: |
| denky_d4            |         22          |         21          |
| esp32-c3-devkitc-02 |         18          |         19          |
| esp32-c6-devkitc-1  |          4          |          5          |
| esp32-gateway       |         16          |         17          |
| esp32-poe           |         35          |         33          |
| esp32-s2-saola-1    |         18          |         17          |
| esp32-s3-devkitc-1  |         16          |         17          |
| esp32dev            |         16          |         17          |
| esp32s3box          |         19          |         20          |
| lilygo_eth_lite_s3  |         17          |         18          |
| nodemcu-32s         |         16          |         17          |
| tinypico            |          4          |         25          |
| wipy3               |          4          |         25          |
| wt32-eth01          |          5          |         17          |

**Electric wiring for JSY boards with 2 channels:**

- Channel 1 (CT1): anything
- Channel 2 (CT2): the grid should go through it

Reason is that on some JSY, channel 1 is a tore on the board while channel 2 is always a clamp.
This is easier to put a clamp around the grid wire.

## How to build your own version

Run at the root of the project:

```bash
PIO_BOARD=esp32dev pio run -e ci -d examples/RemoteUDP/Sender
```

And follow the instructions to flash the firmware shown at the end of the build.

`esp32dev` can be replaced with any board type

If you need to specify your own pin, or change the port, you can do:

```bash
# use default mapping in code
PIO_BOARD=esp32dev PLATFORMIO_BUILD_SRC_FLAGS="-DMYCILA_JSY_SERIAL=Serial2 -DMYCILA_JSY_RX=RX2 -DMYCILA_JSY_TX=TX2 -DMYCILA_UDP_PORT=53964" pio run -e ci -d examples/RemoteUDP/Sender

# use custom mapping in code
PIO_BOARD=esp32dev PLATFORMIO_BUILD_SRC_FLAGS="-DMYCILA_JSY_SERIAL=Serial2 -DMYCILA_JSY_RX=4 -DMYCILA_JSY_TX=25 -DMYCILA_UDP_PORT=53964" pio run -e ci -d examples/RemoteUDP/Sender
```

## Usage

Quick start is simple: the device will open an access point, you need to connect to it and choose the WiFI to join.

Once the device has joined the WiFi and is connected to a JSY, you will see the JSY data on the dashboard of this device, but also the data will flow on client-side: the client will receive the data each 500ms and graphs will be updated in real-time.

- To look at the logs, go to: `http://<device-ip>/console`

![](https://github.com/mathieucarbou/MycilaJSY/assets/61346/a2604cfe-c31b-4c4d-bf00-764961aee756)

- To update the firmware, go to: `http://<device-ip>/update`

![](https://github.com/mathieucarbou/MycilaJSY/assets/61346/8a4c03d8-5fd4-4580-ae46-a70dc5807327)

- Some statistics and device / network information are available:

![](https://github.com/mathieucarbou/MycilaJSY/assets/61346/1943a697-f108-4cdc-a886-98cb3748af24)

- An API is available at: `http://<device-ip>/api/jsy`

```json
{
  "enabled": true,
  "address": 1,
  "speed": 38400,
  "model": 404,
  "model_name": "JSY-MK-194",
  "time": 116089,
  "current1": 0,
  "current2": 3.99,
  "energy_returned1": 0,
  "energy_returned2": 0,
  "energy1": 0,
  "energy2": 0.371,
  "frequency": 49.98,
  "power_factor1": 0,
  "power_factor2": 0.719,
  "power1": 0,
  "power2": 666.8345,
  "voltage1": 232.1408,
  "voltage2": 232.1408
}
```

## For developers

- You can add a password by changing the line `#define MYCILA_ADMIN_PASSWORD ""`
- You can change the definitions for `MYCILA_JSY_RX`, `MYCILA_JSY_TX` and `MYCILA_JSY_SERIAL`

### Compilation with PlatformIO

This is as simple as running:

```bash
PIO_BOARD="<your-board>" pio run -e ci
```

### Compilation with Arduino IDE

_I am not familiar with Arduino IDE because this is not a correct IDE for development._
_Please help contribute to this README if the explanation needs to be improved._

You need to install these Arduino dependencies from the library manager or by downloading them:

```c++
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>             // https://github.com/ESP32Async/AsyncTCP
#include <ESPAsyncWebServer.h>    // https://github.com/ESP32Async/ESPAsyncWebServer
#include <ElegantOTA.h>           // https://github.com/mathieucarbou/ayushsharma82-ElegantOTA#dev
#include <ESPDash.h>              // https://github.com/mathieucarbou/ayushsharma82-ESP-dASH#dev-v5-v5
#include <FastCRC32.h>            // https://github.com/RobTillaart/CRC
#include <MycilaCircularBuffer.h> // https://github.com/mathieucarbou/MycilaUtilities
#include <MycilaESPConnect.h>     // https://github.com/mathieucarbou/MycilaESPConnect
#include <MycilaJSY.h>            // https://github.com/mathieucarbou/MycilaJSY
#include <MycilaLogger.h>         // https://github.com/mathieucarbou/MycilaLogger
#include <MycilaSystem.h>         // https://github.com/mathieucarbou/MycilaSystem
#include <MycilaTaskManager.h>    // https://github.com/mathieucarbou/MycilaTaskManager
#include <MycilaTime.h>           // https://github.com/mathieucarbou/MycilaUtilities
#include <MycilaWebSerial.h>      // https://github.com/mathieucarbou/MycilaWebSerial
```

The program also uses these Arduino libraries:

```
DNSServer
ESP32 Async UDP
ESPmDNS
FS
Networking
LittleFS
WiFi
Ticker
Update
```

The compile and upload.
