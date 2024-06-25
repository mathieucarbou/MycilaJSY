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

#ifndef MYCILA_ADMIN_PASSWORD
  #define MYCILA_ADMIN_PASSWORD ""
#endif

#define TAG                          "JSY-UDP"
#define MYCILA_APP_NAME              "JSY Remote UDP Listener"
#define MYCILA_ADMIN_USERNAME        "admin"
#define MYCILA_UDP_PORT              53964
#define MYCILA_UDP_MSG_TYPE_JSY_DATA 0x01
#define MYCILA_GRAPH_POINTS          120

#include <Arduino.h>
#include <ESPmDNS.h>

#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <AsyncTCP.h>          // https://github.com/mathieucarbou/AsyncTCP
#include <ESPAsyncWebServer.h> // https://github.com/mathieucarbou/ESPAsyncWebServer
#include <ESPDash.h>           // https://github.com/mathieucarbou/ayushsharma82-ESP-DASH#dev
#include <ElegantOTA.h>        // https://github.com/mathieucarbou/ayushsharma82-ElegantOTA#dev
#include <FastCRC32.h>         // https://github.com/RobTillaart/CRC
#include <MycilaESPConnect.h>  // https://github.com/mathieucarbou/MycilaESPConnect
#include <MycilaLogger.h>      // https://github.com/mathieucarbou/MycilaLogger
#include <MycilaSystem.h>      // https://github.com/mathieucarbou/MycilaSystem
#include <MycilaTaskManager.h> // https://github.com/mathieucarbou/MycilaTaskMonitor
#include <MycilaTaskMonitor.h> // https://github.com/mathieucarbou/MycilaTaskMonitor
#include <WebSerial.h>         // https://github.com/mathieucarbou/ayushsharma82-WebSerial#dev

typedef struct {
    float current1 = 0;        // A
    float current2 = 0;        // A
    float energy1 = 0;         // kWh
    float energy2 = 0;         // kWh
    float energyReturned1 = 0; // kWh
    float energyReturned2 = 0; // kWh
    float frequency = 0;       // Hz
    float power1 = 0;          // W
    float power2 = 0;          // W
    float powerFactor1 = 0;
    float powerFactor2 = 0;
    float voltage1 = 0; // V
    float voltage2 = 0; // V
} JSYData;

AsyncUDP udp;
AsyncWebServer webServer(80);
ESPDash dashboard = ESPDash(&webServer, "/dashboard", false);
Mycila::Logger logger;
Mycila::TaskManager coreTaskManager("core");

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

Card restart = Card(&dashboard, BUTTON_CARD, "Restart");
Card reset = Card(&dashboard, BUTTON_CARD, "Reset Device");

int historyX[MYCILA_GRAPH_POINTS] = {0};
int power1HistoryY[MYCILA_GRAPH_POINTS] = {0};
int power2HistoryY[MYCILA_GRAPH_POINTS] = {0};
Chart power1History = Chart(&dashboard, BAR_CHART, "Channel 1 Active Power (W)");
Chart power2History = Chart(&dashboard, BAR_CHART, "Channel 2 Active Power (W)");

String hostname;
String ssid;
JSYData jsyData;

const String toDHHMMSS(uint32_t seconds);
String getEspId();

const Mycila::TaskDoneCallback LOG_EXEC_TIME = [](const Mycila::Task& me, const uint32_t elapsed) {
  logger.debug(TAG, "%s in %" PRIu32 " us", me.getName(), elapsed);
};

Mycila::Task networkManagerTask("ESPConnect", [](void* params) { ESPConnect.loop(); });

Mycila::Task profilerTask("Profiler", [](void* params) {
  Mycila::TaskMonitor.log();
  coreTaskManager.log();
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

  // UDP
  udp.listen(MYCILA_UDP_PORT);
});

Mycila::Task otaTask("OTA", Mycila::TaskType::ONCE, [](void* params) {
  logger.info(TAG, "Preparing OTA update...");
});

Mycila::Task restartTask("Restart", Mycila::TaskType::ONCE, [](void* params) {
  logger.warn(TAG, "Restarting " MYCILA_APP_NAME "...");
  Mycila::System.restart(500);
});

Mycila::Task dashboardTask("Dashboard", [](void* params) {
  ESPConnectMode mode = ESPConnect.getMode();

  networkAPIP.set(ESPConnect.getIPAddress(ESPConnectMode::AP).toString().c_str());
  networkEthIP.set(ESPConnect.getIPAddress(ESPConnectMode::ETH).toString().c_str());
  networkInterface.set(mode == ESPConnectMode::AP ? "AP" : (mode == ESPConnectMode::STA ? "WiFi" : (mode == ESPConnectMode::ETH ? "Ethernet" : "")));
  networkWiFiIP.set(ESPConnect.getIPAddress(ESPConnectMode::STA).toString().c_str());
  networkWiFiRSSI.set((String(ESPConnect.getWiFiRSSI()) + " dBm").c_str());
  networkWiFiSignal.set((String(ESPConnect.getWiFiSignalQuality()) + " %").c_str());
  networkWiFiSSID.set(ESPConnect.getWiFiSSID().c_str());
  uptime.set(toDHHMMSS(Mycila::System.getUptime()).c_str());

  activePower1.update(jsyData.power1);
  apparentPower1.update(jsyData.powerFactor1 == 0 ? 0 : jsyData.power1 / jsyData.powerFactor1);
  current1.update(jsyData.current1);
  energy1.update(jsyData.energy1);
  energyReturned1.update(jsyData.energyReturned1);
  powerFactor1.update(jsyData.powerFactor1);
  voltage1.update(jsyData.voltage1);
  voltageDimmed1.update(jsyData.current1 == 0 ? 0 : jsyData.power1 / jsyData.current1);

  activePower2.update(jsyData.power2);
  apparentPower2.update(jsyData.powerFactor2 == 0 ? 0 : jsyData.power2 / jsyData.powerFactor2);
  current2.update(jsyData.current2);
  energy2.update(jsyData.energy2);
  energyReturned2.update(jsyData.energyReturned2);
  powerFactor2.update(jsyData.powerFactor2);
  voltage2.update(jsyData.voltage2);
  voltageDimmed2.update(jsyData.current2 == 0 ? 0 : jsyData.power2 / jsyData.current2);

  frequency.update(jsyData.frequency);

  // shift array
  for (size_t i = 0; i < MYCILA_GRAPH_POINTS - 1; i++) {
    power1HistoryY[i] = power1HistoryY[i + 1];
    power2HistoryY[i] = power2HistoryY[i + 1];
  }

  // set new value
  power1HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsyData.power1);
  power2HistoryY[MYCILA_GRAPH_POINTS - 1] = round(jsyData.power2);

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
  hostname = "Listener-" + getEspId();
  hostname.toLowerCase();
  ssid = "Listener-" + getEspId();

  // logging
  esp_log_level_set("*", static_cast<esp_log_level_t>(ARDUHAL_LOG_LEVEL_DEBUG));
  logger.setLevel(ARDUHAL_LOG_LEVEL_DEBUG);
  logger.forwardTo(&Serial);
  logger.warn(TAG, "Booting " MYCILA_APP_NAME "...");

  logger.warn(TAG, "Initializing " MYCILA_APP_NAME "...");

  // system
  Mycila::System.begin();

  // tasks
  dashboardTask.setEnabledWhen([]() { return ESPConnect.isConnected() && dashboard.hasClient() && !dashboard.isAsyncAccessInProgress(); });
  dashboardTask.setInterval(1000 * Mycila::TaskDuration::MILLISECONDS);
  dashboardTask.setManager(coreTaskManager);
  Mycila::TaskManager::configureWDT();
  networkManagerTask.setManager(coreTaskManager);
  networkUpTask.setManager(coreTaskManager);
  otaTask.setManager(coreTaskManager);
  profilerTask.setInterval(10 * Mycila::TaskDuration::SECONDS);
  profilerTask.setManager(coreTaskManager);
  restartTask.setManager(coreTaskManager);

  // profiling
  dashboardTask.enableProfiling(10, Mycila::TaskTimeUnit::MILLISECONDS);
  otaTask.setCallback(LOG_EXEC_TIME);
  profilerTask.enableProfiling(10, Mycila::TaskTimeUnit::MILLISECONDS);
  profilerTask.setCallback(LOG_EXEC_TIME);

  // Task Monitor
  Mycila::TaskMonitor.begin();
  Mycila::TaskMonitor.addTask("arduino_events");          // Network
  Mycila::TaskMonitor.addTask("async_tcp");               // AsyncTCP
  Mycila::TaskMonitor.addTask("async_udp");               // AsyncUDP
  Mycila::TaskMonitor.addTask("wifi");                    // WiFI
  Mycila::TaskMonitor.addTask(coreTaskManager.getName()); // App

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
  webServer.rewrite("/", "/dashboard").setFilter([](AsyncWebServerRequest* request) { return ESPConnect.getState() != ESPConnectState::PORTAL_STARTED; });

  restart.attachCallback([](int32_t value) { restartTask.resume(); });

  reset.attachCallback([](int32_t value) {
    ESPConnect.clearConfiguration();
    restartTask.resume();
  });

  for (int i = 0; i < MYCILA_GRAPH_POINTS; i++)
    historyX[i] = i - MYCILA_GRAPH_POINTS;
  power1History.updateX(historyX, MYCILA_GRAPH_POINTS);
  power2History.updateX(historyX, MYCILA_GRAPH_POINTS);

  dashboard.onBeforeUpdate([](bool changes_only) {
    if (!changes_only) {
      logger.debug(TAG, "Dashboard refresh requested");
      networkAPMAC.set(ESPConnect.getMACAddress(ESPConnectMode::AP).c_str());
      networkEthMAC.set(ESPConnect.getMACAddress(ESPConnectMode::ETH).isEmpty() ? "N/A" : ESPConnect.getMACAddress(ESPConnectMode::ETH).c_str());
      networkHostname.set(hostname.c_str());
      networkWiFiMAC.set(ESPConnect.getMACAddress(ESPConnectMode::STA).c_str());
    }
  });
  dashboard.setAuthentication(MYCILA_ADMIN_USERNAME, MYCILA_ADMIN_PASSWORD);
  dashboard.refreshLayout();
  dashboardTask.forceRun();

  // UDP
  udp.onPacket([](AsyncUDPPacket packet) {
    // [0] uint8_t == message type
    // [1] sizeof(Mycila::JSYData)
    // [sizeOfBody] uint32_t == CRC32
    constexpr size_t sizeOfJSYData = sizeof(JSYData);
    constexpr size_t sizeOfBody = 1 + sizeOfJSYData;
    constexpr size_t sizeOfCRC32 = sizeof(uint32_t);
    constexpr size_t sizeOfUDP = sizeOfBody + sizeOfCRC32;

    size_t len = packet.length();
    if (len != sizeOfUDP)
      return;

    uint8_t* data = packet.data();
    const uint8_t type = data[0];

    if (type != MYCILA_UDP_MSG_TYPE_JSY_DATA)
      return;

    FastCRC32 crc32;
    crc32.add(data, sizeOfBody);
    uint32_t crc = crc32.calc();
    if (memcmp(&crc, data + sizeOfBody, sizeOfCRC32) != 0)
      return;

    memcpy(&jsyData, data + 1, sizeOfJSYData);
  });

  // Network Manager
  ESPConnect.setAutoRestart(true);
  ESPConnect.setBlocking(false);
  ESPConnect.listen([](ESPConnectState previous, ESPConnectState state) {
    logger.debug(TAG, "NetworkState: %s => %s", ESPConnect.getStateName(previous), ESPConnect.getStateName(state));
    switch (state) {
      case ESPConnectState::NETWORK_DISABLED:
        logger.warn(TAG, "Disabled Network!");
        break;
      case ESPConnectState::AP_STARTING:
        logger.info(TAG, "Starting Access Point %s...", ESPConnect.getAccessPointSSID().c_str());
        break;
      case ESPConnectState::AP_STARTED:
        logger.info(TAG, "Access Point %s started with IP address %s", ESPConnect.getWiFiSSID().c_str(), ESPConnect.getIPAddress().toString().c_str());
        networkUpTask.resume();
        break;
      case ESPConnectState::NETWORK_CONNECTING:
        logger.info(TAG, "Connecting to network...");
        break;
      case ESPConnectState::NETWORK_CONNECTED:
        logger.info(TAG, "Connected with IP address %s", ESPConnect.getIPAddress().toString().c_str());
        networkUpTask.resume();
        break;
      case ESPConnectState::NETWORK_TIMEOUT:
        logger.warn(TAG, "Unable to connect!");
        break;
      case ESPConnectState::NETWORK_DISCONNECTED:
        logger.warn(TAG, "Disconnected!");
        break;
      case ESPConnectState::NETWORK_RECONNECTING:
        logger.info(TAG, "Trying to reconnect...");
        break;
      case ESPConnectState::PORTAL_STARTING:
        logger.info(TAG, "Starting Captive Portal %s for %" PRIu32 " seconds...", ESPConnect.getAccessPointSSID().c_str(), ESPConnect.getCaptivePortalTimeout());
        break;
      case ESPConnectState::PORTAL_STARTED:
        logger.info(TAG, "Captive Portal started at %s with IP address %s", ESPConnect.getWiFiSSID().c_str(), ESPConnect.getIPAddress().toString().c_str());
        break;
      case ESPConnectState::PORTAL_COMPLETE: {
        if (ESPConnect.hasConfiguredAPMode()) {
          logger.info(TAG, "Captive Portal: Access Point configured");
        } else {
          logger.info(TAG, "Captive Portal: WiFi configured");
        }
        break;
      }
      case ESPConnectState::PORTAL_TIMEOUT:
        logger.warn(TAG, "Captive Portal: timed out.");
        break;
      default:
        break;
    }
  });
  ESPConnect.begin(webServer, hostname, ssid, MYCILA_ADMIN_PASSWORD);

  // start tasks
  assert(coreTaskManager.asyncStart(1024 * 4, 1, 1, 100, true)); // NOLINT

  logger.info(TAG, "Started " MYCILA_APP_NAME "!");
}

// Destroy default Arduino async task
void loop() { vTaskDelete(NULL); }

const String toDHHMMSS(uint32_t seconds) {
  const uint8_t days = seconds / 86400;
  seconds = seconds % (uint32_t)86400;
  const uint8_t hh = seconds / 3600;
  seconds = seconds % (uint32_t)3600;
  const uint8_t mm = seconds / 60;
  const uint8_t ss = seconds % (uint32_t)60;
  char buffer[14];
  snprintf(buffer, sizeof(buffer), "%" PRIu8 "d %02" PRIu8 ":%02" PRIu8 ":%02" PRIu8, days % 1000, hh % 100, mm % 100, ss % 100);
  return buffer;
}

String getEspId() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
}
