#include <HardwareSerial.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif

#if SOC_UART_HP_NUM < 3
  #ifndef Serial2
    #define Serial2 Serial1
  #endif
  #ifndef RX2
    #define RX2 RX1
  #endif
  #ifndef TX2
    #define TX2 TX1
  #endif
#endif

#ifndef CONFIG_ASYNC_TCP_MAX_ACK_TIME
  #define CONFIG_ASYNC_TCP_MAX_ACK_TIME 3000
#endif

#ifndef CONFIG_ASYNC_TCP_PRIORITY
  #define CONFIG_ASYNC_TCP_PRIORITY 10
#endif

#ifndef CONFIG_ASYNC_TCP_QUEUE_SIZE
  #define CONFIG_ASYNC_TCP_QUEUE_SIZE 128
#endif

#ifndef CONFIG_ASYNC_TCP_RUNNING_CORE
  #define CONFIG_ASYNC_TCP_RUNNING_CORE 1
#endif

#ifndef CONFIG_ASYNC_TCP_STACK_SIZE
  #define CONFIG_ASYNC_TCP_STACK_SIZE 4096
#endif

#ifndef ELEGANTOTA_USE_ASYNC_WEBSERVER
  #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#endif

#ifndef MYCILA_JSON_SUPPORT
  #define MYCILA_JSON_SUPPORT 1
#endif

#ifndef MYCILA_LOGGER_SUPPORT
  #define MYCILA_LOGGER_SUPPORT 1
#endif

#ifndef WS_MAX_QUEUED_MESSAGES
  #define WS_MAX_QUEUED_MESSAGES 64
#endif

#ifndef WSL_HIGH_PERF
  #define WSL_HIGH_PERF 1
#endif

#ifndef MYCILA_JSY_SERIAL
  #define MYCILA_JSY_SERIAL Serial2
#endif

#ifndef MYCILA_JSY_RX
  #define MYCILA_JSY_RX RX2
#endif

#ifndef MYCILA_JSY_TX
  #define MYCILA_JSY_TX TX2
#endif

#ifndef MYCILA_ADMIN_PASSWORD
  #define MYCILA_ADMIN_PASSWORD ""
#endif

#ifndef MYCILA_UDP_PORT
  #define MYCILA_UDP_PORT 53964
#endif

#define MYCILA_ADMIN_USERNAME "admin"
#define MYCILA_APP_NAME       "JSY Remote UDP Sender"
#define MYCILA_GRAPH_POINTS   120
// #define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x01 // old json format
#define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x02 // supports all JSY models
#define MYCILA_UDP_SEND_INTERVAL_MS  200
#define MYCILA_UDP_SEND_RATE_WINDOW  50
#define TAG                          "JSY-UDP"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <ESPmDNS.h>

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>             // https://github.com/mathieucarbou/AsyncTCP
#include <ESPAsyncWebServer.h>    // https://github.com/mathieucarbou/ESPAsyncWebServer
#include <ESPDash.h>              // https://github.com/ayushsharma82/ESP-DASH
#include <ElegantOTA.h>           // https://github.com/ayushsharma82/ElegantOTA
#include <FastCRC32.h>            // https://github.com/RobTillaart/CRC
#include <MycilaCircularBuffer.h> // https://github.com/mathieucarbou/MycilaUtilities
#include <MycilaESPConnect.h>     // https://github.com/mathieucarbou/MycilaESPConnect
#include <MycilaJSY.h>            // https://github.com/mathieucarbou/MycilaJSY
#include <MycilaLogger.h>         // https://github.com/mathieucarbou/MycilaLogger
#include <MycilaSystem.h>         // https://github.com/mathieucarbou/MycilaSystem
#include <MycilaTaskManager.h>    // https://github.com/mathieucarbou/MycilaTaskManager
#include <MycilaTime.h>           // https://github.com/mathieucarbou/MycilaUtilities
#include <MycilaWebSerial.h>      // https://github.com/mathieucarbou/MycilaWebSerial

AsyncUDP udp;
AsyncWebServer webServer(80);
Mycila::ESPConnect espConnect(webServer);
ESPDash dashboard = ESPDash(&webServer, "/dashboard", false);
Mycila::JSY jsy;
Mycila::Logger logger;
Mycila::TaskManager coreTaskManager("core");
Mycila::TaskManager jsyTaskManager("jsy");

Statistic networkHostname = Statistic(&dashboard, "Network Hostname");
Statistic networkInterface = Statistic(&dashboard, "Network Interface");
Statistic networkAPIP = Statistic(&dashboard, "Network Access Point IP Address");
Statistic networkAPMAC = Statistic(&dashboard, "Network Access Point MAC Address");
Statistic networkEthIP = Statistic(&dashboard, "Network Ethernet IP Address");
Statistic networkEthMAC = Statistic(&dashboard, "Network Ethernet MAC Address");
Statistic networkWiFiIP = Statistic(&dashboard, "Network WiFi IP Address");
Statistic networkWiFiMAC = Statistic(&dashboard, "Network WiFi MAC Address");
Statistic networkWiFiSSID = Statistic(&dashboard, "Network WiFi SSID");
Statistic networkWiFiRSSI = Statistic(&dashboard, "Network WiFi RSSI");
Statistic networkWiFiSignal = Statistic(&dashboard, "Network WiFi Signal");
Statistic uptime = Statistic(&dashboard, "Uptime");
Statistic version = Statistic(&dashboard, "Version");

Card udpSendEnabledCard = Card(&dashboard, BUTTON_CARD, "Enable UDP Transfer");
Card restart = Card(&dashboard, BUTTON_CARD, "Restart");
Card energyReset = Card(&dashboard, BUTTON_CARD, "Reset Energy");
Card reset = Card(&dashboard, BUTTON_CARD, "Factory Reset");

Card jsyModelCard = Card(&dashboard, GENERIC_CARD, "Model");
Card messageRateCard = Card(&dashboard, GENERIC_CARD, "Message Rate", "msg/s");
Card dataRateCard = Card(&dashboard, GENERIC_CARD, "Data Rate", "bytes/s");
Card frequency = Card(&dashboard, GENERIC_CARD, "Frequency", "Hz");

// JSY-MK-163
Card jsy163Voltage = Card(&dashboard, GENERIC_CARD, "Voltage", "V");
Card jsy163current = Card(&dashboard, GENERIC_CARD, "Current", "A");
Card jsy163PowerFactor = Card(&dashboard, GENERIC_CARD, "Power Factor");
Card jsy163ActivePower = Card(&dashboard, GENERIC_CARD, "Active Power", "W");
Card jsy163ApparentPower = Card(&dashboard, GENERIC_CARD, "Apparent Power", "VA");
Card jsy163ReactivePower = Card(&dashboard, GENERIC_CARD, "ReactivePower", "VAr");
Card jsy163ActiveEnergy = Card(&dashboard, GENERIC_CARD, "Active Energy", "kWh");
Card jsy163ActiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Active Energy Imported", "kWh");
Card jsy163ActiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Active Energy Returned", "kWh");
Chart jsy163ActivePowerHistory = Chart(&dashboard, BAR_CHART, "Active Power (W)");

// JSY-MK-194
Card jsy194Channel1Voltage = Card(&dashboard, GENERIC_CARD, "Channel 1 Voltage", "V");
Card jsy194Channel1Current = Card(&dashboard, GENERIC_CARD, "Channel 1 Current", "A");
Card jsy194Channel1PowerFactor = Card(&dashboard, GENERIC_CARD, "Channel 1 Power Factor");
Card jsy194Channel1ActivePower = Card(&dashboard, GENERIC_CARD, "Channel 1 Active Power", "W");
Card jsy194Channel1ApparentPower = Card(&dashboard, GENERIC_CARD, "Channel 1 Apparent Power", "VA");
Card jsy194Channel1ReactivePower = Card(&dashboard, GENERIC_CARD, "Channel 1 ReactivePower", "VAr");
Card jsy194Channel1ActiveEnergy = Card(&dashboard, GENERIC_CARD, "Channel 1 Active Energy", "kWh");
Card jsy194Channel1ActiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Channel 1 Active Energy Imported", "kWh");
Card jsy194Channel1ActiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Channel 1 Active Energy Returned", "kWh");
Card jsy194Channel2Voltage = Card(&dashboard, GENERIC_CARD, "Channel 2 Voltage", "V");
Card jsy194Channel2Current = Card(&dashboard, GENERIC_CARD, "Channel 2 Current", "A");
Card jsy194Channel2PowerFactor = Card(&dashboard, GENERIC_CARD, "Channel 2 Power Factor");
Card jsy194Channel2ActivePower = Card(&dashboard, GENERIC_CARD, "Channel 2 Active Power", "W");
Card jsy194Channel2ApparentPower = Card(&dashboard, GENERIC_CARD, "Channel 2 Apparent Power", "VA");
Card jsy194Channel2ReactivePower = Card(&dashboard, GENERIC_CARD, "Channel 2 ReactivePower", "VAr");
Card jsy194Channel2ActiveEnergy = Card(&dashboard, GENERIC_CARD, "Channel 2 Active Energy", "kWh");
Card jsy194Channel2ActiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Channel 2 Active Energy Imported", "kWh");
Card jsy194Channel2ActiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Channel 2 Active Energy Returned", "kWh");
Chart jsy194Channel1ActivePowerHistory = Chart(&dashboard, BAR_CHART, "Channel 1 Active Power (W)");
Chart jsy194Channel2ActivePowerHistory = Chart(&dashboard, BAR_CHART, "Channel 2 Active Power (W)");

// JSY-MK-333
Card jsy333PhaseAVoltage = Card(&dashboard, GENERIC_CARD, "Phase A Voltage", "V");
Card jsy333PhaseACurrent = Card(&dashboard, GENERIC_CARD, "Phase A Current", "A");
Card jsy333PhaseAPowerFactor = Card(&dashboard, GENERIC_CARD, "Phase A Power Factor");
Card jsy333PhaseAActivePower = Card(&dashboard, GENERIC_CARD, "Phase A Active Power", "W");
Card jsy333PhaseAApparentPower = Card(&dashboard, GENERIC_CARD, "Phase A Apparent Power", "VA");
Card jsy333PhaseAReactivePower = Card(&dashboard, GENERIC_CARD, "Phase A ReactivePower", "VAr");
Card jsy333PhaseAActiveEnergy = Card(&dashboard, GENERIC_CARD, "Phase A Active Energy", "kWh");
Card jsy333PhaseAActiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Phase A Active Energy Imported", "kWh");
Card jsy333PhaseAActiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Phase A Active Energy Returned", "kWh");
Card jsy333PhaseAReactiveEnergy = Card(&dashboard, GENERIC_CARD, "Phase A Reactive Energy", "kVArh");
Card jsy333PhaseAReactiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Phase A Reactive Energy Imported", "kVArh");
Card jsy333PhaseAReactiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Phase A Reactive Energy Returned", "kVArh");
Card jsy333PhaseAApparentEnergy = Card(&dashboard, GENERIC_CARD, "Phase A Apparent Energy", "kVAh");
Card jsy333PhaseBVoltage = Card(&dashboard, GENERIC_CARD, "Phase B Voltage", "V");
Card jsy333PhaseBCurrent = Card(&dashboard, GENERIC_CARD, "Phase B Current", "A");
Card jsy333PhaseBPowerFactor = Card(&dashboard, GENERIC_CARD, "Phase B Power Factor");
Card jsy333PhaseBActivePower = Card(&dashboard, GENERIC_CARD, "Phase B Active Power", "W");
Card jsy333PhaseBApparentPower = Card(&dashboard, GENERIC_CARD, "Phase B Apparent Power", "VA");
Card jsy333PhaseBReactivePower = Card(&dashboard, GENERIC_CARD, "Phase B ReactivePower", "VAr");
Card jsy333PhaseBActiveEnergy = Card(&dashboard, GENERIC_CARD, "Phase B Active Energy", "kWh");
Card jsy333PhaseBActiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Phase B Active Energy Imported", "kWh");
Card jsy333PhaseBActiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Phase B Active Energy Returned", "kWh");
Card jsy333PhaseBReactiveEnergy = Card(&dashboard, GENERIC_CARD, "Phase B Reactive Energy", "kVArh");
Card jsy333PhaseBReactiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Phase B Reactive Energy Imported", "kVArh");
Card jsy333PhaseBReactiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Phase B Reactive Energy Returned", "kVArh");
Card jsy333PhaseBApparentEnergy = Card(&dashboard, GENERIC_CARD, "Phase B Apparent Energy", "kVAh");
Card jsy333PhaseCVoltage = Card(&dashboard, GENERIC_CARD, "Phase C Voltage", "V");
Card jsy333PhaseCCurrent = Card(&dashboard, GENERIC_CARD, "Phase C Current", "A");
Card jsy333PhaseCPowerFactor = Card(&dashboard, GENERIC_CARD, "Phase C Power Factor");
Card jsy333PhaseCActivePower = Card(&dashboard, GENERIC_CARD, "Phase C Active Power", "W");
Card jsy333PhaseCApparentPower = Card(&dashboard, GENERIC_CARD, "Phase C Apparent Power", "VA");
Card jsy333PhaseCReactivePower = Card(&dashboard, GENERIC_CARD, "Phase C ReactivePower", "VAr");
Card jsy333PhaseCActiveEnergy = Card(&dashboard, GENERIC_CARD, "Phase C Active Energy", "kWh");
Card jsy333PhaseCActiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Phase C Active Energy Imported", "kWh");
Card jsy333PhaseCActiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Phase C Active Energy Returned", "kWh");
Card jsy333PhaseCReactiveEnergy = Card(&dashboard, GENERIC_CARD, "Phase C Reactive Energy", "kVArh");
Card jsy333PhaseCReactiveEnergyImported = Card(&dashboard, GENERIC_CARD, "Phase C Reactive Energy Imported", "kVArh");
Card jsy333PhaseCReactiveEnergyReturned = Card(&dashboard, GENERIC_CARD, "Phase C Reactive Energy Returned", "kVArh");
Card jsy333PhaseCApparentEnergy = Card(&dashboard, GENERIC_CARD, "Phase C Apparent Energy", "kVAh");
Chart jsy333PhaseAActivePowerHistory = Chart(&dashboard, BAR_CHART, "Phase A Active Power (W)");
Chart jsy333PhaseBActivePowerHistory = Chart(&dashboard, BAR_CHART, "Phase B Active Power (W)");
Chart jsy333PhaseCActivePowerHistory = Chart(&dashboard, BAR_CHART, "Phase C Active Power (W)");

int historyX[MYCILA_GRAPH_POINTS] = {0};
int power1HistoryY[MYCILA_GRAPH_POINTS] = {0};
int power2HistoryY[MYCILA_GRAPH_POINTS] = {0};
int power3HistoryY[MYCILA_GRAPH_POINTS] = {0};

String hostname;
String ssid;

bool udpSendEnabled = true;
uint16_t jsyModel = MYCILA_JSY_MK_UNKNOWN;

// circular buffer for msg rate
Mycila::CircularBuffer<float, MYCILA_UDP_SEND_RATE_WINDOW> messageRateBuffer;
volatile float messageRate = 0;

// circular buffer for data rate
Mycila::CircularBuffer<uint32_t, MYCILA_UDP_SEND_RATE_WINDOW> dataRateBuffer;
volatile uint32_t dataRate = 0;

Mycila::Task jsyTask("JSY", [](void* params) { jsy.read(); });
Mycila::Task networkManagerTask("ESPConnect", [](void* params) { espConnect.loop(); });

Mycila::Task profilerTask("Profiler", [](void* params) {
  jsyTaskManager.log();
});

Mycila::Task networkUpTask("Network UP", Mycila::TaskType::ONCE, [](void* params) {
  logger.info(TAG, "Enable Network Services...");

  // Web server
  logger.info(TAG, "Enable Web Server...");
  webServer.begin();
  webServer.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404);
  });

  // mDNS
#ifndef ESPCONNECT_NO_MDNS
  logger.info(TAG, "Enable mDNS...");
  MDNS.addService("http", "tcp", 80);
#endif
});

Mycila::Task otaTask("OTA", Mycila::TaskType::ONCE, [](void* params) {
  logger.info(TAG, "Preparing OTA update...");
  udpSendEnabled = false;
  jsy.end();
});

Mycila::Task restartTask("Restart", Mycila::TaskType::ONCE, [](void* params) {
  logger.warn(TAG, "Restarting " MYCILA_APP_NAME "...");
  Mycila::System::restart(500);
});

Mycila::Task dashboardTask("Dashboard", [](void* params) {
  Mycila::ESPConnect::Mode mode = espConnect.getMode();

  networkAPIP.set(espConnect.getIPAddress(Mycila::ESPConnect::Mode::AP).toString().c_str());
  networkEthIP.set(espConnect.getIPAddress(Mycila::ESPConnect::Mode::ETH).toString().c_str());
  networkInterface.set(mode == Mycila::ESPConnect::Mode::AP ? "AP" : (mode == Mycila::ESPConnect::Mode::STA ? "WiFi" : (mode == Mycila::ESPConnect::Mode::ETH ? "Ethernet" : "")));
  networkWiFiIP.set(espConnect.getIPAddress(Mycila::ESPConnect::Mode::STA).toString().c_str());
  networkWiFiRSSI.set((String(espConnect.getWiFiRSSI()) + " dBm").c_str());
  networkWiFiSignal.set((String(espConnect.getWiFiSignalQuality()) + " %").c_str());
  networkWiFiSSID.set(espConnect.getWiFiSSID().c_str());
  uptime.set(Mycila::Time::toDHHMMSS(Mycila::System::getUptime()).c_str());

  messageRateCard.update(messageRate);
  dataRateCard.update(static_cast<int>(dataRate));
  udpSendEnabledCard.update(udpSendEnabled);
  frequency.update(jsy.data.frequency);

  switch (jsyModel) {
    case MYCILA_JSY_MK_163: {
      jsy163Voltage.update(jsy.data.single().voltage);
      jsy163current.update(jsy.data.single().current);
      jsy163PowerFactor.update(jsy.data.single().powerFactor);
      jsy163ActivePower.update(jsy.data.single().activePower);
      jsy163ApparentPower.update(jsy.data.single().apparentPower);
      jsy163ReactivePower.update(jsy.data.single().reactivePower);
      jsy163ActiveEnergy.update(jsy.data.single().activeEnergy);
      jsy163ActiveEnergyImported.update(jsy.data.single().activeEnergyImported);
      jsy163ActiveEnergyReturned.update(jsy.data.single().activeEnergyReturned);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.phaseA().activePower);

      // update charts
      jsy163ActivePowerHistory.updateY(power1HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    case MYCILA_JSY_MK_194: {
      jsy194Channel1Voltage.update(jsy.data.channel1().voltage);
      jsy194Channel1Current.update(jsy.data.channel1().current);
      jsy194Channel1PowerFactor.update(jsy.data.channel1().powerFactor);
      jsy194Channel1ActivePower.update(jsy.data.channel1().activePower);
      jsy194Channel1ApparentPower.update(jsy.data.channel1().apparentPower);
      jsy194Channel1ReactivePower.update(jsy.data.channel1().reactivePower);
      jsy194Channel1ActiveEnergy.update(jsy.data.channel1().activeEnergy);
      jsy194Channel1ActiveEnergyImported.update(jsy.data.channel1().activeEnergyImported);
      jsy194Channel1ActiveEnergyReturned.update(jsy.data.channel1().activeEnergyReturned);

      jsy194Channel2Voltage.update(jsy.data.channel2().voltage);
      jsy194Channel2Current.update(jsy.data.channel2().current);
      jsy194Channel2PowerFactor.update(jsy.data.channel2().powerFactor);
      jsy194Channel2ActivePower.update(jsy.data.channel2().activePower);
      jsy194Channel2ApparentPower.update(jsy.data.channel2().apparentPower);
      jsy194Channel2ReactivePower.update(jsy.data.channel2().reactivePower);
      jsy194Channel2ActiveEnergy.update(jsy.data.channel2().activeEnergy);
      jsy194Channel2ActiveEnergyImported.update(jsy.data.channel2().activeEnergyImported);
      jsy194Channel2ActiveEnergyReturned.update(jsy.data.channel2().activeEnergyReturned);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
        power2HistoryY[i] = power2HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.channel1().activePower);
      power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.channel2().activePower);

      // update charts
      jsy194Channel1ActivePowerHistory.updateY(power1HistoryY, MYCILA_GRAPH_POINTS);
      jsy194Channel2ActivePowerHistory.updateY(power2HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    case MYCILA_JSY_MK_333: {
      jsy333PhaseAVoltage.update(jsy.data.phaseA().voltage);
      jsy333PhaseACurrent.update(jsy.data.phaseA().current);
      jsy333PhaseAPowerFactor.update(jsy.data.phaseA().powerFactor);
      jsy333PhaseAActivePower.update(jsy.data.phaseA().activePower);
      jsy333PhaseAApparentPower.update(jsy.data.phaseA().apparentPower);
      jsy333PhaseAReactivePower.update(jsy.data.phaseA().reactivePower);
      jsy333PhaseAActiveEnergy.update(jsy.data.phaseA().activeEnergy);
      jsy333PhaseAActiveEnergyImported.update(jsy.data.phaseA().activeEnergyImported);
      jsy333PhaseAActiveEnergyReturned.update(jsy.data.phaseA().activeEnergyReturned);
      jsy333PhaseAReactiveEnergy.update(jsy.data.phaseA().reactiveEnergy);
      jsy333PhaseAReactiveEnergyImported.update(jsy.data.phaseA().reactiveEnergyImported);
      jsy333PhaseAReactiveEnergyReturned.update(jsy.data.phaseA().reactiveEnergyReturned);
      jsy333PhaseAApparentEnergy.update(jsy.data.phaseA().apparentEnergy);

      jsy333PhaseBVoltage.update(jsy.data.phaseB().voltage);
      jsy333PhaseBCurrent.update(jsy.data.phaseB().current);
      jsy333PhaseBPowerFactor.update(jsy.data.phaseB().powerFactor);
      jsy333PhaseBActivePower.update(jsy.data.phaseB().activePower);
      jsy333PhaseBApparentPower.update(jsy.data.phaseB().apparentPower);
      jsy333PhaseBReactivePower.update(jsy.data.phaseB().reactivePower);
      jsy333PhaseBActiveEnergy.update(jsy.data.phaseB().activeEnergy);
      jsy333PhaseBActiveEnergyImported.update(jsy.data.phaseB().activeEnergyImported);
      jsy333PhaseBActiveEnergyReturned.update(jsy.data.phaseB().activeEnergyReturned);
      jsy333PhaseBReactiveEnergy.update(jsy.data.phaseB().reactiveEnergy);
      jsy333PhaseBReactiveEnergyImported.update(jsy.data.phaseB().reactiveEnergyImported);
      jsy333PhaseBReactiveEnergyReturned.update(jsy.data.phaseB().reactiveEnergyReturned);
      jsy333PhaseBApparentEnergy.update(jsy.data.phaseB().apparentEnergy);

      jsy333PhaseCVoltage.update(jsy.data.phaseC().voltage);
      jsy333PhaseCCurrent.update(jsy.data.phaseC().current);
      jsy333PhaseCPowerFactor.update(jsy.data.phaseC().powerFactor);
      jsy333PhaseCActivePower.update(jsy.data.phaseC().activePower);
      jsy333PhaseCApparentPower.update(jsy.data.phaseC().apparentPower);
      jsy333PhaseCReactivePower.update(jsy.data.phaseC().reactivePower);
      jsy333PhaseCActiveEnergy.update(jsy.data.phaseC().activeEnergy);
      jsy333PhaseCActiveEnergyImported.update(jsy.data.phaseC().activeEnergyImported);
      jsy333PhaseCActiveEnergyReturned.update(jsy.data.phaseC().activeEnergyReturned);
      jsy333PhaseCReactiveEnergy.update(jsy.data.phaseC().reactiveEnergy);
      jsy333PhaseCReactiveEnergyImported.update(jsy.data.phaseC().reactiveEnergyImported);
      jsy333PhaseCReactiveEnergyReturned.update(jsy.data.phaseC().reactiveEnergyReturned);
      jsy333PhaseCApparentEnergy.update(jsy.data.phaseC().apparentEnergy);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
        power2HistoryY[i] = power2HistoryY[i + 1];
        power3HistoryY[i] = power3HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.phaseA().activePower);
      power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.phaseB().activePower);
      power3HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.phaseC().activePower);

      // update charts
      jsy333PhaseAActivePowerHistory.updateY(power1HistoryY, MYCILA_GRAPH_POINTS);
      jsy333PhaseBActivePowerHistory.updateY(power2HistoryY, MYCILA_GRAPH_POINTS);
      jsy333PhaseCActivePowerHistory.updateY(power3HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    default:
      break;
  }

  dashboard.sendUpdates();
});

void setup() {
  Serial.begin(115200);

#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  // hostname
  hostname = "jsy-" + Mycila::System::getChipIDStr();
  hostname.toLowerCase();
  ssid = "JSY-" + Mycila::System::getChipIDStr();

  // logging
  esp_log_level_set("*", static_cast<esp_log_level_t>(ARDUHAL_LOG_LEVEL_DEBUG));
  logger.setLevel(ARDUHAL_LOG_LEVEL_DEBUG);
  logger.forwardTo(&Serial);
  logger.warn(TAG, "Booting " MYCILA_APP_NAME "...");

  logger.warn(TAG, "Initializing " MYCILA_APP_NAME "...");

  // system
  Mycila::System::init();

  // tasks
  dashboardTask.setEnabledWhen([]() { return espConnect.isConnected() && !dashboard.isAsyncAccessInProgress(); });
  dashboardTask.setInterval(1000 * Mycila::TaskDuration::MILLISECONDS);
  dashboardTask.setManager(coreTaskManager);
  jsyTask.setEnabledWhen([]() { return jsy.isEnabled(); });
  jsyTask.setManager(jsyTaskManager);
  Mycila::TaskManager::configureWDT();
  networkManagerTask.setManager(coreTaskManager);
  networkUpTask.setManager(coreTaskManager);
  otaTask.setManager(coreTaskManager);
  profilerTask.setInterval(10 * Mycila::TaskDuration::SECONDS);
  profilerTask.setManager(coreTaskManager);
  restartTask.setManager(coreTaskManager);

  // profiling
  jsyTask.enableProfiling(10, Mycila::TaskTimeUnit::MILLISECONDS);

  // WebSerial
  WebSerial.setAuthentication(MYCILA_ADMIN_USERNAME, MYCILA_ADMIN_PASSWORD);
  WebSerial.begin(&webServer, "/console");
  logger.forwardTo(&WebSerial);

  // ElegantOTA
  ElegantOTA.setAutoReboot(false);
  ElegantOTA.onStart([]() { otaTask.resume(); });
  ElegantOTA.onEnd([](bool success) {
    if (success) {
      logger.info(TAG, "OTA Update Success! Restarting...");
    } else {
      logger.error(TAG, "OTA Failed! Restarting...");
    }
    restartTask.resume();
  });
  ElegantOTA.begin(&webServer, MYCILA_ADMIN_USERNAME, MYCILA_ADMIN_PASSWORD);

  // Dashboard
  webServer.rewrite("/", "/dashboard").setFilter([](AsyncWebServerRequest* request) { return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

  webServer.on("/api/jsy", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    jsy.toJson(response->getRoot());
    response->setLength();
    request->send(response);
  });

  webServer.on("/api/restart", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200);
    ESP.restart();
  });

  version.set(MYCILA_JSY_VERSION);

  restart.attachCallback([](int32_t value) { restartTask.resume(); });

  reset.attachCallback([](int32_t value) {
    espConnect.clearConfiguration();
    restartTask.resume();
  });

  for (int i = 0; i < MYCILA_GRAPH_POINTS; i++)
    historyX[i] = i - MYCILA_GRAPH_POINTS;
  jsy163ActivePowerHistory.updateX(historyX, MYCILA_GRAPH_POINTS);
  jsy194Channel1ActivePowerHistory.updateX(historyX, MYCILA_GRAPH_POINTS);
  jsy194Channel2ActivePowerHistory.updateX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseAActivePowerHistory.updateX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseBActivePowerHistory.updateX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseCActivePowerHistory.updateX(historyX, MYCILA_GRAPH_POINTS);

  energyReset.attachCallback([](int32_t value) {
    jsy.resetEnergy();
    energyReset.update(0);
    dashboard.refreshCard(&energyReset);
  });

  udpSendEnabledCard.attachCallback([](int32_t value) {
    udpSendEnabled = value != 0;
    udpSendEnabledCard.update(udpSendEnabled);
    dashboard.refreshCard(&udpSendEnabledCard);
  });

  dashboard.onBeforeUpdate([](bool changes_only) {
    if (!changes_only) {
      logger.debug(TAG, "Dashboard refresh requested");
      jsyModelCard.update(jsyModel == MYCILA_JSY_MK_UNKNOWN ? "Unknown" : jsy.getModelName());
      networkAPMAC.set(espConnect.getMACAddress(Mycila::ESPConnect::Mode::AP).c_str());
      networkEthMAC.set(espConnect.getMACAddress(Mycila::ESPConnect::Mode::ETH).isEmpty() ? "N/A" : espConnect.getMACAddress(Mycila::ESPConnect::Mode::ETH).c_str());
      networkHostname.set(hostname.c_str());
      networkWiFiMAC.set(espConnect.getMACAddress(Mycila::ESPConnect::Mode::STA).c_str());
    }
  });
  dashboard.setAuthentication(MYCILA_ADMIN_USERNAME, MYCILA_ADMIN_PASSWORD);
  dashboard.refreshLayout();
  dashboardTask.forceRun();

  // jsy
  jsy.setCallback([](const Mycila::JSY::EventType eventType) {
    if (!udpSendEnabled) {
      messageRate = 0;
      dataRate = 0;
      return;
    }

    if (eventType == Mycila::JSY::EventType::EVT_CHANGE) {

      Mycila::ESPConnect::Mode mode = espConnect.getMode();

      if (mode != Mycila::ESPConnect::Mode::NONE) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        jsy.toJson(root);

        // buffer[0] == MYCILA_UDP_MSG_TYPE_JSY_DATA (1)
        // buffer[1] == size_t (4)
        // buffer[5] == MsgPack (?)
        // buffer[5 + size] == CRC32 (4)
        uint32_t size = measureMsgPack(doc);
        uint32_t packetSize = size + 9;
        uint8_t buffer[packetSize];
        buffer[0] = MYCILA_UDP_MSG_TYPE_JSY_DATA;
        memcpy(buffer + 1, &size, 4);
        serializeMsgPack(root, buffer + 5, size);

        // crc32
        FastCRC32 crc32;
        crc32.add(buffer, size + 5);
        uint32_t crc = crc32.calc();
        memcpy(buffer + size + 5, &crc, 4);

        if (packetSize > CONFIG_TCP_MSS) {
          ESP_LOGE(TAG, "Packet size too big: %" PRIu32 " / %d bytes", packetSize, CONFIG_TCP_MSS);
          messageRate = 0;
          dataRate = 0;
          return;
        }

        // send
        switch (mode) {
          case Mycila::ESPConnect::Mode::AP:
            udp.broadcastTo(buffer, packetSize, MYCILA_UDP_PORT, tcpip_adapter_if_t::TCPIP_ADAPTER_IF_AP);
            break;
          case Mycila::ESPConnect::Mode::STA:
            udp.broadcastTo(buffer, packetSize, MYCILA_UDP_PORT, tcpip_adapter_if_t::TCPIP_ADAPTER_IF_STA);
            break;
          case Mycila::ESPConnect::Mode::ETH:
            udp.broadcastTo(buffer, packetSize, MYCILA_UDP_PORT, tcpip_adapter_if_t::TCPIP_ADAPTER_IF_ETH);
            break;
          default:
            break;
        }

        // update rate
        messageRateBuffer.add(millis() / 1000.0f);
        float diff = messageRateBuffer.diff();
        float count = messageRateBuffer.count();
        messageRate = diff == 0 ? 0 : count / diff;
        dataRateBuffer.add(packetSize);
        dataRate = diff == 0 ? 0 : dataRateBuffer.sum() / diff;
      } else {
        messageRate = 0;
        dataRate = 0;
      }
    }
  });
  jsy.begin(MYCILA_JSY_SERIAL, MYCILA_JSY_RX, MYCILA_JSY_TX);
  if (jsy.isEnabled() && jsy.getBaudRate() != jsy.getMaxAvailableBaudRate())
    jsy.setBaudRate(jsy.getMaxAvailableBaudRate());

  jsyModel = jsy.getModel();

  if (jsyModel == MYCILA_JSY_MK_194 || jsyModel == MYCILA_JSY_MK_333 || jsyModel == MYCILA_JSY_MK_UNKNOWN) {
    dashboard.remove(&jsy163Voltage);
    dashboard.remove(&jsy163current);
    dashboard.remove(&jsy163PowerFactor);
    dashboard.remove(&jsy163ActivePower);
    dashboard.remove(&jsy163ApparentPower);
    dashboard.remove(&jsy163ReactivePower);
    dashboard.remove(&jsy163ActiveEnergy);
    dashboard.remove(&jsy163ActiveEnergyImported);
    dashboard.remove(&jsy163ActiveEnergyReturned);
    dashboard.remove(&jsy163ActivePowerHistory);
  }

  if (jsyModel == MYCILA_JSY_MK_163 || jsyModel == MYCILA_JSY_MK_333 || jsyModel == MYCILA_JSY_MK_UNKNOWN) {
    dashboard.remove(&jsy194Channel1Voltage);
    dashboard.remove(&jsy194Channel1Current);
    dashboard.remove(&jsy194Channel1PowerFactor);
    dashboard.remove(&jsy194Channel1ActivePower);
    dashboard.remove(&jsy194Channel1ApparentPower);
    dashboard.remove(&jsy194Channel1ReactivePower);
    dashboard.remove(&jsy194Channel1ActiveEnergy);
    dashboard.remove(&jsy194Channel1ActiveEnergyImported);
    dashboard.remove(&jsy194Channel1ActiveEnergyReturned);
    dashboard.remove(&jsy194Channel2Voltage);
    dashboard.remove(&jsy194Channel2Current);
    dashboard.remove(&jsy194Channel2PowerFactor);
    dashboard.remove(&jsy194Channel2ActivePower);
    dashboard.remove(&jsy194Channel2ApparentPower);
    dashboard.remove(&jsy194Channel2ReactivePower);
    dashboard.remove(&jsy194Channel2ActiveEnergy);
    dashboard.remove(&jsy194Channel2ActiveEnergyImported);
    dashboard.remove(&jsy194Channel2ActiveEnergyReturned);
    dashboard.remove(&jsy194Channel1ActivePowerHistory);
    dashboard.remove(&jsy194Channel2ActivePowerHistory);
  }

  if (jsyModel == MYCILA_JSY_MK_163 || jsyModel == MYCILA_JSY_MK_194 || jsyModel == MYCILA_JSY_MK_UNKNOWN) {
    dashboard.remove(&jsy333PhaseAVoltage);
    dashboard.remove(&jsy333PhaseACurrent);
    dashboard.remove(&jsy333PhaseAPowerFactor);
    dashboard.remove(&jsy333PhaseAActivePower);
    dashboard.remove(&jsy333PhaseAApparentPower);
    dashboard.remove(&jsy333PhaseAReactivePower);
    dashboard.remove(&jsy333PhaseAActiveEnergy);
    dashboard.remove(&jsy333PhaseAActiveEnergyImported);
    dashboard.remove(&jsy333PhaseAActiveEnergyReturned);
    dashboard.remove(&jsy333PhaseAReactiveEnergy);
    dashboard.remove(&jsy333PhaseAReactiveEnergyImported);
    dashboard.remove(&jsy333PhaseAReactiveEnergyReturned);
    dashboard.remove(&jsy333PhaseAApparentEnergy);
    dashboard.remove(&jsy333PhaseBVoltage);
    dashboard.remove(&jsy333PhaseBCurrent);
    dashboard.remove(&jsy333PhaseBPowerFactor);
    dashboard.remove(&jsy333PhaseBActivePower);
    dashboard.remove(&jsy333PhaseBApparentPower);
    dashboard.remove(&jsy333PhaseBReactivePower);
    dashboard.remove(&jsy333PhaseBActiveEnergy);
    dashboard.remove(&jsy333PhaseBActiveEnergyImported);
    dashboard.remove(&jsy333PhaseBActiveEnergyReturned);
    dashboard.remove(&jsy333PhaseBReactiveEnergy);
    dashboard.remove(&jsy333PhaseBReactiveEnergyImported);
    dashboard.remove(&jsy333PhaseBReactiveEnergyReturned);
    dashboard.remove(&jsy333PhaseBApparentEnergy);
    dashboard.remove(&jsy333PhaseCVoltage);
    dashboard.remove(&jsy333PhaseCCurrent);
    dashboard.remove(&jsy333PhaseCPowerFactor);
    dashboard.remove(&jsy333PhaseCActivePower);
    dashboard.remove(&jsy333PhaseCApparentPower);
    dashboard.remove(&jsy333PhaseCReactivePower);
    dashboard.remove(&jsy333PhaseCActiveEnergy);
    dashboard.remove(&jsy333PhaseCActiveEnergyImported);
    dashboard.remove(&jsy333PhaseCActiveEnergyReturned);
    dashboard.remove(&jsy333PhaseCReactiveEnergy);
    dashboard.remove(&jsy333PhaseCReactiveEnergyImported);
    dashboard.remove(&jsy333PhaseCReactiveEnergyReturned);
    dashboard.remove(&jsy333PhaseCApparentEnergy);
    dashboard.remove(&jsy333PhaseAActivePowerHistory);
    dashboard.remove(&jsy333PhaseBActivePowerHistory);
    dashboard.remove(&jsy333PhaseCActivePowerHistory);
  }

  // Network Manager
  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);
  espConnect.listen([](Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    logger.debug(TAG, "NetworkState: %s => %s", espConnect.getStateName(previous), espConnect.getStateName(state));
    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_DISABLED:
        logger.warn(TAG, "Disabled Network!");
        break;
      case Mycila::ESPConnect::State::AP_STARTING:
        logger.info(TAG, "Starting Access Point %s...", espConnect.getAccessPointSSID().c_str());
        break;
      case Mycila::ESPConnect::State::AP_STARTED:
        logger.info(TAG, "Access Point %s started with IP address %s", espConnect.getWiFiSSID().c_str(), espConnect.getIPAddress().toString().c_str());
        networkUpTask.resume();
        break;
      case Mycila::ESPConnect::State::NETWORK_CONNECTING:
        logger.info(TAG, "Connecting to network...");
        break;
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
        logger.info(TAG, "Connected with IP address %s", espConnect.getIPAddress().toString().c_str());
        networkUpTask.resume();
        break;
      case Mycila::ESPConnect::State::NETWORK_TIMEOUT:
        logger.warn(TAG, "Unable to connect!");
        break;
      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        logger.warn(TAG, "Disconnected!");
        break;
      case Mycila::ESPConnect::State::NETWORK_RECONNECTING:
        logger.info(TAG, "Trying to reconnect...");
        break;
      case Mycila::ESPConnect::State::PORTAL_STARTING:
        logger.info(TAG, "Starting Captive Portal %s for %" PRIu32 " seconds...", espConnect.getAccessPointSSID().c_str(), espConnect.getCaptivePortalTimeout());
        break;
      case Mycila::ESPConnect::State::PORTAL_STARTED:
        logger.info(TAG, "Captive Portal started at %s with IP address %s", espConnect.getWiFiSSID().c_str(), espConnect.getIPAddress().toString().c_str());
        break;
      case Mycila::ESPConnect::State::PORTAL_COMPLETE: {
        if (espConnect.hasConfiguredAPMode()) {
          logger.info(TAG, "Captive Portal: Access Point configured");
        } else {
          logger.info(TAG, "Captive Portal: WiFi configured");
        }
        break;
      }
      case Mycila::ESPConnect::State::PORTAL_TIMEOUT:
        logger.warn(TAG, "Captive Portal: timed out.");
        break;
      default:
        break;
    }
  });
  espConnect.begin(hostname.c_str(), ssid.c_str(), MYCILA_ADMIN_PASSWORD);

  // start tasks
  // assert(coreTaskManager.asyncStart(1024 * 4, 1, 1, 100, true)); // NOLINT
  // assert(jsyTaskManager.asyncStart(1024 * 3, 1, 1, 100, true));  // NOLINT

  logger.info(TAG, "Started " MYCILA_APP_NAME "!");
}

// Destroy default Arduino async task
void loop() { 
  // vTaskDelete(NULL); 
  coreTaskManager.loop();
  jsyTaskManager.loop();
}
