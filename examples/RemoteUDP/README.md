# Remote JSY with UDP

The JSY can be used connected to an ESP32 to send the JSY data several times per second to a remote server through UDP.
Both devices needs to be connected to the same WiFi network and UDP packets must be allowed.

Screenshot of the ESP32 running the JSY: the `Sender`

![](https://github.com/mathieucarbou/MycilaJSY/assets/61346/3066bf12-31d5-45de-9303-d810f14731d0)

The `Listener` is the same app but will display the data received through UDP.

**Speed**

The JSY Remote through UDP is nearly as fast as having the JSY wired to the ESP.
All changes to the JSY are immediately sent through UDP to the listener at a rate of about **20 messages per second.**
This is the rate at which the JSY usually updates its data.

## How to wire

**Sender**

1. Just attach the JSY module to the ESP32
2. Eventually reconfigure the Serial, TX and RX pin in `Sender.ino`
3. Advise is to power the ESP32 thanks to a MeanWell HDR-15-5 DIN module which is placed just after a 2A breaker.
4. Connect L + N wires to the JSY and set the clamp around the main phase.
5. Ideally, put everything in an isolated box or DIN rail

## Installation with Arduino IDE

_I am not familiar with Arduino IDE because this is not a correct IDE for development._
_Please help contribute to this README if the explanation needs to be improved._

1. You need to install these Arduino dependencies from the library manager or by downloading them:

```c++
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>          // https://github.com/mathieucarbou/AsyncTCP
#include <ESPAsyncWebServer.h> // https://github.com/mathieucarbou/ESPAsyncWebServer
#include <ESPDash.h>           // https://github.com/mathieucarbou/ayushsharma82-ESP-DASH#dev
#include <ElegantOTA.h>        // https://github.com/mathieucarbou/ayushsharma82-ElegantOTA#dev
#include <MycilaESPConnect.h>  // https://github.com/mathieucarbou/MycilaESPConnect
#include <MycilaJSY.h>         // https://github.com/mathieucarbou/MycilaJSY
#include <MycilaLogger.h>      // https://github.com/mathieucarbou/MycilaLogger
#include <MycilaSystem.h>      // https://github.com/mathieucarbou/MycilaSystem
#include <MycilaTaskManager.h> // https://github.com/mathieucarbou/MycilaTaskMonitor
#include <MycilaTaskMonitor.h> // https://github.com/mathieucarbou/MycilaTaskMonitor
#include <WebSerialLite.h>     // https://github.com/mathieucarbou/WebSerialLite
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

2. Compile and deploy to your board.

## Installation with PlatformIO

This is as simple as running:

```bash
pio run -t upload -e <your-board>
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


- To add a password, change the line `#define MYCILA_ADMIN_PASSWORD ""`
