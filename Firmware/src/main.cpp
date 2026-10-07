#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include "usbKeyboardMouseSerial.h"
#include "fingerprint.h"
#include "__CONFIG.h"

#include <Preferences.h>
Preferences preferences;
String text1, text2;

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
    USBSerial.printf("CapsLock: %s | NumLock: %s\n", 
                  capsLockStatus ? "ON" : "OFF", 
                  numLockStatus ? "ON" : "OFF");
    USBSerial.flush();

    // Physically turn an LED on/off based on Caps Lock status
    digitalWrite(LED_PIN, capsLockStatus ? HIGH : LOW);
    setRingLED(RING_WHITE, capsLockStatus ? FINGERPRINT_LED_ON : FINGERPRINT_LED_OFF);    
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

// ===============================================================================================================================================================

// Reads from USBSerial, collects characters, and executes actions on CR (\r).
void handleSerialInput() {
    while (USBSerial.available() > 0) {
        char incomingChar = (char)USBSerial.read();

        // Update timestamp whenever a new character arrives
        lastReceiveTime = millis();

        // 1. Check for Carriage Return (CR)
        if (incomingChar == '\r') {
            // Check if received string is "usr"
            if (inputBuffer == "rst") {
                USBSerial.println("Restarting...");
                USBSerial.flush();
                delay(200);
                ESP.restart();
              } 
            // Check if received string is "usr"
            else if (inputBuffer == "usr") {
                USBSerial.println("Waiting for data 1");
                text1ConvertSaveFlag = true;
            } 
            // Check if received string is "pwd"
            else if (inputBuffer == "pwd") {
                USBSerial.println("Waiting for data 2");
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
            inputBuffer += incomingChar; // collect chars into an incoming string
        }
        // Check if received string is "v"
        if ((inputBuffer == "v") || (inputBuffer == "v\r")) {
            USBSerial.println("ESP32 HID fingerprint sensor");
            USBSerial.println("Project: github.com/aly-fly/ESP32_HID");
            USBSerial.print("Version: ");
            USBSerial.println(VERSION);
            USBSerial.print("Build: ");
            USBSerial.println(BUILD_TIMESTAMP);
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

void Action1 (void) {
  USBSerial.println("Running action 1");
  setRingLED(RING_GREEN, FINGERPRINT_LED_BREATHING); // Green breathing LED

  // turn off caps lock
  if (capsLockStatus)
    Keyboard.write(KEY_CAPS_LOCK);

  // Type a string
  Keyboard.print(text2);
  delay(50);
  Keyboard.write(KEY_RETURN);

  delay(1500);
  setRingLED(RING_GREEN, FINGERPRINT_LED_BREATHING);
  delay(1500);
  setRingLED(RING_GREEN, FINGERPRINT_LED_BREATHING);
  delay(1500);
  USBSerial.println("Finished 1");
}

void Action2 (void) {
  USBSerial.println("Running action 2");
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

  delay(1500);
  setRingLED(RING_BLUE, FINGERPRINT_LED_BREATHING);
  delay(1500);
  setRingLED(RING_BLUE, FINGERPRINT_LED_BREATHING);
  delay(1500);
  USBSerial.println("Finished 2");
}

void Action3 (void) {
  USBSerial.println("Running action 3");
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

  setRingLED(RING_CYAN, FINGERPRINT_LED_BREATHING);

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

  setRingLED(RING_CYAN, FINGERPRINT_LED_BREATHING);
  
  Keyboard.print(text2);
  Keyboard.write(KEY_RETURN);

  delay(1500);
  setRingLED(RING_CYAN, FINGERPRINT_LED_BREATHING);
  delay(1500);
  USBSerial.println("Finished 3");
}

// ===============================================================================================================================================================
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

  fingerprintInit();
}


// ===============================================================================================================================================================
// ===============================================================================================================================================================


void loop() {
  handleSerialInput();
  
  /*
  // Type if the BOOT button is being pressed.
  if (digitalRead(START_TYPING_PIN) == LOW) {
    USBSerial.println("Button pressed! Sending keystrokes...");
    
    Action1 ();
  }
  */

  unsigned long currentMillis = millis();
  // --- MOUSE WIGGLER (Runs every 2 minutes) ---
  if (currentMillis - lastWiggleTime >= WIGGLE_INTERVAL) {
    USBSerial.println("Wiggling mouse...");
    
    // Move mouse 2 pixels right, 2 pixels down
    Mouse.move(2, 2); 
    delay(50);
    // Move mouse to original spot
    Mouse.move(-2, -2); 
    
    lastWiggleTime = currentMillis; // Reset the timer
  }  

  fingerprintLoop();
}

// ===============================================================================================================================================================
