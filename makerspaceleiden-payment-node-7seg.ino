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
//


#ifndef WIFI_NETWORK
#define WIFI_NETWORK "MyWifiNetwork"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "MyWifiPassword"
#endif

#ifndef PAYMENT_TERMINAL_BEARER
// Must match line 245 in  makerspaceleiden/settings.py of https://github.com/MakerSpaceLeiden/makerspaceleiden-crm
#define PAYMENT_TERMINAL_BEARER "not-so-very-secret-127.0.0.1"
#endif

#ifndef PAYMENT_URL
#define PAYMENT_URL "https://test.makerspaceleiden.nl/test-server-crm/api/v1/pay"
#endif

#ifndef SDU_URL
#define SDU_URL "https://test.makerspaceleiden.nl/test-server-crm/api/v1/pay"
#endif

#ifndef SKU_URL
#define SKU_URL "https://test.makerspaceleiden.nl/test-server-crm/api/v1/sku"
#endif

#ifndef SKU
#define SKU 1
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
#include "rest.h"
#include "SyslogStream.h"
#include "TelnetSerialStream.h"

#define DISPLAY_CLK 25
#define DISPLAY_DIO 26

TM1637TinyDisplay display(DISPLAY_CLK, DISPLAY_DIO);

#if SKU == 1
#define RFID_RESET    22
#define RFID_IRQ      21
#define RFID_CS       15
#define RFID_SCLK     18
#define RFID_MOSI     23
#define RFID_MISO     19
#else
#define RFID_CS       5 // default VSPI wiring
#define RFID_SCLK     18
#define RFID_MOSI     23
#define RFID_MISO     19
#define RFID_RESET    21 // these two pins swapped on the older beer-node.
#define RFID_IRQ      22
#endif

SPIClass RFID_SPI(VSPI);
MFRC522_SPI spiDevice = MFRC522_SPI(RFID_CS, RFID_RESET, &RFID_SPI);
MFRC522 mfrc522 = MFRC522(&spiDevice);

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
char tag[128];
state_t md = BOOT;

#ifdef SYSLOG_HOST
SyslogStream syslogStream = SyslogStream();
#endif
TelnetSerialStream telnetSerialStream = TelnetSerialStream();

TLog Log, Debug;

static void setupRFID()
{
  RFID_SPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
  mfrc522.PCD_Init();
  Log.print("RFID Scanner: ");
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
}

static int loopRFID() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    Log.println("Bad read (was card removed too quickly ? )");
    return 0;
  }
  if (mfrc522.uid.size < 3) {
    Log.println("Bad card (size tool small)");
    return 0;
  };

  // We're somewhat strict on the parsing/what we accept; as we use it unadultared in the URL.
  //
  if (mfrc522.uid.size > sizeof(mfrc522.uid.uidByte)) {
    Log.println("Too large a card id size. Ignoring.)");
    return 0;
  }

  memset(tag, 0, sizeof(tag));
  for (int i = 0; i < mfrc522.uid.size; i++) {
    char buff[5]; // 3 digits, dash and \0.
    snprintf(buff, sizeof(buff), "%s%d", i ? "-" : "", mfrc522.uid.uidByte[i]);
    strncat(tag, buff, sizeof(tag) - 1);
  };
  Log.println("Good scan");

  mfrc522.PICC_HaltA();
  return 1;
}

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
  snprintf(terminalName, sizeof(terminalName), "%s-%s-%02x%02x%02x", TERMINAL_NAME, VERSION, mac[3], mac[4], mac[5]);
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


#ifdef SYSLOG_HOST
  syslogStream.setDestination(SYSLOG_HOST);
  syslogStream.setRaw(true);
#ifdef SYSLOG_PORT
  syslogStream.setPort(SYSLOG_PORT);
#endif
  Log.addPrintStream(std::make_shared<SyslogStream>(syslogStream));
  Debug.addPrintStream(std::make_shared<SyslogStream>(syslogStream));
#endif

  Log.addPrintStream(std::make_shared<TelnetSerialStream>(telnetSerialStream));
  Debug.addPrintStream(std::make_shared<TelnetSerialStream>(telnetSerialStream));

  Log.begin();
  Log.println("\n\n\Build: " __DATE__ " " __TIME__ "\nUnit:  " __FILE__);
  Log.println(terminalName);

  // try to get some reliable time; to stop my cert
  // checking code complaining.
  configTime(0, 0, "nl.pool.ntp.org");
  yield();

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


  RFID_SPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

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
  p += 11;
  p[5] = 0;

  if (strncmp(p, AUTO_REBOOT_TIME, strlen(AUTO_REBOOT_TIME)) == 0 && millis() > 3600UL) {
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
      if (loopRFID())
        registerDevice();
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
        display.showString("NET FAIL");
      };
      break;
    default:
      break;
  };
}
