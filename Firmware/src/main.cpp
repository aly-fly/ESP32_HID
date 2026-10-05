#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include "__CONFIG.h"

#include <Preferences.h>
Preferences preferences;
String text1, text2;

#include <Adafruit_Fingerprint.h>

// Use HardwareSerial 1 or 2 on ESP32-S3
HardwareSerial fingerSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);
bool FingerptintSensorOk = false;


#include "Version.h"

// Create the keyboard object
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
USBCDC USBSerial;

// Timing variables for the 2-minute wiggler
unsigned long lastWiggleTime = 0;

bool capsLockStatus = false;
bool numLockStatus;

// ===============================================================================================================================================================

void NVSReadSettings() {
  // Read data from NVRAM
  preferences.begin(NVS_NAMESPACE, true);  // read-only mode
  text1 = preferences.getString(NVS_KEY_TEXT1, ""); 
  text2 = preferences.getString(NVS_KEY_TEXT2, ""); 
  preferences.end();
  USBSerial.printf("Read from NVS: %d and %d characters.\r\n", text1.length(), text2.length());
}

void NVSWriteSettings() {
  USBSerial.printf("Saving '%s' and '%s' to NVS...\r\n", text1, text2);
  preferences.begin(NVS_NAMESPACE, false); // read-write mode
  preferences.putString(NVS_KEY_TEXT1, text1); 
  preferences.putString(NVS_KEY_TEXT2, text2); 
  USBSerial.println("Data Saved using Preferences");  
  preferences.end();        
}

// ===============================================================================================================================================================

// This callback function runs automatically whenever the PC sends data back to the keyboard
void onKeyboardEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_id == ARDUINO_USB_HID_KEYBOARD_LED_EVENT) {
    // Cast the incoming data to the LED status byte
    uint8_t led_status = *(uint8_t*)event_data;

    // The led_status byte uses bitwise flags:
    // Bit 0 (0x01): Num Lock
    // Bit 1 (0x02): Caps Lock
    // Bit 2 (0x04): Scroll Lock

    capsLockStatus = (led_status & LED_CAPSLOCK);
    numLockStatus  = (led_status & LED_NUMLOCK);

    // Print the status to the Serial Monitor
    USBSerial.printf("LED Report Received! CapsLock: %s | NumLock: %s\n", 
                  capsLockStatus ? "ON" : "OFF", 
                  numLockStatus ? "ON" : "OFF");
    USBSerial.flush();

    // Physically turn an LED on/off based on Caps Lock status
    digitalWrite(LED_PIN, capsLockStatus ? HIGH : LOW);
  }
}

// ===============================================================================================================================================================

/*
 * Color: 1=red, 2=blue, 3=purple, 4=green, 5=yellow, 6=cyan, 7=white
 * Mode: 1=breathing, 2=flashing, 3=always on, 4=always off
 */
void setRingLED(uint8_t color, uint8_t mode) {
  finger.LEDcontrol(mode, 100, color, 1);
}

void handleFingerAction(uint16_t fingerID) {
  switch (fingerID) {
    case 1: 
      USBSerial.println("Finger 1");
      setRingLED(RING_GREEN, FINGERPRINT_LED_BREATHING); // Green breathing LED

      // turn off caps lock
      if (capsLockStatus)
        Keyboard.write(KEY_CAPS_LOCK);

      // Type a string
      Keyboard.print(text2);
      delay(50);
      Keyboard.write(KEY_RETURN);

      USBSerial.println("Finished 1");
      delay(1000); // Debounce / delay      
      break;

    case 2: 
      USBSerial.println("Finger 2");
      setRingLED(RING_BLUE, FINGERPRINT_LED_BREATHING); // Blue breathing LED
      // turn off caps lock
      if (capsLockStatus)
        Keyboard.write(KEY_CAPS_LOCK);

      // Type a string
      Keyboard.print(text1);
      Keyboard.write(KEY_TAB);
      delay(100);

      Keyboard.print(text2);
      delay(50);
      Keyboard.write(KEY_RETURN);
      
      USBSerial.println("Finished 2");
      delay(1000); // Debounce / delay      
      break;

    case 3: 
      USBSerial.println("Finger 3");
      setRingLED(RING_CYAN, FINGERPRINT_LED_BREATHING); // Cyan breathing LED
      // turn off caps lock
      if (capsLockStatus)
        Keyboard.write(KEY_CAPS_LOCK);

      Keyboard.press(KEY_LEFT_GUI); // Windows  
      Keyboard.press('d');
      delay(100);
      Keyboard.releaseAll();
      delay(300);
      Keyboard.print("cns");
      delay(50);
      Keyboard.write(KEY_RETURN);
      delay(1500);

      Keyboard.write(KEY_TAB);
      delay(150);
      Keyboard.write(KEY_TAB);
      delay(150);
      Keyboard.write(KEY_TAB);
      delay(150);
      Keyboard.write(KEY_TAB);
      delay(150);
      Keyboard.write(KEY_RETURN);
      delay(150);
      Keyboard.write(KEY_TAB);
      delay(150);
      Keyboard.write(KEY_RETURN);
      delay(3000);

      Keyboard.print(text2);
      Keyboard.write(KEY_RETURN);
      
      delay(1000); // Debounce / delay      
      USBSerial.println("Finished 3");
      break;

    default:
      USBSerial.printf("Recognized finger ID #%d (Unassigned Action)\n", fingerID);
      break;
  }
}

// ===============================================================================================================================================================

int getFingerprintID() {
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
      FingerptintSensorOk = false;
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
    FingerptintSensorOk = false;
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
      FingerptintSensorOk = false;
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
    FingerptintSensorOk = false;
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
    FingerptintSensorOk = false;
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
    FingerptintSensorOk = false;
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

void setup() {
  pinMode(GPIO_NUM_0, INPUT_PULLUP);
  pinMode(START_TYPING_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(100);

  // config USB
  Keyboard.onEvent(onKeyboardEvent);

  // Initialize USB HID - this disconnects the COM port!
  Keyboard.begin();
  Mouse.begin();
  USBSerial.begin();
  USB.begin();

  USBSerial.setTxTimeoutMs(0);    // Set TX timeout to 0 ms so writes drop non-blocking if host buffer is unread
  USBSerial.enableReboot(false);  // Disable board reboot on DTR/RTS line toggle

  delay(2000); // Give the OS time to recognize the device

  // (This blocks forever if the terminal does not set DTR=true)
  // while (!USBSerial) { delay(10); } // wait until terminal program sets the DTR signal
  USBSerial.setDebugOutput(true);


  USBSerial.println("Project: github.com/aly-fly/ESP32_HID");
  USBSerial.print("Version: ");
  USBSerial.println(VERSION);
  USBSerial.print("Build: ");
  USBSerial.println(BUILD_TIMESTAMP);
  NVSReadSettings();
  USBSerial.flush();
  delay(500); // wait until data is sent to the terminal

  pinMode(FINGERPRINT_WAKEUP, INPUT_PULLUP);

  fingerSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
  if (finger.verifyPassword()) {
    USBSerial.println("Found R503 fingerprint sensor!");
    setRingLED(RING_BLUE, FINGERPRINT_LED_ON); // Solid Blue ready state

    // Display sensor memory capacity
    if (finger.getParameters() == FINGERPRINT_OK) {
      finger.getTemplateCount();
      FingerptintSensorOk = true;
      USBSerial.printf("Sensor Capacity: %d templates | Security Level: %d\n", finger.capacity, finger.security_level);
    }

  } else {
    USBSerial.println("Did not find fingerprint sensor :(");
    FingerptintSensorOk = false;
  }  
}


// ===============================================================================================================================================================

// Global state flags and input buffer
bool text1ConvertSaveFlag = false;
bool text2ConvertSaveFlag = false;
bool enrolFinger = false;
String inputBuffer = "";

unsigned long lastReceiveTime = 0;
const unsigned long SERIAL_TIMEOUT_MS = 10000; // 10 seconds

// Maps input ASCII characters to the Slovenian QWERTZ keyboard layout.
char mapToSlovenianLayout(char c) {
    switch (c) {
        // Core QWERTZ swap
        case 'y': return 'z';
        case 'z': return 'y';
        case 'Y': return 'Z';
        case 'Z': return 'Y';
       
        // --- 3. Unshifted Top Row (Slovenian input -> US key position) ---
        case '\'': return '-'; // SI '\'' (key 11) -> US '-'
        case '+':  return '='; // SI '+'  (key 12) -> US '='

        // --- 4. Shifted Top Row (Slovenian input -> US key position) ---
        case '"':  return '@'; // SI '"' (Shift+2) -> US '@'
        case '&':  return '^'; // SI '&' (Shift+6) -> US '^'
        case '/':  return '&'; // SI '/' (Shift+7) -> US '&'
        case '(':  return '*'; // SI '(' (Shift+8) -> US '*'
        case ')':  return '('; // SI ')' (Shift+9) -> US '('
        case '=':  return ')'; // SI '=' (Shift+0) -> US ')'
        case '?':  return '_'; // SI '?' (Shift+') -> US '_'
        case '*':  return '+'; // SI '*' (Shift++) -> US '+'
        
        default: return c;
    }
}

// Converts an entire string character-by-character using the Slovenian mapping logic.

String convertSlovenianString(const String &input) {
    String output = "";
    output.reserve(input.length());
    for (size_t i = 0; i < input.length(); i++) {
        output += mapToSlovenianLayout(input.charAt(i));
    }
    return output;
}

// Reads from USBSerial, collects characters, and executes actions on CR (\r).
void handleSerialInput() {
    while (USBSerial.available() > 0) {
        char incomingChar = (char)USBSerial.read();

        // Update timestamp whenever a new character arrives
        lastReceiveTime = millis();

        // 1. Check for Carriage Return (CR)
        if (incomingChar == '\r') {
            
            // Check if received string is "pwd"
            if (inputBuffer == "usr") {
                USBSerial.println("Waiting for data");
                text1ConvertSaveFlag = true;
            } 
            // Check if received string is "pwd"
            else if (inputBuffer == "pwd") {
                USBSerial.println("Waiting for data");
                text2ConvertSaveFlag = true;
            } 
            // Check if received string is "fin"
            else if (inputBuffer == "fin") {
                USBSerial.println("Enroll fingerprint. Enter number 1..200.");
                enrolFinger = true;
            } 
            // Process and convert string if the flag is enabled
            else if (text1ConvertSaveFlag) {
                String convertedData = convertSlovenianString(inputBuffer);
                
                USBSerial.print("Received: ");
                USBSerial.println(inputBuffer);
                USBSerial.print("Converted: ");
                USBSerial.println(convertedData);
                text1 = convertedData;
                NVSWriteSettings();
                text1ConvertSaveFlag = false;
            }
            // Process and convert string if the flag is enabled
            else if (text2ConvertSaveFlag) {
                String convertedData = convertSlovenianString(inputBuffer);
                
                USBSerial.print("Received: ");
                USBSerial.println(inputBuffer);
                USBSerial.print("Converted: ");
                USBSerial.println(convertedData);
                text2 = convertedData;
                NVSWriteSettings();
                text2ConvertSaveFlag = false;
            }
            else if (enrolFinger) {
              int fingerNumber = inputBuffer.toInt();
              USBSerial.printf("Number = %d.\n", fingerNumber);
              if ((fingerNumber >= 1) && (fingerNumber <= 200)) {
                enrollFingerprint(fingerNumber);
              }
            }

            // Reset the buffer for the next incoming payload
            inputBuffer.clear();
        } 
        // Ignore Line Feed (\n) characters commonly paired with CR (\r\n)
        else if (incomingChar != '\n') {
            inputBuffer += incomingChar;
        }
        // prevent memory overrun
        if (inputBuffer.length() > 100)
        {
          inputBuffer.clear();
          text1ConvertSaveFlag = false;
          text2ConvertSaveFlag = false;
        }   
    }

    // Inactivity Timeout Check
    // If the buffer contains partial data and 10 seconds have elapsed without new input, clear it
    if (inputBuffer.length() > 0 && (millis() - lastReceiveTime >= SERIAL_TIMEOUT_MS)) {
        inputBuffer.clear(); 
        text1ConvertSaveFlag = false;
        text2ConvertSaveFlag = false;
        USBSerial.println("[Timeout] Incomplete input cleared.");
    }    
}

// ===============================================================================================================================================================




void loop() {
  handleSerialInput();
  
  // SAFETY CHECK: Only type if the BOOT button is being pressed.
  // This prevents infinite loops from hijacking your keyboard if something goes wrong.
  if (digitalRead(START_TYPING_PIN) == LOW) {
    USBSerial.println("Button pressed! Sending keystrokes...");
    
    // turn off caps lock
    if (capsLockStatus)
      Keyboard.write(KEY_CAPS_LOCK);

    // Type a string
    Keyboard.print(text2);
    Keyboard.write(KEY_RETURN);
    
    delay(1000); // Debounce / delay
  }

  unsigned long currentMillis = millis();
  // --- MOUSE WIGGLER (Runs every 2 minutes) ---
  if (currentMillis - lastWiggleTime >= WIGGLE_INTERVAL) {
    USBSerial.println("Wiggling mouse to stay awake...");
    
    // Move mouse 2 pixels right, 2 pixels down
    Mouse.move(2, 2); 
    delay(50);
    // Move mouse to original spot
    Mouse.move(-2, -2); 
    
    lastWiggleTime = currentMillis; // Reset the timer
  }  

  if (FingerptintSensorOk) {
    // if finger is presented, this pin will be 0. Otherwise don't talk to the sensor.
    if (digitalRead(FINGERPRINT_WAKEUP) == 0) {
      setRingLED(RING_BLUE, FINGERPRINT_LED_ON);
      int matchedID = getFingerprintID();
      if (matchedID > 0) {
        handleFingerAction(matchedID);
      }
    }
  }
}

// ===============================================================================================================================================================
