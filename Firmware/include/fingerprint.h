#include <Arduino.h>
#include <Adafruit_Fingerprint.h>


void setRingLED(uint8_t color, uint8_t mode);
uint8_t enrollFingerprint(uint8_t id);

void fingerprintInit();
void fingerprintLoop();
