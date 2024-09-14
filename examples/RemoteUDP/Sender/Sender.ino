#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
#endif

#ifndef HTTPCLIENT_NOSECURE
  #define HTTPCLIENT_NOSECURE 1
#endif

#ifndef ARDUINO_LOOP_STACK_SIZE
  #define ARDUINO_LOOP_STACK_SIZE 4096
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

#ifndef DASH_JSON_SIZE
  #define DASH_JSON_SIZE 4096
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
  #define MYCILA_JSY_RX 16
#endif

#ifndef MYCILA_JSY_TX
  #define MYCILA_JSY_TX 17
#endif

#ifndef MYCILA_ADMIN_PASSWORD
  #define MYCILA_ADMIN_PASSWORD ""
#endif

#define MYCILA_ADMIN_USERNAME        "admin"
#define MYCILA_APP_NAME              "JSY Remote UDP Sender"
#define MYCILA_GRAPH_POINTS          120
#define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x01
#define MYCILA_UDP_PORT              53964
#define MYCILA_UDP_SEND_INTERVAL_MS  200
#define MYCILA_UDP_SEND_RATE_WINDOW  50
#define TAG                          "JSY-UDP"

#include <Arduino.h>
#include <ESPmDNS.h>

#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>             // https://github.com/mathieucarbou/AsyncTCP
#include <ESPAsyncWebServer.h>    // https://github.com/mathieucarbou/ESPAsyncWebServer
#include <ESPDash.h>              // https://github.com/mathieucarbou/ayushsharma82-ESP-DASH#dev
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

Card voltage1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Voltage", "V");
Card activePower1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Active Power", "W");
Card apparentPower1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Apparent Power", "VA");
Card powerFactor1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Power Factor");

Card voltageDimmed1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Dimmed Voltage", "V");
Card current1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Current", "A");
Card energy1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Imported Energy", "kWh");
Card energyReturned1 = Card(&dashboard, GENERIC_CARD, "Channel 1 Exported Energy", "kWh");

Card voltage2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Voltage", "V");
Card activePower2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Active Power", "W");
Card apparentPower2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Apparent Power", "VA");
Card powerFactor2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Power Factor");

Card voltageDimmed2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Dimmed Voltage", "V");
Card current2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Current", "A");
Card energy2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Imported Energy", "kWh");
Card energyReturned2 = Card(&dashboard, GENERIC_CARD, "Channel 2 Exported Energy", "kWh");

Card frequency = Card(&dashboard, GENERIC_CARD, "Grid Frequency", "Hz");

Card messageRateCard = Card(&dashboard, GENERIC_CARD, "Message Rate", "msg/s");
Card dataRateCard = Card(&dashboard, GENERIC_CARD, "Data Rate", "bytes/s");

Card udpSendEnabledCard = Card(&dashboard, BUTTON_CARD, "Enable UDP Transfer");
Card restart = Card(&dashboard, BUTTON_CARD, "Restart");
Card energyReset = Card(&dashboard, BUTTON_CARD, "Reset JSY");
Card reset = Card(&dashboard, BUTTON_CARD, "Reset Device");

int historyX[MYCILA_GRAPH_POINTS] = {0};
int power1HistoryY[MYCILA_GRAPH_POINTS] = {0};
int power2HistoryY[MYCILA_GRAPH_POINTS] = {0};
Chart power1History = Chart(&dashboard, BAR_CHART, "Channel 1 Active Power (W)");
Chart power2History = Chart(&dashboard, BAR_CHART, "Channel 2 Active Power (W)");

String hostname;
String ssid;

bool udpSendEnabled = true;

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

  activePower1.update(jsy.getPower1());
  apparentPower1.update(jsy.getApparentPower1());
  current1.update(jsy.getCurrent1());
  energy1.update(jsy.getEnergy1());
  energyReturned1.update(jsy.getEnergyReturned1());
  powerFactor1.update(jsy.getPowerFactor1());
  voltage1.update(jsy.getVoltage1());
  voltageDimmed1.update(jsy.getDimmedVoltage1());

  activePower2.update(jsy.getPower2());
  apparentPower2.update(jsy.getApparentPower2());
  current2.update(jsy.getCurrent2());
  energy2.update(jsy.getEnergy2());
  energyReturned2.update(jsy.getEnergyReturned2());
  powerFactor2.update(jsy.getPowerFactor2());
  voltage2.update(jsy.getVoltage2());
  voltageDimmed2.update(jsy.getDimmedVoltage2());

  frequency.update(jsy.getFrequency());

  messageRateCard.update(messageRate);
  dataRateCard.update(static_cast<int>(dataRate));

  udpSendEnabledCard.update(udpSendEnabled);

  // shift array
  for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
    power1HistoryY[i] = power1HistoryY[i + 1];
    power2HistoryY[i] = power2HistoryY[i + 1];
  }

  // set new value
  power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.getPower1());
  power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsy.getPower2());

  // update charts
  power1History.updateY(power1HistoryY, MYCILA_GRAPH_POINTS);
  power2History.updateY(power2HistoryY, MYCILA_GRAPH_POINTS);

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

  version.set(MYCILA_JSY_VERSION);

  restart.attachCallback([](int32_t value) { restartTask.resume(); });

  reset.attachCallback([](int32_t value) {
    espConnect.clearConfiguration();
    restartTask.resume();
  });

  for (int i = 0; i < MYCILA_GRAPH_POINTS; i++)
    historyX[i] = i - MYCILA_GRAPH_POINTS;
  power1History.updateX(historyX, MYCILA_GRAPH_POINTS);
  power2History.updateX(historyX, MYCILA_GRAPH_POINTS);

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
  jsy.setCallback([](const Mycila::JSYEventType eventType) {
    if (!udpSendEnabled) {
      messageRate = 0;
      dataRate = 0;
      return;
    }

    if (eventType == Mycila::JSYEventType::EVT_CHANGE) {

      Mycila::ESPConnect::Mode mode = espConnect.getMode();

      if (mode != Mycila::ESPConnect::Mode::NONE) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["c1"] = jsy.getCurrent1();
        root["c2"] = jsy.getCurrent2();
        root["e1"] = jsy.getEnergy1();
        root["e2"] = jsy.getEnergy2();
        root["er1"] = jsy.getEnergyReturned1();
        root["er2"] = jsy.getEnergyReturned2();
        root["f"] = jsy.getFrequency();
        root["p1"] = jsy.getPower1();
        root["p2"] = jsy.getPower2();
        root["pf1"] = jsy.getPowerFactor1();
        root["pf2"] = jsy.getPowerFactor2();
        root["v1"] = jsy.getVoltage1();
        root["v2"] = jsy.getVoltage2();

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
  if (jsy.isEnabled() && jsy.getBaudRate() != Mycila::JSYBaudRate::BAUD_38400)
    jsy.setBaudRate(Mycila::JSYBaudRate::BAUD_38400);

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
  assert(coreTaskManager.asyncStart(1024 * 4, 1, 1, 100, true)); // NOLINT
  assert(jsyTaskManager.asyncStart(1024 * 3, 5, 0, 100, true));  // NOLINT

  logger.info(TAG, "Started " MYCILA_APP_NAME "!");
}

// Destroy default Arduino async task
void loop() { vTaskDelete(NULL); }
