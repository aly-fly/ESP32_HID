 
#ifndef __CONFIG_H_
#define __CONFIG_H_

#define NVS_NAMESPACE  "settings"
#define NVS_KEY_TEXT1  "name"
#define NVS_KEY_TEXT2  "pass"

#define FINGERPRINT_RX_PIN 13
#define FINGERPRINT_TX_PIN 12
#define FINGERPRINT_WAKEUP 11  // idle = 1; finger presented = 0

const int START_TYPING_PIN = 0; // Boot button on most S3 dev boards
const int LED_PIN = GPIO_NUM_48;

const unsigned long WIGGLE_INTERVAL = 119000; // ms (almost 2 minutes)

// Fingerprint sensor ring colors:
// 1=red, 2=blue, 3=purple, 4=green, 5=yellow, 6=cyan, 7=white

#define RING_RED     1
#define RING_BLUE    2
#define RING_PURPLE  3
#define RING_GREEN   4
#define RING_YELLOW  5
#define RING_CYAN    6
#define RING_WHITE   7


#endif /* __CONFIG_H_ */
