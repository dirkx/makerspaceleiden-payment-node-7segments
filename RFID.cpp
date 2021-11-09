
#include <SPI.h>
#include <MFRC522.h>

#include "global.h"
#include "log.h"

SPIClass * spi;
MFRC522_SPI * spiDevice;
MFRC522 * mfrc522;

char tag[128];

void setupRFID()
{
  unsigned char mac[6];
  const unsigned char  RFID_SCLK  =   18;
  const unsigned char  RFID_MISO  =   19;
  const unsigned char  RFID_MOSI  =   23;
        unsigned char  RFID_CS    =   5; // default VSPI wiring
        unsigned char  RFID_RESET =   21; // these two pins swapped on the older beer-node.
       unsigned char  RFID_IRQ   =   22;

  // 3C:71:BF:43:0F:E4 - oldest beer node with funny wiring.
  WiFi.macAddress(mac);
  if (mac[3] == 0x43 && mac[4] == 0x0F  && mac[5] == 0xE4) {
    RFID_RESET =   22;
    RFID_IRQ   =   21;
    RFID_CS    =   15;
  };

  spi = new SPIClass(VSPI);
  spi->begin(RFID_SCLK, RFID_MISO, RFID_MOSI, RFID_CS);

  spiDevice = new MFRC522_SPI(RFID_CS, RFID_RESET, spi);
  mfrc522 = new MFRC522(spiDevice);
  mfrc522->PCD_Init();

  Serial.print("RFID Scanner init.");
  mfrc522->PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
}

int loopRFID() {
  if ( ! mfrc522->PICC_IsNewCardPresent()) {
    return 0;
  }
  if ( ! mfrc522->PICC_ReadCardSerial()) {
    Log.println("Bad read (was card removed too quickly ? )");
    return 0;
  }
  if (mfrc522->uid.size < 3) {
    Log.println("Bad card (size tool small)");
    return 0;
  };

  // We're somewhat strict on the parsing/what we accept; as we use it unadultared in the URL.
  //
  if (mfrc522->uid.size > sizeof(mfrc522->uid.uidByte)) {
    Log.println("Too large a card id size. Ignoring.)");
    return 0;
  }

  memset(tag, 0, sizeof(tag));
  for (int i = 0; i < mfrc522->uid.size; i++) {
    char buff[5]; // 3 digits, dash and \0.
    snprintf(buff, sizeof(buff), "%s%d", i ? "-" : "", mfrc522->uid.uidByte[i]);
    strncat(tag, buff, sizeof(tag) - 1);
  };
  Log.println("Good scan");

  mfrc522->PICC_HaltA();
  return 1;
}
