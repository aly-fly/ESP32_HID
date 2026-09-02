#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

#include "Version.h"

// Create the keyboard object
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
USBCDC USBSerial;


// A safety pin to prevent the ESP32 from locking you out of your computer!
const int START_TYPING_PIN = 0; // Boot button on most S3 dev boards
const int LED_PIN = GPIO_NUM_48;

// Timing variables for the 2-minute wiggler
unsigned long lastWiggleTime = 0;
const unsigned long WIGGLE_INTERVAL = 119000; // ms (almost 2 minutes)

bool capsLockStatus = false;
bool numLockStatus;

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

    // Physically turn an LED on/off based on Caps Lock status
    digitalWrite(LED_PIN, capsLockStatus ? HIGH : LOW);
  }
}

// ===============================================================================================================================================================

void setup() {
  pinMode(GPIO_NUM_0, INPUT_PULLUP);
  pinMode(START_TYPING_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  delay(100);

  // Wait for the virtual com port to establish the connection with the host (PC) and terminal opens the com port
//  Serial.begin(115200);
//  Serial.setDebugOutput(true);

  // config USB
  Keyboard.onEvent(onKeyboardEvent);

  // Initialize USB HID - this disconnects the COM port!
  Keyboard.begin();
  Mouse.begin();
  USBSerial.begin();
  USB.begin();

  delay(2000); // Give the OS time to recognize the device

  /*
  while (!USBSerial) { delay(10); }

  USBSerial.setDebugOutput(true);
*/
  USBSerial.println("Project: github.com/aly-fly/ESP32_HID");
  USBSerial.print("Version: ");
  USBSerial.println(VERSION);
  USBSerial.print("Build: ");
  USBSerial.println(BUILD_TIMESTAMP);
  delay(500); // wait until data is sent to the terminal
}


// ===============================================================================================================================================================


void loop() {
  // SAFETY CHECK: Only type if the BOOT button is being pressed.
  // This prevents infinite loops from hijacking your keyboard if something goes wrong.
  if (digitalRead(START_TYPING_PIN) == LOW) {
    USBSerial.println("Button pressed! Sending keystrokes...");
    
    // turn off caps lock
    if (capsLockStatus)
      Keyboard.write(KEY_CAPS_LOCK);

    // Type a string
    Keyboard.print("Zxcvbnm!234567");
    Keyboard.write(KEY_RETURN);
    
    Mouse.move(5, 5); 

    delay(1000); // Debounce / delay
  }

  unsigned long currentMillis = millis();
  // --- MOUSE WIGGLER (Runs every 2 minutes) ---
  if (currentMillis - lastWiggleTime >= WIGGLE_INTERVAL) {
    USBSerial.println("Wiggling mouse to stay awake...");
    
    // Move mouse 2 pixels right, 2 pixels down
    Mouse.move(2, 2); 
    delay(50);
    // Move mouse to original spot)
    Mouse.move(-2, -2); 
    
    lastWiggleTime = currentMillis; // Reset the timer
  }  
}

// ===============================================================================================================================================================
