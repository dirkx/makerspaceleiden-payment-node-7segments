// dirkx@webweaving.org, apache license, for the makerspaceleiden.nl
//
// Tools settings:
//  Board ESP32 WRover Module
//  Port: [the COM port your board has connected to]
//
// Boards used:
//    ESP32 WRover Module
//    MFRC522 (https://www.tinytronics.nl/shop/nl/communicatie-en-signalen/draadloos/rfid/rfid-kit-mfrc522-s50-mifare-met-kaart-en-key-tag)
//    7 segment display ( https://www.hobbyelectronica.nl/product/4-digit-klok-display-module)
//
// Additional Librariries (via Sketch -> library manager):
//    MFRC522-spi-i2c-uart-async
//    TM1637TinyDisplay
//    Arduino_JSON
//    PubSub (for MQTT)

#ifndef WIFI_NETWORK
#define WIFI_NETWORK "MyWifiNetwork"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "MyWifiPassword"
#endif

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Arduino_JSON.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TM1637TinyDisplay.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "global.h"
#include "log.h"
#include "rfid.h"
#include "rest.h"

#include "TelnetSerialStream.h"

// Wiring of 7 segment display - just
// two wires. See https://www.mcielectronics.cl/website_MCI/static/documents/Datasheet_TM1637.pdf
// and https://create.arduino.cc/projecthub/ryanchan/tm1637-digit-display-arduino-quick-tutorial-ca8a93
//
#define DISPLAY_CLK 25
#define DISPLAY_DIO 26

TM1637TinyDisplay display(DISPLAY_CLK, DISPLAY_DIO);

// Very ugly global vars - used to communicate between the REST call and the rest.
//
char terminalName[64] = TERMINAL_NAME;
int NA = 0;
char **amounts = NULL;
char **prices = NULL;
char **descs = NULL;
int amount = 0;
int default_item = -1;
double amount_no_ok_needed = AMOUNT_NO_OK_NEEDED;
const char * version = VERSION;
unsigned long device_specific_reboot_offset;
String label;
state_t md = BOOT;

TelnetSerialStream telnetSerialStream = TelnetSerialStream();

#ifdef SYSLOG_HOST
#include "SyslogStream.h"
SyslogStream syslogStream = SyslogStream();
#endif

#ifdef MQTT_HOST
#include "MqttlogStream.h"
// EthernetClient client;
WiFiClient client;
MqttStream mqttStream = MqttStream(&client, MQTT_HOST);
char topic[128] = "log/" TERMINAL_NAME;
#endif

TLog Log, Debug;

void setupDisplay() {
  display.setBrightness(BRIGHT_7);
  display.showString(VERSION);
  Log.println("Display set to " VERSION);
}

void setup()
{
  Serial.begin(115200);
  md = BOOT;

  byte mac[6];
  WiFi.macAddress(mac);
  snprintf(terminalName, sizeof(terminalName), "%s-%s-%02x%02x%02x", TERMINAL_NAME, 1 + VERSION, mac[3], mac[4], mac[5]);
  device_specific_reboot_offset = (*(unsigned short*)(mac + 4)) % 3600;

  Serial.println("\n\n\Build: " __DATE__ " " __TIME__ "\nUnit:  " __FILE__);
  Serial.println(terminalName);

  setupDisplay();
  setupAuth(terminalName);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWD);

  Log.print("Connecting to ");
  Log.println(WIFI_NETWORK);
  yield();

  display.showString("conn");

  WiFi.setHostname(terminalName);

  Log.print(terminalName);
  Log.println(" Wifi connecting");

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Log.println("Rebooting, wifi issue" );
    display.showString("FAIL");
    delay(5000);
    ESP.restart();
  };
  Log.println("Connected.");
  yield();

  // try to get some reliable time; to stop my cert
  // checking code complaining.
  configTime(0, 0, NTP_SERVER);
  yield();

  Log.addPrintStream(std::make_shared<TelnetSerialStream>(telnetSerialStream));
  Debug.addPrintStream(std::make_shared<TelnetSerialStream>(telnetSerialStream));

#ifdef SYSLOG_HOST
  syslogStream.setDestination(SYSLOG_HOST);
  syslogStream.setRaw(false); // wether or not the syslog server is a modern(ish) unix.
#ifdef SYSLOG_PORT
  syslogStream.setPort(SYSLOG_PORT);
#endif
  Log.addPrintStream(std::make_shared<SyslogStream>(syslogStream));
#endif

#ifdef MQTT_HOST
#ifdef MQTT_TOPIC_PREFIX
  snprintf(topic, sizeof(topic), "%s/log/%s", MQTT_TOPIC_PREFIX, terminalName);
  mqttStream.setTopic(topic);
#endif
  Log.addPrintStream(std::make_shared<MqttStream>(mqttStream));
#endif

  Log.begin();
  char * p =  __FILE__;
  if (rindex(p, '/')) p = rindex(p, '/') + 1;

  Log.printf( "File:     %s\n", p);
  Log.println("Firmware: " TERMINAL_NAME "-" VERSION);
  Log.println("Build:    " __DATE__ " " __TIME__ );
  Log.print(  "Unit:     ");
  Log.println(terminalName);

  setupRFID();

  ArduinoOTA.setHostname(terminalName);
#ifdef OTA_HASH
  ArduinoOTA.setPasswordHash(OTA_HASH);
#else
#ifdef OTA_PASSWD
  ArduinoOTA.setPassword(OTA_PASSWD);
#endif
#endif

  ArduinoOTA
  .onStart([]() {
    display.showString("PROG");
    Log.println("OTA started");
  })
  .onEnd([]() {
    // prevent a cleversod uplading a special binary. ignore the hardware/serial angle
    Log.println("OTA completed OK - need to wipe keystore before rebooting into new code.");
    wipekeys();
    display.showString("Done");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    static int l = -100;
    int w = 100 * progress / total;
    if (w != l) {
      char fw[5];
      snprintf(fw, sizeof(fw), "F%3d", w);
      display.showString(fw);
    };
    l = w;
  })
  .onError([](ota_error_t error) {
    char * label;
    if (error == OTA_AUTH_ERROR) label = "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) label = "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) label = "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) label = "Receive Failed";
    else if (error == OTA_END_ERROR) label = "End Failed";
    else label = "Uknown error";
    display.showString(label);
    Log.print("OTA error");
    Log.println(label);
  });

  Debug.println("Starting loop");

  display.setBrightness(BRIGHT_HIGH / 3);
  display.showString("----");
  ArduinoOTA.begin();

  md = WAITING_FOR_NTP;
}

static void loop_RebootAtMidnight() {
  static unsigned long lst = millis();
  if (millis() - lst < 60 * 1000)
    return;
  lst = millis();

  static unsigned long debug = 0;
  if (millis() - debug > 60 * 60 * 1000) {
    debug = millis();
    time_t now = time(nullptr);
    char * p = ctime(&now);
    p[5 + 11 + 3] = 0;
    Log.printf("%s Heap: %d Kb\n", p, (512 + heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) / 1024UL);
  }
  time_t now = time(nullptr);
  if (now < 3600)
    return;

#ifdef AUTO_REBOOT_TIME
  now += device_specific_reboot_offset;
  char * p = ctime(&now);
  //  0123456789012345678 9 0
  // "Thu Nov  4 09:47:43\n\0" -> 09:47\0
  p += 11;
  p[5] = 0;
  Debug.printf("strncmp(\"%s\",\"%s\",%d), %u, %u\n", p, AUTO_REBOOT_TIME, strlen(AUTO_REBOOT_TIME), now, millis());

  if (strncmp(p, AUTO_REBOOT_TIME, strlen(AUTO_REBOOT_TIME)) == 0 && millis() > 3600 ) {
    Log.println("Nightly reboot - also to fetch new pricelist and fix any memory eaks.");
    ESP.restart();
  }
#endif
}

void loop()
{
  Log.loop();
  ArduinoOTA.handle();
  loop_RebootAtMidnight();

  switch (md) {
    case WAITING_FOR_NTP:
      display.showString("ntp");
      if (time(nullptr) > 3600)
        md = FETCH_CA;
      return;
      break;
    case FETCH_CA:
      display.showString("F CA");
      fetchCA();
      return;
    case REGISTER:
      display.showString("reg");
      registerDevice();
      return;
    case WAIT_FOR_REGISTER_SWIPE:
      display.showString("pair");
      if (loopRFID()) {
        display.showString("vrfy");
        registerDevice();
      };
      return;
    case REGISTER_PRICELIST:
      display.showString("F PL");
      if (fetchPricelist())
        md = ENTER_AMOUNT;
      return;
    case ENTER_AMOUNT:
      display.setBrightness(BRIGHT_HIGH / 3);
      display.showNumber(atof(prices[amount]), 2);
      if (NA > 0 && loopRFID()) {
        display.setBrightness(BRIGHT_HIGH);
        display.showString("[--]");
        int status = payByREST(tag, prices[amount], descs[amount]);
        if (status == 200) {
          display.showString("PAID");
          delay(500);
        } else {
          for (int i = 0; i < 4; i++) {
            display.showString("FAIL");
            delay(500);
            display.showNumber(status);
            delay(500);
          };
        };
        display.setBrightness(BRIGHT_HIGH / 3);
        display.showNumber(amount, 2);
      };
      break;
    case WIFI_FAIL_REBOOT:
      { static unsigned long last = millis();
        if (millis() - last > 10 * 1000)
          ESP.restart();
        display.showString("FFER");
      };
      break;
    default:
      break;
  };
}
