
#include <SPI.h>
#include <MFRC522.h>

#include "global.h"
#include "log.h"


#if 0 // beer / older one of the two
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

char tag[128];

void setupRFID()
{
  RFID_SPI.begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);
  mfrc522.PCD_Init();
  Serial.print("RFID Scanner init.");
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
}

int loopRFID() {
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
