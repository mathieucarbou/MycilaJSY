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

#ifndef MYCILA_LOGGER_SUPPORT
  #define MYCILA_LOGGER_SUPPORT 1
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

#define MYCILA_ADMIN_USERNAME        "admin"
#define MYCILA_APP_NAME              "JSY Remote UDP Sender"
#define MYCILA_GRAPH_POINTS          60
#define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x02 // supports all JSY models
#define MYCILA_UDP_SEND_RATE_WINDOW  20
#define TAG                          "JSY-UDP"

#include <Arduino.h>
#include <AsyncUDP.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>             // https://github.com/ESP32Async/AsyncTCP
#include <ElegantOTA.h>           // https://github.com/ayushsharma82/ElegantOTA
#include <ESPAsyncWebServer.h>    // https://github.com/ESP32Async/ESPAsyncWebServer
#include <ESPDash.h>              // https://github.com/ayushsharma82/ESP-DASH
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

Mycila::Logger logger;

static AsyncUDP udp;
static AsyncWebServer webServer(80);
static AsyncAuthenticationMiddleware authMiddleware;
static Mycila::ESPConnect espConnect(webServer);
static ESPDash dashboard = ESPDash(webServer, "/dashboard", false);
static WebSerial webSerial;
static Preferences preferences;

static Mycila::JSY jsy;
static volatile Mycila::JSY::Data jsyData;
static Mycila::JSY::Data prevData;
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

static dash::ToggleButtonCard publishDataCard(dashboard, "Publish Data");
static dash::ToggleButtonCard restart(dashboard, "Restart");
static dash::ToggleButtonCard energyReset(dashboard, "Reset Energy");
static dash::ToggleButtonCard reset(dashboard, "Factory Reset");

static dash::GenericCard jsyModelCard(dashboard, "Model");
static dash::GenericCard<float, 2> messageRateCard(dashboard, "Message Rate", "msg/s");
static dash::GenericCard<uint32_t> dataRateCard(dashboard, "Data Rate", "bytes/s");

// JSY-MK-163
static dash::GenericCard<float, 1> jsy163Frequency(dashboard, "Frequency", "Hz");
static dash::GenericCard<float, 1> jsy163Voltage(dashboard, "Voltage", "V");
static dash::GenericCard<float, 2> jsy163current(dashboard, "Current", "A");
static dash::GenericCard<float, 2> jsy163PowerFactor(dashboard, "Power Factor");
static dash::GenericCard<float, 0> jsy163ActivePower(dashboard, "Active Power", "W");
static dash::GenericCard<float, 0> jsy163ApparentPower(dashboard, "Apparent Power", "VA");
static dash::GenericCard<float, 0> jsy163ReactivePower(dashboard, "Reactive Power", "VAr");
static dash::GenericCard<uint32_t> jsy163ActiveEnergy(dashboard, "Active Energy", "Wh");
static dash::GenericCard<uint32_t> jsy163ActiveEnergyImported(dashboard, "Active Energy Imported", "Wh");
static dash::GenericCard<uint32_t> jsy163ActiveEnergyReturned(dashboard, "Active Energy Returned", "Wh");
static dash::BarChart<int8_t, int16_t> jsy163ActivePowerHistory(dashboard, "Active Power (W)");

// JSY-MK-194
static dash::GenericCard<float, 1> jsy194Channel1Frequency(dashboard, "Frequency 1", "Hz");
static dash::GenericCard<float, 1> jsy194Channel1Voltage(dashboard, "Voltage 1", "V");
static dash::GenericCard<float, 2> jsy194Channel1Current(dashboard, "Current 1", "A");
static dash::GenericCard<float, 2> jsy194Channel1PowerFactor(dashboard, "Power Factor 1");
static dash::GenericCard<float, 0> jsy194Channel1ActivePower(dashboard, "Active Power 1", "W");
static dash::GenericCard<float, 0> jsy194Channel1ApparentPower(dashboard, "Apparent Power 1", "VA");
static dash::GenericCard<float, 0> jsy194Channel1ReactivePower(dashboard, "Reactive Power 1", "VAr");
static dash::GenericCard<uint32_t> jsy194Channel1ActiveEnergy(dashboard, "Active Energy 1", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel1ActiveEnergyImported(dashboard, "Active Energy Imported 1", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel1ActiveEnergyReturned(dashboard, "Active Energy Returned 1", "Wh");
static dash::GenericCard<float, 1> jsy194Channel2Frequency(dashboard, "Frequency 2", "Hz");
static dash::GenericCard<float, 1> jsy194Channel2Voltage(dashboard, "Voltage 2", "V");
static dash::GenericCard<float, 2> jsy194Channel2Current(dashboard, "Current 2", "A");
static dash::GenericCard<float, 2> jsy194Channel2PowerFactor(dashboard, "Power Factor 2");
static dash::GenericCard<float, 0> jsy194Channel2ActivePower(dashboard, "Active Power 2", "W");
static dash::GenericCard<float, 0> jsy194Channel2ApparentPower(dashboard, "Apparent Power 2", "VA");
static dash::GenericCard<float, 0> jsy194Channel2ReactivePower(dashboard, "Reactive Power 2", "VAr");
static dash::GenericCard<uint32_t> jsy194Channel2ActiveEnergy(dashboard, "Active Energy 2", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel2ActiveEnergyImported(dashboard, "Active Energy Imported 2", "Wh");
static dash::GenericCard<uint32_t> jsy194Channel2ActiveEnergyReturned(dashboard, "Active Energy Returned 2", "Wh");
static dash::BarChart<int8_t, int16_t> jsy194Channel1ActivePowerHistory(dashboard, "Active Power 1 (W)");
static dash::BarChart<int8_t, int16_t> jsy194Channel2ActivePowerHistory(dashboard, "Active Power 2 (W)");

// JSY-MK-333
static dash::GenericCard<float, 1> jsy333PhaseAFrequency(dashboard, "Frequency A", "Hz");
static dash::GenericCard<float, 1> jsy333PhaseAVoltage(dashboard, "Voltage A", "V");
static dash::GenericCard<float, 2> jsy333PhaseACurrent(dashboard, "Current A", "A");
static dash::GenericCard<float, 2> jsy333PhaseAPowerFactor(dashboard, "Power Factor A");
static dash::GenericCard<float, 0> jsy333PhaseAActivePower(dashboard, "Active Power A", "W");
static dash::GenericCard<float, 0> jsy333PhaseAApparentPower(dashboard, "Apparent Power A", "VA");
static dash::GenericCard<float, 0> jsy333PhaseAReactivePower(dashboard, "Reactive Power A", "VAr");
static dash::GenericCard<uint32_t> jsy333PhaseAActiveEnergy(dashboard, "Active Energy A", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseAActiveEnergyImported(dashboard, "Active Energy Imported A", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseAActiveEnergyReturned(dashboard, "Active Energy Returned A", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseAReactiveEnergy(dashboard, "Reactive Energy A", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseAReactiveEnergyImported(dashboard, "Reactive Energy Imported A", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseAReactiveEnergyReturned(dashboard, "Reactive Energy Returned A", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseAApparentEnergy(dashboard, "Apparent Energy A", "VAh");
static dash::GenericCard<float, 1> jsy333PhaseBFrequency(dashboard, "Frequency B", "Hz");
static dash::GenericCard<float, 1> jsy333PhaseBVoltage(dashboard, "Voltage B", "V");
static dash::GenericCard<float, 2> jsy333PhaseBCurrent(dashboard, "Current B", "A");
static dash::GenericCard<float, 2> jsy333PhaseBPowerFactor(dashboard, "Power Factor B");
static dash::GenericCard<float, 0> jsy333PhaseBActivePower(dashboard, "Active Power B", "W");
static dash::GenericCard<float, 0> jsy333PhaseBApparentPower(dashboard, "Apparent Power B", "VA");
static dash::GenericCard<float, 0> jsy333PhaseBReactivePower(dashboard, "Reactive Power B", "VAr");
static dash::GenericCard<uint32_t> jsy333PhaseBActiveEnergy(dashboard, "Active Energy B", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseBActiveEnergyImported(dashboard, "Active Energy Imported B", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseBActiveEnergyReturned(dashboard, "Active Energy Returned B", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseBReactiveEnergy(dashboard, "Reactive Energy B", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseBReactiveEnergyImported(dashboard, "Reactive Energy Imported B", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseBReactiveEnergyReturned(dashboard, "Reactive Energy Returned B", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseBApparentEnergy(dashboard, "Apparent Energy B", "VAh");
static dash::GenericCard<float, 1> jsy333PhaseCFrequency(dashboard, "Frequency C", "Hz");
static dash::GenericCard<float, 1> jsy333PhaseCVoltage(dashboard, "Voltage C", "V");
static dash::GenericCard<float, 2> jsy333PhaseCCurrent(dashboard, "Current C", "A");
static dash::GenericCard<float, 2> jsy333PhaseCPowerFactor(dashboard, "Power Factor C");
static dash::GenericCard<float, 0> jsy333PhaseCActivePower(dashboard, "Active Power C", "W");
static dash::GenericCard<float, 0> jsy333PhaseCApparentPower(dashboard, "Apparent Power C", "VA");
static dash::GenericCard<float, 0> jsy333PhaseCReactivePower(dashboard, "Reactive Power C", "VAr");
static dash::GenericCard<uint32_t> jsy333PhaseCActiveEnergy(dashboard, "Active Energy C", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseCActiveEnergyImported(dashboard, "Active Energy Imported C", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseCActiveEnergyReturned(dashboard, "Active Energy Returned C", "Wh");
static dash::GenericCard<uint32_t> jsy333PhaseCReactiveEnergy(dashboard, "Reactive Energy C", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseCReactiveEnergyImported(dashboard, "Reactive Energy Imported C", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseCReactiveEnergyReturned(dashboard, "Reactive Energy Returned C", "VArh");
static dash::GenericCard<uint32_t> jsy333PhaseCApparentEnergy(dashboard, "Apparent Energy C", "VAh");
static dash::BarChart<int8_t, int16_t> jsy333PhaseAActivePowerHistory(dashboard, "Active Power A (W)");
static dash::BarChart<int8_t, int16_t> jsy333PhaseBActivePowerHistory(dashboard, "Active Power B (W)");
static dash::BarChart<int8_t, int16_t> jsy333PhaseCActivePowerHistory(dashboard, "Active Power C (W)");

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
static volatile float messageRate = 0;

// circular buffer for data rate
static Mycila::CircularBuffer<uint32_t, MYCILA_UDP_SEND_RATE_WINDOW> dataRateBuffer;
static volatile uint32_t dataRate = 0;

static Mycila::Task jsyTask("JSY", [](void* params) { jsy.read(); });

static Mycila::Task networkManagerTask("ESPConnect", [](void* params) { espConnect.loop(); });

static Mycila::Task networkUpTask("Network UP", Mycila::Task::Type::ONCE, [](void* params) {
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

static Mycila::Task otaTask("OTA", Mycila::Task::Type::ONCE, [](void* params) {
  logger.info(TAG, "Preparing OTA update...");
  udpSendEnabled = false;
  jsy.end();
});

static Mycila::Task restartTask("Restart", Mycila::Task::Type::ONCE, [](void* params) {
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

  messageRateCard.setValue(static_cast<float>(messageRate));
  dataRateCard.setValue(static_cast<uint32_t>(dataRate));
  publishDataCard.setValue(udpSendEnabled);

  switch (jsyModel) {
    case MYCILA_JSY_MK_163: {
      jsy163Frequency.setValue(prevData.single().frequency);
      jsy163Voltage.setValue(prevData.single().voltage);
      jsy163current.setValue(prevData.single().current);
      jsy163PowerFactor.setValue(prevData.single().powerFactor);
      jsy163ActivePower.setValue(prevData.single().activePower);
      jsy163ApparentPower.setValue(prevData.single().apparentPower);
      jsy163ReactivePower.setValue(prevData.single().reactivePower);
      jsy163ActiveEnergy.setValue((int)prevData.single().activeEnergy);
      jsy163ActiveEnergyImported.setValue((int)prevData.single().activeEnergyImported);
      jsy163ActiveEnergyReturned.setValue((int)prevData.single().activeEnergyReturned);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(prevData.phaseA().activePower);

      // update charts
      jsy163ActivePowerHistory.setY(power1HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    case MYCILA_JSY_MK_193:
    case MYCILA_JSY_MK_194: {
      jsy194Channel1Frequency.setValue(prevData.channel1().frequency);
      jsy194Channel1Voltage.setValue(prevData.channel1().voltage);
      jsy194Channel1Current.setValue(prevData.channel1().current);
      jsy194Channel1PowerFactor.setValue(prevData.channel1().powerFactor);
      jsy194Channel1ActivePower.setValue(prevData.channel1().activePower);
      jsy194Channel1ApparentPower.setValue(prevData.channel1().apparentPower);
      jsy194Channel1ReactivePower.setValue(prevData.channel1().reactivePower);
      jsy194Channel1ActiveEnergy.setValue(prevData.channel1().activeEnergy);
      jsy194Channel1ActiveEnergyImported.setValue(prevData.channel1().activeEnergyImported);
      jsy194Channel1ActiveEnergyReturned.setValue(prevData.channel1().activeEnergyReturned);

      jsy194Channel2Frequency.setValue(prevData.channel2().frequency);
      jsy194Channel2Voltage.setValue(prevData.channel2().voltage);
      jsy194Channel2Current.setValue(prevData.channel2().current);
      jsy194Channel2PowerFactor.setValue(prevData.channel2().powerFactor);
      jsy194Channel2ActivePower.setValue(prevData.channel2().activePower);
      jsy194Channel2ApparentPower.setValue(prevData.channel2().apparentPower);
      jsy194Channel2ReactivePower.setValue(prevData.channel2().reactivePower);
      jsy194Channel2ActiveEnergy.setValue(prevData.channel2().activeEnergy);
      jsy194Channel2ActiveEnergyImported.setValue(prevData.channel2().activeEnergyImported);
      jsy194Channel2ActiveEnergyReturned.setValue(prevData.channel2().activeEnergyReturned);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
        power2HistoryY[i] = power2HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(prevData.channel1().activePower);
      power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(prevData.channel2().activePower);

      // update charts
      jsy194Channel1ActivePowerHistory.setY(power1HistoryY, MYCILA_GRAPH_POINTS);
      jsy194Channel2ActivePowerHistory.setY(power2HistoryY, MYCILA_GRAPH_POINTS);

      break;
    }
    case MYCILA_JSY_MK_333: {
      jsy333PhaseAFrequency.setValue(prevData.phaseA().frequency);
      jsy333PhaseAVoltage.setValue(prevData.phaseA().voltage);
      jsy333PhaseACurrent.setValue(prevData.phaseA().current);
      jsy333PhaseAPowerFactor.setValue(prevData.phaseA().powerFactor);
      jsy333PhaseAActivePower.setValue(prevData.phaseA().activePower);
      jsy333PhaseAApparentPower.setValue(prevData.phaseA().apparentPower);
      jsy333PhaseAReactivePower.setValue(prevData.phaseA().reactivePower);
      jsy333PhaseAActiveEnergy.setValue(prevData.phaseA().activeEnergy);
      jsy333PhaseAActiveEnergyImported.setValue(prevData.phaseA().activeEnergyImported);
      jsy333PhaseAActiveEnergyReturned.setValue(prevData.phaseA().activeEnergyReturned);
      jsy333PhaseAReactiveEnergy.setValue(prevData.phaseA().reactiveEnergy);
      jsy333PhaseAReactiveEnergyImported.setValue(prevData.phaseA().reactiveEnergyImported);
      jsy333PhaseAReactiveEnergyReturned.setValue(prevData.phaseA().reactiveEnergyReturned);
      jsy333PhaseAApparentEnergy.setValue(prevData.phaseA().apparentEnergy);

      jsy333PhaseBFrequency.setValue(prevData.phaseB().frequency);
      jsy333PhaseBVoltage.setValue(prevData.phaseB().voltage);
      jsy333PhaseBCurrent.setValue(prevData.phaseB().current);
      jsy333PhaseBPowerFactor.setValue(prevData.phaseB().powerFactor);
      jsy333PhaseBActivePower.setValue(prevData.phaseB().activePower);
      jsy333PhaseBApparentPower.setValue(prevData.phaseB().apparentPower);
      jsy333PhaseBReactivePower.setValue(prevData.phaseB().reactivePower);
      jsy333PhaseBActiveEnergy.setValue(prevData.phaseB().activeEnergy);
      jsy333PhaseBActiveEnergyImported.setValue(prevData.phaseB().activeEnergyImported);
      jsy333PhaseBActiveEnergyReturned.setValue(prevData.phaseB().activeEnergyReturned);
      jsy333PhaseBReactiveEnergy.setValue(prevData.phaseB().reactiveEnergy);
      jsy333PhaseBReactiveEnergyImported.setValue(prevData.phaseB().reactiveEnergyImported);
      jsy333PhaseBReactiveEnergyReturned.setValue(prevData.phaseB().reactiveEnergyReturned);
      jsy333PhaseBApparentEnergy.setValue(prevData.phaseB().apparentEnergy);

      jsy333PhaseCFrequency.setValue(prevData.phaseC().frequency);
      jsy333PhaseCVoltage.setValue(prevData.phaseC().voltage);
      jsy333PhaseCCurrent.setValue(prevData.phaseC().current);
      jsy333PhaseCPowerFactor.setValue(prevData.phaseC().powerFactor);
      jsy333PhaseCActivePower.setValue(prevData.phaseC().activePower);
      jsy333PhaseCApparentPower.setValue(prevData.phaseC().apparentPower);
      jsy333PhaseCReactivePower.setValue(prevData.phaseC().reactivePower);
      jsy333PhaseCActiveEnergy.setValue(prevData.phaseC().activeEnergy);
      jsy333PhaseCActiveEnergyImported.setValue(prevData.phaseC().activeEnergyImported);
      jsy333PhaseCActiveEnergyReturned.setValue(prevData.phaseC().activeEnergyReturned);
      jsy333PhaseCReactiveEnergy.setValue(prevData.phaseC().reactiveEnergy);
      jsy333PhaseCReactiveEnergyImported.setValue(prevData.phaseC().reactiveEnergyImported);
      jsy333PhaseCReactiveEnergyReturned.setValue(prevData.phaseC().reactiveEnergyReturned);
      jsy333PhaseCApparentEnergy.setValue(prevData.phaseC().apparentEnergy);

      // shift array
      for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
        power1HistoryY[i] = power1HistoryY[i + 1];
        power2HistoryY[i] = power2HistoryY[i + 1];
        power3HistoryY[i] = power3HistoryY[i + 1];
      }

      // set new value
      power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(prevData.phaseA().activePower);
      power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(prevData.phaseB().activePower);
      power3HistoryY[MYCILA_GRAPH_POINTS - 1] = round(prevData.phaseC().activePower);

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

  // system
  disableLoopWDT();
  Mycila::System::init();

  // config
  preferences.begin("jsy", false);
  udpSendEnabled = preferences.getBool("udp_send", udpSendEnabled);

  // tasks
  dashboardTask.setEnabledWhen([]() { return espConnect.isConnected() && !dashboard.isAsyncAccessInProgress(); });
  dashboardTask.setInterval(1000);
  jsyTask.setEnabledWhen([]() { return jsy.isEnabled(); });
  jsyTaskManager.addTask(jsyTask);
  coreTaskManager.addTask(dashboardTask);
  coreTaskManager.addTask(networkManagerTask);
  coreTaskManager.addTask(networkUpTask);
  coreTaskManager.addTask(otaTask);
  coreTaskManager.addTask(restartTask);

  // WebSerial
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
  ElegantOTA.begin(&webServer);

  // Dashboard - Auth
  authMiddleware.setAuthType(AsyncAuthType::AUTH_DIGEST);
  authMiddleware.setRealm(hostname.c_str());
  authMiddleware.setUsername(MYCILA_ADMIN_USERNAME);
  authMiddleware.setPassword(MYCILA_ADMIN_PASSWORD);
  authMiddleware.generateHash();
  webServer.addMiddleware(&authMiddleware);

  // Dashboard - Routes
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

  // Dashboard - Callbacks
  restart.onChange([](bool state) { restartTask.resume(); });
  reset.onChange([](bool state) {
    espConnect.clearConfiguration();
    restartTask.resume();
  });
  energyReset.onChange([](bool state) {
    jsy.resetEnergy();
    energyReset.setValue(0);
    dashboard.refresh(energyReset);
  });
  publishDataCard.onChange([](bool state) {
    udpSendEnabled = state;
    preferences.putBool("udp_send", udpSendEnabled);
    publishDataCard.setValue(udpSendEnabled);
    dashboard.refresh(publishDataCard);
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

  // Dashboard - Widgets Values
  version.setValue(MYCILA_JSY_VERSION);
  for (int i = 0; i < MYCILA_GRAPH_POINTS; i++)
    historyX[i] = i - MYCILA_GRAPH_POINTS;
  jsy163ActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy194Channel1ActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy194Channel2ActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseAActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseBActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  jsy333PhaseCActivePowerHistory.setX(historyX, MYCILA_GRAPH_POINTS);
  dashboard.refreshLayout();
  dashboardTask.forceRun();

  // Network Manager
  espConnect.setAutoRestart(true);
  espConnect.setBlocking(CRC16_CCITT_FALSE_REV_IN);
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
        if (espConnect.getConfig().apMode) {
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

  // jsy
  jsy.setCallback([](const Mycila::JSY::EventType eventType, const Mycila::JSY::Data& data) {
    if (prevData == data)
      return;

    prevData = data;

    if (!udpSendEnabled) {
      messageRate = 0;
      dataRate = 0;
      return;
    }

    const Mycila::ESPConnect::Mode mode = espConnect.getMode();
    if (mode == Mycila::ESPConnect::Mode::NONE) {
      messageRate = 0;
      dataRate = 0;
      return;
    }

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
  });

  jsy.begin(MYCILA_JSY_SERIAL, MYCILA_JSY_RX, MYCILA_JSY_TX);

  if (jsy.isEnabled() && jsy.getBaudRate() != jsy.getMaxAvailableBaudRate())
    jsy.setBaudRate(jsy.getMaxAvailableBaudRate());

  jsyModel = jsy.getModel();

  if (jsyModel == MYCILA_JSY_MK_194 || jsyModel == MYCILA_JSY_MK_333 || jsyModel == MYCILA_JSY_MK_UNKNOWN) {
    dashboard.remove(jsy163Frequency);
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
    dashboard.remove(jsy194Channel1Frequency);
    dashboard.remove(jsy194Channel1Voltage);
    dashboard.remove(jsy194Channel1Current);
    dashboard.remove(jsy194Channel1PowerFactor);
    dashboard.remove(jsy194Channel1ActivePower);
    dashboard.remove(jsy194Channel1ApparentPower);
    dashboard.remove(jsy194Channel1ReactivePower);
    dashboard.remove(jsy194Channel1ActiveEnergy);
    dashboard.remove(jsy194Channel1ActiveEnergyImported);
    dashboard.remove(jsy194Channel1ActiveEnergyReturned);
    dashboard.remove(jsy194Channel2Frequency);
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
    dashboard.remove(jsy333PhaseAFrequency);
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
    dashboard.remove(jsy333PhaseBFrequency);
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
    dashboard.remove(jsy333PhaseCFrequency);
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

  coreTaskManager.asyncStart(512 * 8, 5, 1, 100, false);
  jsyTaskManager.asyncStart(512 * 8, 5, 1, 100, false);

  logger.info(TAG, "Started " MYCILA_APP_NAME "!");
}

void loop() {
  vTaskDelete(NULL);
}
