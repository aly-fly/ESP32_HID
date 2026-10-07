#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include "usbKeyboardMouseSerial.h"
#include "__CONFIG.h"

// Use HardwareSerial 1 or 2 on ESP32-S3
HardwareSerial fingerSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

bool FingerprintSensorOk = false;

// ===============================================================================================================================================================

/*
 * Color: 1=red, 2=blue, 3=purple, 4=green, 5=yellow, 6=cyan, 7=white
 * Mode: 1=breathing, 2=flashing, 3=always on, 4=always off
 */
void setRingLED(uint8_t color, uint8_t mode) {
  if (!FingerprintSensorOk) return;
  finger.LEDcontrol(mode, 100, color, 1);
}

void handleFingerAction(uint16_t fingerID) {
  switch (fingerID) {
    case 1: 
      USBSerial.println("Finger 1");
      Action1 ();    
      break;

    case 2: 
      USBSerial.println("Finger 2");
      Action2 ();
      break;

    case 3: 
      USBSerial.println("Finger 3");
      Action3 ();
      break;

    default:
      USBSerial.printf("Recognized finger ID #%d (Unassigned Action)\n", fingerID);
      break;
  }
}

// ===============================================================================================================================================================

int getFingerprintID() {
  if (!FingerprintSensorOk) return 0xBB;

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    USBSerial.printf("Imaging error. Status = %2X.\n", p);
    return -1;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    USBSerial.printf("Image convert error. Status = %2X.\n", p);
    return -1;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    USBSerial.printf("Found ID #%d with confidence %d\n", finger.fingerID, finger.confidence);
    return finger.fingerID;
  } else {
    USBSerial.printf("Finger not recognized. Status = %2X.\n", p);
    setRingLED(RING_RED, FINGERPRINT_LED_FLASHING); // Red
    return -1;
  }
}

uint8_t enrollFingerprint(uint8_t id) {
  if (!FingerprintSensorOk) return 0xBB;

  uint8_t p = 0xAA;
  USBSerial.print("Waiting for valid finger to enroll as #");
  USBSerial.println(id);
  setRingLED (RING_YELLOW, FINGERPRINT_LED_ON); // Yellow
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      USBSerial.println("Image taken");
      setRingLED (RING_GREEN, FINGERPRINT_LED_ON); // Green
      break;
    case FINGERPRINT_NOFINGER:
      USBSerial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      USBSerial.println("Communication error");
      FingerprintSensorOk = false;
      return p;
    case FINGERPRINT_IMAGEFAIL:
      USBSerial.println("Imaging error");
      setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
      return p;
    default:
      USBSerial.println("Unknown error");
      setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
      return p;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
  case FINGERPRINT_OK:
    USBSerial.println("Image converted");
    setRingLED (RING_GREEN, FINGERPRINT_LED_ON);
    break;
  case FINGERPRINT_IMAGEMESS:
    USBSerial.println("Image too messy");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    USBSerial.println("Communication error");
    FingerprintSensorOk = false;
    return p;
  case FINGERPRINT_FEATUREFAIL:
    USBSerial.println("Could not find fingerprint features");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    USBSerial.println("Invalid image");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  default:
    USBSerial.println("Unknown error");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  }

  USBSerial.println("Remove finger");
  delay(2000);
  p = 0xAA;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  p = 0xAA;
  USBSerial.println("Place same finger again");
  setRingLED (RING_YELLOW, FINGERPRINT_LED_ON); // Yellow
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      USBSerial.println("Image taken");
      setRingLED (RING_GREEN, FINGERPRINT_LED_ON);
      break;
    case FINGERPRINT_NOFINGER:
      USBSerial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      USBSerial.println("Communication error");
      FingerprintSensorOk = false;
      return p;
    case FINGERPRINT_IMAGEFAIL:
      USBSerial.println("Imaging error");
      setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
      return p;
    default:
      USBSerial.println("Unknown error");
      setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
      return p;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
  case FINGERPRINT_OK:
    USBSerial.println("Image converted");
    setRingLED (RING_GREEN, FINGERPRINT_LED_ON);
    break;
  case FINGERPRINT_IMAGEMESS:
    USBSerial.println("Image too messy");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    USBSerial.println("Communication error");
    FingerprintSensorOk = false;
    return p;
  case FINGERPRINT_FEATUREFAIL:
    USBSerial.println("Could not find fingerprint features");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    USBSerial.println("Invalid image");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  default:
    USBSerial.println("Unknown error");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  }

  // OK converted!
  USBSerial.print("Creating model for #");
  USBSerial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    USBSerial.println("Prints matched!");
    setRingLED (RING_WHITE, FINGERPRINT_LED_ON); // Cyan
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    USBSerial.println("Communication error");
    FingerprintSensorOk = false;
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    USBSerial.println("Fingerprints did not match");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  } else {
    USBSerial.println("Unknown error");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  }


  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    USBSerial.println("Stored!");
    setRingLED (RING_WHITE, FINGERPRINT_LED_ON); // White
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    USBSerial.println("Communication error");
    FingerprintSensorOk = false;
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    USBSerial.println("Could not store in that location");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    USBSerial.println("Error writing to flash");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  } else {
    USBSerial.println("Unknown error");
    setRingLED (RING_RED, FINGERPRINT_LED_ON); // Red
    return p;
  }

  return true;
}

// ===============================================================================================================================================================

void fingerprintInit() {
  pinMode(FINGERPRINT_WAKEUP, INPUT_PULLUP);

  fingerSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
  if (finger.verifyPassword()) {
    USBSerial.println("Found R503 fingerprint sensor!");

    // Display sensor memory capacity
    if (finger.getParameters() == FINGERPRINT_OK) {
      finger.getTemplateCount();
      FingerprintSensorOk = true;
      USBSerial.printf("Sensor Capacity: %d templates | Security Level: %d\n", finger.capacity, finger.security_level);
    }
    setRingLED(RING_BLUE, FINGERPRINT_LED_ON); // Solid Blue ready state

  } else {
    USBSerial.println("Did not find fingerprint sensor :(");
    FingerprintSensorOk = false;
  }  
}

// ===============================================================================================================================================================





void fingerprintLoop() {
  if (!FingerprintSensorOk) return;
  
  // if finger is presented, this pin will be 0. Otherwise don't talk to the sensor.
  if (digitalRead(FINGERPRINT_WAKEUP) == 0) {
    setRingLED(RING_WHITE, FINGERPRINT_LED_ON);
    delay(10);
    int matchedID = getFingerprintID();
    if (matchedID > 0) {
      handleFingerAction(matchedID);
    }
  }
}

// ===============================================================================================================================================================
