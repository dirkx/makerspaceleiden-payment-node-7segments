#ifndef _H_MPN_GLOBAL
#define _H_MPN_GLOBAL

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

#define VERSION "F1-05"

#ifndef TERMINAL_NAME
#define TERMINAL_NAME "4x7"
#endif

#ifndef WIFI_NETWORK
#define WIFI_NETWORK "MyWifiNetwork"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "MyWifiPassword"
#endif

#ifndef PAY_URL
#define PAY_URL "https://test.makerspaceleiden.nl:4443/crm/pettycash/api"
#endif

#define HTTP_TIMEOUT (5000)

// Jump back to the default after this many milliseconds, provided
// that there is a default item set in the CRM.
//
#define DEFAULT_TIMEOUT (60*1000)

// Reboot every day (or comment this out).
#define AUTO_REBOOT_TIME "04:00"

// Wait up to 10 seconds for wifi during boot.
#define WIFI_MAX_WAIT (20*1000)

#ifndef NTP_POOL
#define NTP_POOL "nl.pool.ntp.org"
#endif

#ifndef AMOUNT_NO_OK_NEEDED
#define AMOUNT_NO_OK_NEEDED (5.0)
#endif

typedef enum { BOOT = 0, WAITING_FOR_NTP, FETCH_CA, REGISTER, WAIT_FOR_REGISTER_SWIPE, REGISTER_PRICELIST, ENTER_AMOUNT, PAID, FIRMWARE_UPDATE, FIRMWARE_FAIL, REGISTER_FAIL, OEPSIE, WIFI_FAIL_REBOOT } state_t;

// keep all ugly globals in one place for now.
//
extern int NA;
extern char **amounts;
extern char **prices;
extern char **descs;
extern int amount;
extern int default_item ;
extern double amount_no_ok_needed;
extern state_t md;
extern const char * version;
extern char terminalName[64];
extern char tag[128];
extern String label;
#endif
