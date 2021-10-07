// dirkx@webweaving.org, apache license, for the makerspaceleiden.nl
//
// Tools settings:
//  Board OLIMEX ESP32-PoE (ISO of non-ISO)
//  Port: [the COM port your board has connected to]
//
// Boards used:
//    OLIMEX ESP32-PoE (ISO)
//    MFRC522 (https://www.tinytronics.nl/shop/nl/communicatie-en-signalen/draadloos/rfid/rfid-kit-mfrc522-s50-mifare-met-kaart-en-key-tag)
//    7 segment display ( https://www.hobbyelectronica.nl/product/4-digit-klok-display-module)
//
// Additional Librariries (via Sketch -> library manager):
//    MFRC522-spi-i2c-uart-async
//    TM1637TinyDisplay
//    Arduino_JSON
//
#include "/Users/dirkx/.passwd.h"

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

#define HTTP_TIMEOUT (5000)


#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "ca_root.h"
#include <Arduino_JSON.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TM1637TinyDisplay.h>

#define DISPLAY_CLK 2
#define DISPLAY_DIO 4

TM1637TinyDisplay display(DISPLAY_CLK, DISPLAY_DIO);

#define RFID_CS       13 // SDA on board, SS in library
#define RFID_SCLK     14
#define RFID_MOSI     15
#define RFID_MISO     16
#define RFID_RESET    32 // not soldered at this time !

SPIClass SDSPI(HSPI);
MFRC522_SPI spiDevice = MFRC522_SPI(RFID_CS, RFID_RESET, &SDSPI);
MFRC522 mfrc522 = MFRC522(&spiDevice);

// Very ugly global vars - used to communicate between the REST call and the rest.
//
char tag[sizeof(mfrc522.uid.uidByte) * 4 + 1 ] = { 0 };
char * description = "Maker Pie";
double amount = 3.14;

static int setupRFID()
{
  SDSPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
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
  https.setTimeout(HTTP_TIMEOUT);

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
  Serial.println(httpCode);

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

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Rebooting, wifi issue" );
    display.showString("FAIL");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Connected.");
  yield();

  // try to get some reliable time; to stop my cert
  // checking code complaining.
  configTime(0, 0, "nl.pool.ntp.org");
  yield();

  if (setupPrices(SKU) != 200) {
    for (int i = 0; i < 10; i++)
      display.showString("no prices ");
    ESP.restart();
  }

  SDSPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
  mfrc522.PCD_Init();
  Serial.print("RFID Scanner: ");
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  Serial.println("Starting loop");

  display.setBrightness(BRIGHT_HIGH / 3);
  display.showNumber(amount, 2);
}

void loop()
{
  static unsigned long t = millis();
  if (millis() - t > 2500) {
    time_t now = time(nullptr);
    Serial.print(ctime(&now));
    t = millis();
  }

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
