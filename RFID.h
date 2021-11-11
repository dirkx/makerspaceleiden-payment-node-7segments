#ifndef _H_RFID
#define _H_RFID

extern char tag[128];
void setupRFID();
int loopRFID();
extern unsigned int rfid_scans, rfid_miss;
#endif
