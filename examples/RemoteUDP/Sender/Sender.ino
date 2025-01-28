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

#define DASH_USE_STL_STRING 1

#define MYCILA_ADMIN_USERNAME "admin"
#define MYCILA_APP_NAME       "JSY Remote UDP Sender"
#define MYCILA_GRAPH_POINTS   60
// #define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x01 // old json format
#define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x02 // supports all JSY models
#define MYCILA_UDP_SEND_INTERVAL_MS  200
#define MYCILA_UDP_SEND_RATE_WINDOW  50
#define TAG                          "JSY-UDP"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <ESPmDNS.h>

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>             // https://github.com/ESP32Async/AsyncTCP
#include <ESPAsyncWebServer.h>    // https://github.com/ESP32Async/ESPAsyncWebServer
#include <ESPDash.h>              // https://github.com/mathieucarbou/ayushsharma82-ESP-dASH#dev-v5
#include <ElegantOTA.h>           // https://github.com/mathieucarbou/ayushsharma82-ElegantOTA#dev
#include <FastCRC32.h>            // https://github.com/RobTillaart/CRC
#include <MycilaCircularBuffer.h> // https://github.com/mathieucarbou/MycilaUtilities
#include <MycilaESPConnect.h>     // https://github.com/mathieucarbou/MycilaESPConnect
#include <MycilaJSY.h>            // https://github.com/mathieucarbou/MycilaJSY
#include <MycilaLogger.h>         // https://github.com/mathieucarbou/MycilaLogger
#include <MycilaSystem.h>         // https://github.com/mathieucarbou/MycilaSystem
#include <MycilaTaskManager.h>    // https://github.com/mathieucarbou/MycilaTaskManager
#include <MycilaTime.h>           // https://github.com/mathieucarbou/MycilaUtilities
#include <MycilaWebSerial.h>      // https://github.com/mathieucarbou/MycilaWebSerial

#include <string>

static AsyncUDP udp;
static AsyncWebServer webServer(80);
static Mycila::ESPConnect espConnect(webServer);
static ESPDash dashboard = ESPDash(webServer, "/dashboard", false);
static WebSerial webSerial;
static Mycila::JSY jsy;
static Mycila::Logger logger;
static Mycila::TaskManager coreTaskManager("core");
static Mycila::TaskManager jsyTaskManager("jsy");

static dash::StatisticValue networkHostname(dashboard, "Network Hostname");
static dash::StatisticValue networkInterface(dashboard, "Network Interface");
static dash::StatisticValue networkAPIP(dashboard, "Network Access Point IP Address");
static dash::StatisticValue networkAPMAC(dashboard, "Network Access Point MAC Address");
static dash::StatisticValue networkEthIP(dashboard, "Network Ethernet IP Address");
static dash::StatisticValue networkEthMAC(dashboard, "Network Ethernet MAC Address");
static dash::StatisticValue networkWiFiIP(dashboard, "Network WiFi IP Address");
static dash::StatisticValue networkWiFiMAC(dashboard, "Network WiFi MAC Address");
static dash::StatisticValue networkWiFiSSID(dashboard, "Network WiFi SSID");
static dash::StatisticValue networkWiFiRSSI(dashboard, "Network WiFi RSSI");
static dash::StatisticValue networkWiFiSignal(dashboard, "Network WiFi Signal");
static dash::StatisticValue uptime(dashboard, "Uptime");
static dash::StatisticValue version(dashboard, "Version");

static dash::SwitchCard udpSendEnabledCard(dashboard, "Enable UDP Transfer");
static dash::SwitchCard restart(dashboard, "Restart");
static dash::SwitchCard energyReset(dashboard, "Reset Energy");
static dash::SwitchCard reset(dashboard, "Factory Reset");

static dash::GenericCard jsyModelCard(dashboard, "Model");
static dash::GenericCard<float, 2> messageRateCard(dashboard, "Message Rate", "msg/s");
static dash::GenericCard<float, 2> dataRateCard(dashboard, "Data Rate", "bytes/s");

// JSY-MK-163
static dash::GenericCard<float, 1> jsy163Frequency(dashboard, "Frequency", "Hz");
static dash::GenericCard<float, 1> jsy163Voltage(dashboard, "Voltage", "V");
static dash::GenericCard<float, 2> jsy163current(dashboard, "Current", "A");
static dash::GenericCard<float, 2> jsy163PowerFactor(dashboard, "Power Factor");
static dash::GenericCard<float, 0> jsy163ActivePower(dashboard, "Active Power", "W");
static dash::GenericCard<float, 0> jsy163ApparentPower(dashboard, "Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy163ReactivePower(dashboard, "ReactivePower", "VAr");
static dash::GenericCard<uint32_t> jsy163ActiveEnergy(dashboard, "Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy163ActiveEnergyImported(dashboard, "Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy163ActiveEnergyReturned(dashboard, "Active Energy Returned", "Wh");
static dash::BarChart<int8_t, int16_t> jsy163ActivePowerHistory(dashboard, "Active Power (W)");

// JSY-MK-194
static dash::GenericCard<float, 1> jsy194Channel1Frequency(dashboard, "Channel 1 Frequency", "Hz");
static dash::GenericCard<float, 1> jsy194Channel1Voltage(dashboard, "Channel 1 Voltage", "V");
static dash::GenericCard<float, 2> jsy194Channel1Current(dashboard, "Channel 1 Current", "A");
static dash::GenericCard<float, 2> jsy194Channel1PowerFactor(dashboard, "Channel 1 Power Factor");
static dash::GenericCard<float, 0> jsy194Channel1ActivePower(dashboard, "Channel 1 Active Power", "W");
static dash::GenericCard<float, 0> jsy194Channel1ApparentPower(dashboard, "Channel 1 Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy194Channel1ReactivePower(dashboard, "Channel 1 ReactivePower", "VAr");
static dash::GenericCard<uint32_t> jsy194Channel1ActiveEnergy(dashboard, "Channel 1 Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel1ActiveEnergyImported(dashboard, "Channel 1 Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel1ActiveEnergyReturned(dashboard, "Channel 1 Active Energy Returned", "Wh");
static dash::GenericCard<float, 1> jsy194Channel2Frequency(dashboard, "Channel 2 Frequency", "Hz");
static dash::GenericCard<float, 1> jsy194Channel2Voltage(dashboard, "Channel 2 Voltage", "V");
static dash::GenericCard<float, 2> jsy194Channel2Current(dashboard, "Channel 2 Current", "A");
static dash::GenericCard<float, 2> jsy194Channel2PowerFactor(dashboard, "Channel 2 Power Factor");
static dash::GenericCard<float, 0> jsy194Channel2ActivePower(dashboard, "Channel 2 Active Power", "W");
static dash::GenericCard<float, 0> jsy194Channel2ApparentPower(dashboard, "Channel 2 Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy194Channel2ReactivePower(dashboard, "Channel 2 ReactivePower", "VAr");
static dash::GenericCard<uint32_t> jsy194Channel2ActiveEnergy(dashboard, "Channel 2 Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel2ActiveEnergyImported(dashboard, "Channel 2 Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel2ActiveEnergyReturned(dashboard, "Channel 2 Active Energy Returned", "Wh");
static dash::BarChart<int8_t, int16_t> jsy194Channel1ActivePowerHistory(dashboard, "Channel 1 Active Power (W)");
static dash::BarChart<int8_t, int16_t> jsy194Channel2ActivePowerHistory(dashboard, "Channel 2 Active Power (W)");

// JSY-MK-333
static dash::GenericCard<float, 1> jsy333PhaseAFrequency(dashboard, "Phase A Frequency", "Hz");
static dash::GenericCard<float, 1> jsy333PhaseAVoltage(dashboard, "Phase A Voltage", "V");
static dash::GenericCard<float, 2> jsy333PhaseACurrent(dashboard, "Phase A Current", "A");
static dash::GenericCard<float, 2> jsy333PhaseAPowerFactor(dashboard, "Phase A Power Factor");
static dash::GenericCard<float, 0> jsy333PhaseAActivePower(dashboard, "Phase A Active Power", "W");
static dash::GenericCard<float, 0> jsy333PhaseAApparentPower(dashboard, "Phase A Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy333PhaseAReactivePower(dashboard, "Phase A ReactivePower", "VAr");
static dash::GenericCard<uint32_t> jsy333PhaseAActiveEnergy(dashboard, "Phase A Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseAActiveEnergyImported(dashboard, "Phase A Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseAActiveEnergyReturned(dashboard, "Phase A Active Energy Returned", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseAReactiveEnergy(dashboard, "Phase A Reactive Energy", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseAReactiveEnergyImported(dashboard, "Phase A Reactive Energy Imported", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseAReactiveEnergyReturned(dashboard, "Phase A Reactive Energy Returned", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseAApparentEnergy(dashboard, "Phase A Apparent Energy", "VAh");
static dash::GenericCard<float, 1> jsy333PhaseBFrequency(dashboard, "Phase B Frequency", "Hz");
static dash::GenericCard<float, 1> jsy333PhaseBVoltage(dashboard, "Phase B Voltage", "V");
static dash::GenericCard<float, 2> jsy333PhaseBCurrent(dashboard, "Phase B Current", "A");
static dash::GenericCard<float, 2> jsy333PhaseBPowerFactor(dashboard, "Phase B Power Factor");
static dash::GenericCard<float, 0> jsy333PhaseBActivePower(dashboard, "Phase B Active Power", "W");
static dash::GenericCard<float, 0> jsy333PhaseBApparentPower(dashboard, "Phase B Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy333PhaseBReactivePower(dashboard, "Phase B ReactivePower", "VAr");
static dash::GenericCard<uint32_t> jsy333PhaseBActiveEnergy(dashboard, "Phase B Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseBActiveEnergyImported(dashboard, "Phase B Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseBActiveEnergyReturned(dashboard, "Phase B Active Energy Returned", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseBReactiveEnergy(dashboard, "Phase B Reactive Energy", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseBReactiveEnergyImported(dashboard, "Phase B Reactive Energy Imported", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseBReactiveEnergyReturned(dashboard, "Phase B Reactive Energy Returned", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseBApparentEnergy(dashboard, "Phase B Apparent Energy", "VAh");
static dash::GenericCard<float, 1> jsy333PhaseCFrequency(dashboard, "Phase C Frequency", "Hz");
static dash::GenericCard<float, 1> jsy333PhaseCVoltage(dashboard, "Phase C Voltage", "V");
static dash::GenericCard<float, 2> jsy333PhaseCCurrent(dashboard, "Phase C Current", "A");
static dash::GenericCard<float, 2> jsy333PhaseCPowerFactor(dashboard, "Phase C Power Factor");
static dash::GenericCard<float, 0> jsy333PhaseCActivePower(dashboard, "Phase C Active Power", "W");
static dash::GenericCard<float, 0> jsy333PhaseCApparentPower(dashboard, "Phase C Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy333PhaseCReactivePower(dashboard, "Phase C ReactivePower", "VAr");
static dash::GenericCard<uint32_t> jsy333PhaseCActiveEnergy(dashboard, "Phase C Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseCActiveEnergyImported(dashboard, "Phase C Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseCActiveEnergyReturned(dashboard, "Phase C Active Energy Returned", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseCReactiveEnergy(dashboard, "Phase C Reactive Energy", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseCReactiveEnergyImported(dashboard, "Phase C Reactive Energy Imported", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseCReactiveEnergyReturned(dashboard, "Phase C Reactive Energy Returned", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseCApparentEnergy(dashboard, "Phase C Apparent Energy", "VAh");
static dash::BarChart<int8_t, int16_t> jsy333PhaseAActivePowerHistory(dashboard, "Phase A Active Power (W)");
static dash::BarChart<int8_t, int16_t> jsy333PhaseBActivePowerHistory(dashboard, "Phase B Active Power (W)");
static dash::BarChart<int8_t, int16_t> jsy333PhaseCActivePowerHistory(dashboard, "Phase C Active Power (W)");

static int8_t historyX[MYCILA_GRAPH_POINTS] = {0};
static int16_t power1HistoryY[MYCILA_GRAPH_POINTS] = {0};
static int16_t power2HistoryY[MYCILA_GRAPH_POINTS] = {0};
static int16_t power3HistoryY[MYCILA_GRAPH_POINTS] = {0};

static std::string hostname;
static std::string ssid;

static bool udpSendEnabled = true;
static uint16_t jsyModel = MYCILA_JSY_MK_UNKNOWN;

// circular buffer for msg rate
static Mycila::CircularBuffer<float, MYCILA_UDP_SEND_RATE_WINDOW> messageRateBuffer;
static float messageRate = 0;

// circular buffer for data rate
static Mycila::CircularBuffer<uint32_t, MYCILA_UDP_SEND_RATE_WINDOW> dataRateBuffer;
static uint32_t dataRate = 0;

static Mycila::Task jsyTask("JSY", [](void* params) { jsy.read(); });
static Mycila::Task networkManagerTask("ESPConnect", [](void* params) { espConnect.loop(); });

static Mycila::Task networkUpTask("Network UP", Mycila::TaskType::ONCE, [](void* params) {
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

static Mycila::Task otaTask("OTA", Mycila::TaskType::ONCE, [](void* params) {
  logger.info(TAG, "Preparing OTA update...");
  udpSendEnabled = false;
  jsy.end();
});

static Mycila::Task restartTask("Restart", Mycila::TaskType::ONCE, [](void* params) {
  logger.warn(TAG, "Restarting " MYCILA_APP_NAME "...");
  Mycila::System::restart(500);
});

static Mycila::Task dashboardTask("Dashboard", [](void* params) {
  Mycila::ESPConnect::Mode mode = espConnect.getMode();

  networkAPIP.setValue(espConnect.getIPAddress(Mycila::ESPConnect::Mode::AP).toString().c_str());
  networkEthIP.setValue(espConnect.getIPAddress(Mycila::ESPConnect::Mode::ETH).toString().c_str());
  networkInterface.setValue(mode == Mycila::ESPConnect::Mode::AP ? "AP" : (mode == Mycila::ESPConnect::Mode::STA ? "WiFi" : (mode == Mycila::ESPConnect::Mode::ETH ? "Ethernet" : "")));
  networkWiFiIP.setValue(espConnect.getIPAddress(Mycila::ESPConnect::Mode::STA).toString().c_str());
  networkWiFiRSSI.setValue((std::to_string(espConnect.getWiFiRSSI()) + " dBm"));
  networkWiFiSignal.setValue((std::to_string(espConnect.getWiFiSignalQuality()) + " %"));
  networkWiFiSSID.setValue(espConnect.getWiFiSSID());
  uptime.setValue(Mycila::Time::toDHHMMSS(Mycila::System::getUptime()));

  messageRateCard.setValue(messageRate);
  dataRateCard.setValue(static_cast<int>(dataRate));
  udpSendEnabledCard.setValue(udpSendEnabled);

  switch (jsyModel) {
    case MYCILA_JSY_MK_163: {
      jsy163Frequency.setValue(jsy.data.single().frequency);
      jsy163Voltage.setValue(jsy.data.single().voltage);
      jsy163current.setValue(jsy.data.single().current);
      jsy163PowerFactor.setValue(jsy.data.single().powerFactor);
      jsy163ActivePower.setValue(jsy.data.single().activePower);
      jsy163ApparentPower.setValue(jsy.data.single().apparentPower);
      jsy163ReactivePower.setValue(jsy.data.single().reactivePower);
      jsy163ActiveEnergy.setValue((int)jsy.data.single().activeEnergy);
      jsy163ActiveEnergyImported.setValue((int)jsy.data.single().activeEnergyImported);
      jsy163ActiveEnergyReturned.setValue((int)jsy.data.single().activeEnergyReturned);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.phaseA().activePower);

      // update charts
      jsy163ActivePowerHistory.setY(power1HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    case MYCILA_JSY_MK_193:
    case MYCILA_JSY_MK_194: {
      jsy194Channel1Frequency.setValue(jsy.data.channel1().frequency);
      jsy194Channel1Voltage.setValue(jsy.data.channel1().voltage);
      jsy194Channel1Current.setValue(jsy.data.channel1().current);
      jsy194Channel1PowerFactor.setValue(jsy.data.channel1().powerFactor);
      jsy194Channel1ActivePower.setValue(jsy.data.channel1().activePower);
      jsy194Channel1ApparentPower.setValue(jsy.data.channel1().apparentPower);
      jsy194Channel1ReactivePower.setValue(jsy.data.channel1().reactivePower);
      jsy194Channel1ActiveEnergy.setValue(jsy.data.channel1().activeEnergy);
      jsy194Channel1ActiveEnergyImported.setValue(jsy.data.channel1().activeEnergyImported);
      jsy194Channel1ActiveEnergyReturned.setValue(jsy.data.channel1().activeEnergyReturned);

      jsy194Channel2Frequency.setValue(jsy.data.channel2().frequency);
      jsy194Channel2Voltage.setValue(jsy.data.channel2().voltage);
      jsy194Channel2Current.setValue(jsy.data.channel2().current);
      jsy194Channel2PowerFactor.setValue(jsy.data.channel2().powerFactor);
      jsy194Channel2ActivePower.setValue(jsy.data.channel2().activePower);
      jsy194Channel2ApparentPower.setValue(jsy.data.channel2().apparentPower);
      jsy194Channel2ReactivePower.setValue(jsy.data.channel2().reactivePower);
      jsy194Channel2ActiveEnergy.setValue(jsy.data.channel2().activeEnergy);
      jsy194Channel2ActiveEnergyImported.setValue(jsy.data.channel2().activeEnergyImported);
      jsy194Channel2ActiveEnergyReturned.setValue(jsy.data.channel2().activeEnergyReturned);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
        power2HistoryY[i] = power2HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.channel1().activePower);
      power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.data.channel2().activePower);

      // update charts
      jsy194Channel1ActivePowerHistory.setY(power1HistoryY, MYCILA_GRAPH_POINTS);
      jsy194Channel2ActivePowerHistory.setY(power2HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    case MYCILA_JSY_MK_333: {
      jsy333PhaseAFrequency.setValue(jsy.data.phaseA().frequency);
      jsy333PhaseAVoltage.setValue(jsy.data.phaseA().voltage);
      jsy333PhaseACurrent.setValue(jsy.data.phaseA().current);
      jsy333PhaseAPowerFactor.setValue(jsy.data.phaseA().powerFactor);
      jsy333PhaseAActivePower.setValue(jsy.data.phaseA().activePower);
      jsy333PhaseAApparentPower.setValue(jsy.data.phaseA().apparentPower);
      jsy333PhaseAReactivePower.setValue(jsy.data.phaseA().reactivePower);
      jsy333PhaseAActiveEnergy.setValue(jsy.data.phaseA().activeEnergy);
      jsy333PhaseAActiveEnergyImported.setValue(jsy.data.phaseA().activeEnergyImported);
      jsy333PhaseAActiveEnergyReturned.setValue(jsy.data.phaseA().activeEnergyReturned);
      jsy333PhaseAReactiveEnergy.setValue(jsy.data.phaseA().reactiveEnergy);
      jsy333PhaseAReactiveEnergyImported.setValue(jsy.data.phaseA().reactiveEnergyImported);
      jsy333PhaseAReactiveEnergyReturned.setValue(jsy.data.phaseA().reactiveEnergyReturned);
      jsy333PhaseAApparentEnergy.setValue(jsy.data.phaseA().apparentEnergy);

      jsy333PhaseBFrequency.setValue(jsy.data.phaseB().frequency);
      jsy333PhaseBVoltage.setValue(jsy.data.phaseB().voltage);
      jsy333PhaseBCurrent.setValue(jsy.data.phaseB().current);
      jsy333PhaseBPowerFactor.setValue(jsy.data.phaseB().powerFactor);
      jsy333PhaseBActivePower.setValue(jsy.data.phaseB().activePower);
      jsy333PhaseBApparentPower.setValue(jsy.data.phaseB().apparentPower);
      jsy333PhaseBReactivePower.setValue(jsy.data.phaseB().reactivePower);
      jsy333PhaseBActiveEnergy.setValue(jsy.data.phaseB().activeEnergy);
      jsy333PhaseBActiveEnergyImported.setValue(jsy.data.phaseB().activeEnergyImported);
      jsy333PhaseBActiveEnergyReturned.setValue(jsy.data.phaseB().activeEnergyReturned);
      jsy333PhaseBReactiveEnergy.setValue(jsy.data.phaseB().reactiveEnergy);
      jsy333PhaseBReactiveEnergyImported.setValue(jsy.data.phaseB().reactiveEnergyImported);
      jsy333PhaseBReactiveEnergyReturned.setValue(jsy.data.phaseB().reactiveEnergyReturned);
      jsy333PhaseBApparentEnergy.setValue(jsy.data.phaseB().apparentEnergy);

      jsy333PhaseCFrequency.setValue(jsy.data.phaseC().frequency);
      jsy333PhaseCVoltage.setValue(jsy.data.phaseC().voltage);
      jsy333PhaseCCurrent.setValue(jsy.data.phaseC().current);
      jsy333PhaseCPowerFactor.setValue(jsy.data.phaseC().powerFactor);
      jsy333PhaseCActivePower.setValue(jsy.data.phaseC().activePower);
      jsy333PhaseCApparentPower.setValue(jsy.data.phaseC().apparentPower);
      jsy333PhaseCReactivePower.setValue(jsy.data.phaseC().reactivePower);
      jsy333PhaseCActiveEnergy.setValue(jsy.data.phaseC().activeEnergy);
      jsy333PhaseCActiveEnergyImported.setValue(jsy.data.phaseC().activeEnergyImported);
      jsy333PhaseCActiveEnergyReturned.setValue(jsy.data.phaseC().activeEnergyReturned);
      jsy333PhaseCReactiveEnergy.setValue(jsy.data.phaseC().reactiveEnergy);
      jsy333PhaseCReactiveEnergyImported.setValue(jsy.data.phaseC().reactiveEnergyImported);
      jsy333PhaseCReactiveEnergyReturned.setValue(jsy.data.phaseC().reactiveEnergyReturned);
      jsy333PhaseCApparentEnergy.setValue(jsy.data.phaseC().apparentEnergy);

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
      jsy333PhaseAActivePowerHistory.setY(power1HistoryY, MYCILA_GRAPH_POINTS);
      jsy333PhaseBActivePowerHistory.setY(power2HistoryY, MYCILA_GRAPH_POINTS);
      jsy333PhaseCActivePowerHistory.setY(power3HistoryY, MYCILA_GRAPH_POINTS);

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
  hostname = std::string("jsy-") + Mycila::System::getChipIDStr();
  std::transform(hostname.begin(), hostname.end(), hostname.begin(), ::tolower);
  ssid = std::string("JSY-") + Mycila::System::getChipIDStr();

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
  restartTask.setManager(coreTaskManager);

  // profiling
  jsyTask.enableProfiling(10, Mycila::TaskTimeUnit::MILLISECONDS);

  // WebSerial
  webSerial.setAuthentication(MYCILA_ADMIN_USERNAME, MYCILA_ADMIN_PASSWORD);
  webSerial.begin(&webServer, "/console");
  logger.forwardTo(&webSerial);

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

  version.setValue(MYCILA_JSY_VERSION);

  restart.onChange([](bool state) { restartTask.resume(); });

  reset.onChange([](bool state) {
    espConnect.clearConfiguration();
    restartTask.resume();
  });

  for (int i = 0; i < MYCILA_GRAPH_POINTS; i++)
    historyX[i] = i - MYCILA_GRAPH_POINTS;
  jsy163ActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy194Channel1ActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy194Channel2ActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseAActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseBActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseCActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);

  energyReset.onChange([](bool state) {
    jsy.resetEnergy();
    energyReset.setValue(0);
    dashboard.refresh(energyReset);
  });

  udpSendEnabledCard.onChange([](bool state) {
    udpSendEnabled = state;
    udpSendEnabledCard.setValue(udpSendEnabled);
    dashboard.refresh(udpSendEnabledCard);
  });

  dashboard.onBeforeUpdate([](bool changes_only) {
    if (!changes_only) {
      logger.debug(TAG, "Dashboard refresh requested");
      jsyModelCard.setValue(jsyModel == MYCILA_JSY_MK_UNKNOWN ? "Unknown" : jsy.getModelName());
      networkAPMAC.setValue(espConnect.getMACAddress(Mycila::ESPConnect::Mode::AP));
      networkEthMAC.setValue(espConnect.getMACAddress(Mycila::ESPConnect::Mode::ETH).empty() ? std::string("N/A") : espConnect.getMACAddress(Mycila::ESPConnect::Mode::ETH));
      networkHostname.setValue(hostname);
      networkWiFiMAC.setValue(espConnect.getMACAddress(Mycila::ESPConnect::Mode::STA));
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
        messageRateBuffer.add(static_cast<float>(esp_timer_get_time() / 1000000));
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
    dashboard.remove(jsy163Voltage);
    dashboard.remove(jsy163current);
    dashboard.remove(jsy163PowerFactor);
    dashboard.remove(jsy163ActivePower);
    dashboard.remove(jsy163ApparentPower);
    dashboard.remove(jsy163ReactivePower);
    dashboard.remove(jsy163ActiveEnergy);
    dashboard.remove(jsy163ActiveEnergyImported);
    dashboard.remove(jsy163ActiveEnergyReturned);
    dashboard.remove(jsy163ActivePowerHistory);
  }

  if (jsyModel == MYCILA_JSY_MK_163 || jsyModel == MYCILA_JSY_MK_333 || jsyModel == MYCILA_JSY_MK_UNKNOWN) {
    dashboard.remove(jsy194Channel1Voltage);
    dashboard.remove(jsy194Channel1Current);
    dashboard.remove(jsy194Channel1PowerFactor);
    dashboard.remove(jsy194Channel1ActivePower);
    dashboard.remove(jsy194Channel1ApparentPower);
    dashboard.remove(jsy194Channel1ReactivePower);
    dashboard.remove(jsy194Channel1ActiveEnergy);
    dashboard.remove(jsy194Channel1ActiveEnergyImported);
    dashboard.remove(jsy194Channel1ActiveEnergyReturned);
    dashboard.remove(jsy194Channel2Voltage);
    dashboard.remove(jsy194Channel2Current);
    dashboard.remove(jsy194Channel2PowerFactor);
    dashboard.remove(jsy194Channel2ActivePower);
    dashboard.remove(jsy194Channel2ApparentPower);
    dashboard.remove(jsy194Channel2ReactivePower);
    dashboard.remove(jsy194Channel2ActiveEnergy);
    dashboard.remove(jsy194Channel2ActiveEnergyImported);
    dashboard.remove(jsy194Channel2ActiveEnergyReturned);
    dashboard.remove(jsy194Channel1ActivePowerHistory);
    dashboard.remove(jsy194Channel2ActivePowerHistory);
  }

  if (jsyModel == MYCILA_JSY_MK_163 || jsyModel == MYCILA_JSY_MK_194 || jsyModel == MYCILA_JSY_MK_UNKNOWN) {
    dashboard.remove(jsy333PhaseAVoltage);
    dashboard.remove(jsy333PhaseACurrent);
    dashboard.remove(jsy333PhaseAPowerFactor);
    dashboard.remove(jsy333PhaseAActivePower);
    dashboard.remove(jsy333PhaseAApparentPower);
    dashboard.remove(jsy333PhaseAReactivePower);
    dashboard.remove(jsy333PhaseAActiveEnergy);
    dashboard.remove(jsy333PhaseAActiveEnergyImported);
    dashboard.remove(jsy333PhaseAActiveEnergyReturned);
    dashboard.remove(jsy333PhaseAReactiveEnergy);
    dashboard.remove(jsy333PhaseAReactiveEnergyImported);
    dashboard.remove(jsy333PhaseAReactiveEnergyReturned);
    dashboard.remove(jsy333PhaseAApparentEnergy);
    dashboard.remove(jsy333PhaseBVoltage);
    dashboard.remove(jsy333PhaseBCurrent);
    dashboard.remove(jsy333PhaseBPowerFactor);
    dashboard.remove(jsy333PhaseBActivePower);
    dashboard.remove(jsy333PhaseBApparentPower);
    dashboard.remove(jsy333PhaseBReactivePower);
    dashboard.remove(jsy333PhaseBActiveEnergy);
    dashboard.remove(jsy333PhaseBActiveEnergyImported);
    dashboard.remove(jsy333PhaseBActiveEnergyReturned);
    dashboard.remove(jsy333PhaseBReactiveEnergy);
    dashboard.remove(jsy333PhaseBReactiveEnergyImported);
    dashboard.remove(jsy333PhaseBReactiveEnergyReturned);
    dashboard.remove(jsy333PhaseBApparentEnergy);
    dashboard.remove(jsy333PhaseCVoltage);
    dashboard.remove(jsy333PhaseCCurrent);
    dashboard.remove(jsy333PhaseCPowerFactor);
    dashboard.remove(jsy333PhaseCActivePower);
    dashboard.remove(jsy333PhaseCApparentPower);
    dashboard.remove(jsy333PhaseCReactivePower);
    dashboard.remove(jsy333PhaseCActiveEnergy);
    dashboard.remove(jsy333PhaseCActiveEnergyImported);
    dashboard.remove(jsy333PhaseCActiveEnergyReturned);
    dashboard.remove(jsy333PhaseCReactiveEnergy);
    dashboard.remove(jsy333PhaseCReactiveEnergyImported);
    dashboard.remove(jsy333PhaseCReactiveEnergyReturned);
    dashboard.remove(jsy333PhaseCApparentEnergy);
    dashboard.remove(jsy333PhaseAActivePowerHistory);
    dashboard.remove(jsy333PhaseBActivePowerHistory);
    dashboard.remove(jsy333PhaseCActivePowerHistory);
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
