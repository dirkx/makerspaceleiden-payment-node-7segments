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

#define VERSION "F1-01"

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

#ifndef TERMINAL_NAME
#define TERMINAL_NAME "Bierplaat-MSL"
#endif

#ifndef SKU
#define SKU 1
#endif

#define HTTP_TIMEOUT (15000)


#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "ca_root.h"
#include <Arduino_JSON.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TM1637TinyDisplay.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define DISPLAY_CLK 25
#define DISPLAY_DIO 26

TM1637TinyDisplay display(DISPLAY_CLK, DISPLAY_DIO);

#define RFID_CS       15 // default VSPI wiring
#define RFID_SCLK     18
#define RFID_MOSI     23
#define RFID_MISO     19

#define RFID_RESET    22
#define RFID_IRQ      21

SPIClass RFID_SPI(VSPI);
MFRC522_SPI spiDevice = MFRC522_SPI(RFID_CS, RFID_RESET, &RFID_SPI);
MFRC522 mfrc522 = MFRC522(&spiDevice);

// Very ugly global vars - used to communicate between the REST call and the rest.
//
char tag[sizeof(mfrc522.uid.uidByte) * 4 + 1 ] = { 0 };
char * description = "no-price-list";
double amount = 0.00;

enum { BOOT = 0, PRICES, RUN } state = BOOT;

static int setupRFID()
{
  RFID_SPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
  mfrc522.PCD_Init();
  Serial.print("RFID Scanner: ");
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
}

static int loopRFID() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Bad read (was card removed too quickly ? )");
    return 0;
  }
  if (mfrc522.uid.size < 3) {
    Serial.println("Bad card (size tool small)");
    return 0;
  };

  // We're somewhat strict on the parsing/what we accept; as we use it unadultared in the URL.
  //
  if (mfrc522.uid.size > sizeof(mfrc522.uid.uidByte)) {
    Serial.println("Too large a card id size. Ignoring.)");
    return 0;
  }

  memset(tag, 0, sizeof(tag));
  for (int i = 0; i < mfrc522.uid.size; i++) {
    char buff[5]; // 3 digits, dash and \0.
    snprintf(buff, sizeof(buff), "%s%d", i ? "-" : "", mfrc522.uid.uidByte[i]);
    strncat(tag, buff, sizeof(tag) - 1);
  };
  Serial.println("Good scan");

  mfrc522.PICC_HaltA();
  return 1;
}


static unsigned char hex_digit(unsigned char c) {
  return "0123456789ABCDEF"[c & 0x0F];
};

char *_argencode(char *dst, size_t n, char *src)
{
  char c, *d = dst;
  while ((c = *src++) != 0)
  {
    if (c == ' ') {
      *d++ = '+';
    } else if (strchr("!*'();:@&=+$,/?%#[] ", c) || c < 32 || c > 127 ) {
      *d++ = '%';
      *d++ = hex_digit(c >> 4);
      *d++ = hex_digit(c);
    } else {
      *d++ = c;
    };
    if (d + 1 >= dst + n) {
      Serial.println("Warning - buffer was too small. Truncating.");
      break;
    }
  };
  *d++ = '\0';
  return dst;
}

JSONVar rest(const char *url, int * statusCode) {
  WiFiClientSecure *client = new WiFiClientSecure;
  String label = "unset";
  HTTPClient https;
  static JSONVar res;

  client->setCACert(ca_root);
  // client->setInsecure();
  https.setTimeout(HTTP_TIMEOUT);

  // Serial.println(url);

  if (!https.begin(*client, url)) {
    Serial.println("setup fail");
    return 999;
  };

#ifdef PAYMENT_TERMINAL_BEARER
  https.addHeader("X-Bearer", PAYMENT_TERMINAL_BEARER);
#endif
  int httpCode = https.GET();

  Serial.print("Result: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = https.getString();
    bool ok = false;

    Serial.print("Payload: ");
    Serial.println(payload);
    res = JSON.parse(payload);
  };

  if (httpCode != 200) {
    label = https.errorToString(httpCode);

    if (label.length() < 2)
      label = https.getString();

    Serial.print("REST payment call failed: ");
    Serial.print(httpCode);
    Serial.print("-");
    Serial.println(label);
  };
  https.end();
  *statusCode = httpCode;

  return res;
}

int setupPrices(int mySKU) {
  char buff[512];
  int httpCode = 0;

  snprintf(buff, sizeof(buff), SKU_URL "/%d", mySKU);
  JSONVar res = rest(buff, &httpCode);

  // {"id": 1, "name": "Bier", "description": "Bier, Pijpje of flesje", "price": 1.0}
  if (httpCode == 200) {
    description = strdup(res["description"]);
    amount = res["price"];

    if (amount >= 0.05 && amount <= 10.00 && strlen(description) > 1)
      return 200;
  };
  return httpCode == 200 ? 999 : httpCode;
}

int payByREST(char *tag, float amount) {
  char buff[512];
  char desc[128];
  char tmp[128];

  snprintf(desc, sizeof(desc), "%s. Payment at terminal %s", description, TERMINAL_NAME);

  // avoid logging the tag for privacy/security-by-obscurity reasons.
  //
  snprintf(buff, sizeof(buff), PAYMENT_URL "?node=%s&src=%s&amount=%.2f&description=%s",
           TERMINAL_NAME, "XX-XX-XX-XXX", amount, _argencode(tmp, sizeof(tmp), desc));
  Serial.print("URL: ");
  Serial.println(buff);

  snprintf(buff, sizeof(buff), PAYMENT_URL "?node=%s&src=%s&amount=%.2f&description=%s",
           TERMINAL_NAME, tag, amount, _argencode(tmp, sizeof(tmp), desc));

  int httpCode = 0;
  JSONVar res = rest(buff, &httpCode);

  if (httpCode == 200) {
    bool ok = false;
    if (res.hasOwnProperty("result"))
      ok = (bool) res["result"];

    if (!ok) {
      Serial.println("200 Ok, but false / incpmplete result.");
      httpCode = 600;
    }
  };

  return httpCode;
}

void setupDisplay() {
  display.setBrightness(BRIGHT_7);
  display.showString(VERSION);
  Serial.println("Display set to " VERSION);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n\nStarted, build " __DATE__ " " __TIME__ "\n" __FILE__);
  setupDisplay();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWD);

  Serial.print("Connecting to ");
  Serial.println(WIFI_NETWORK);
  yield();

  display.showString("conn");

  byte mac[6];
  char terminalName[128];

  WiFi.macAddress(mac);
  snprintf(terminalName, sizeof(terminalName), "%s-2x%02x%02x", TERMINAL_NAME, mac[3], mac[4], mac[5]);
  Serial.print(terminalName);
  Serial.println(" Wifi connecting");

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Rebooting, wifi issue" );
    display.showString("FAIL");
    delay(5000);
    ESP.restart();
  };
  Serial.println("Connected.");
  yield();

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

  })
  .onEnd([]() {
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
  });


  RFID_SPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
  mfrc522.PCD_Init();
  Serial.print("RFID Scanner: ");
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  Serial.println("Starting loop");

  display.setBrightness(BRIGHT_HIGH / 3);
  display.showNumber(amount, 2);
  ArduinoOTA.begin();

  state = PRICES;
}

void loop()
{
  ArduinoOTA.handle();
  if (state == PRICES) {
    static unsigned long lst = 0;
    if (millis() - lst > 15 * 1000 || lst == 0) {
      lst = millis();
      Serial.println("Fetch prices");
      if (setupPrices(SKU) != 200) {
        display.showString("no prices ");
        if (millis() > 180 * 1000)
          ESP.restart();
        return;
      };
      display.showNumber(amount, 2);
      state = RUN;
    };
  }

  static unsigned long t = millis();
  if (millis() - t > 2500) {
    time_t now = time(nullptr);
    char * tstr = ctime(&now);
    // Sat Oct 16 20:53:36 2021;
    if (strncmp(tstr + 11, "06:00", 5) == 0 && millis() > 30*1000) {
      Serial.println("Nightly reboot; in case of memory leaks and fetches new prices.");
      ESP.restart();
    }
    Serial.print(tstr);
    t = millis();
  }

  if (state < RUN)
    return;

  if (loopRFID()) {
    display.setBrightness(BRIGHT_HIGH);
    display.showString("[--]");
    int status = payByREST(tag, amount);
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
}
