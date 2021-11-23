#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin # (or -1 if sharing Arduino reset pin)

#define MIN_DUTY 150      // Minimal speed when fan starts to spin
#define MAX_DUTY 1024     // Limit maximum fan speed
#define DIF_DUTY 30       // Difference (from 0 to 100%) between input and exhaust fan PWM signals (To create positive pressure)

#define FAN_IN_PIN D6     // Intake fan PWM pin
#define FAN_EX_PIN D8     // Exhaust fan PWM pin
#define TACHO_IN_PIN D5   // Intake fan tacho pin
#define TACHO_EX_PIN D7   // Exhaust fan tacho pin

#define MIN_TEMP 25       // Temperature that triggers fan power on (min pwm)
#define MAX_TEMP 35       // Temperature for max fan pwm

const byte influxHost[] = {192, 168, 69, 69};   // Hostname of the server running influxDB
const int influxPort = 8089;                    // UDP port number of the server running influxDB
const String nodeName = "Fanitizer";            // Hostname used for WiFi, OTA, influxDB tags and node identification

const char* ssid = "WiFi_SSID";					        // WiFi SSID
const char* password = "************";			    // WiFi password

const char* otaPassword = "veryStrongPass";     // Password for OTA updates
