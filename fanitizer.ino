#include <Wire.h>
#include <WiFiUdp.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "ESP8266WiFi.h"
#include "OTA.hpp"
#include "TimerObject.h"

#define __DEBUG__
#define DISPLAY_OK 1
#define DISPLAY_NOT_PRESENT 2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;
WiFiUDP udp;

int displayStatus;

float temperature;        // Temperature read from sensor
float humidity;           // Humidity read from sensor

int fan_in_duty;          // Intake fan duty cycle value
int fan_ex_duty;          // Exhaust fan duty cycle value
volatile int inPulses;    // Intake fan counter of tacho pulses
volatile int exPulses;    // Exhaust fan counter of tacho pulses
int fanInRPM;             // Intake fan calculated RPM
int fanExRPM;             // Exhaust fan calculated RPM

TimerObject *environmentTimer = new TimerObject(5000);
TimerObject *rpmTimer = new TimerObject(1000);          // RPM MUST be calculated every second!
TimerObject *displayTimer = new TimerObject(5500);
TimerObject *influxTimer = new TimerObject(10000);

// IN tacho pulse counter
ICACHE_RAM_ATTR void countInPulse() {
  inPulses++;
}

// EX tacho pulse counter
ICACHE_RAM_ATTR void countExPulse() {
  exPulses++;
}

// Updates RPM values
void calculateRPM() {
  fanInRPM = getRPM(inPulses);
  fanExRPM = getRPM(exPulses);
}

// Converts pulses to RPM. MUST be called once every second!
int getRPM(volatile int &pulses) {
  int RPM;
  noInterrupts();
  RPM = (pulses / 2) * 60;
  pulses = 0;
  interrupts();
  return RPM;
}

void setup() {
  Serial.begin(115200);

  // Initialize temperature sensor
  bme.begin(0x76);

  // Initialize PWM settings
  analogWriteFreq(25000); // 25KHz
  pinMode(FAN_IN_PIN, OUTPUT);
  pinMode(FAN_EX_PIN, OUTPUT);

  pinMode(TACHO_IN_PIN, INPUT_PULLUP);
  pinMode(TACHO_EX_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACHO_IN_PIN), countInPulse, RISING);
  attachInterrupt(digitalPinToInterrupt(TACHO_EX_PIN), countExPulse, RISING);

  // Initialize display  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    Serial.println("SSD1306 allocation failed");
    displayStatus = DISPLAY_NOT_PRESENT;
    // Keep going, we got other stuff to do...
  }
  displayStatus = DISPLAY_OK;

  InitOTA();

  environmentTimer->setOnTimer(&adjustPWM);
  environmentTimer->Start();

  rpmTimer->setOnTimer(&calculateRPM);
  rpmTimer->Start();

  displayTimer->setOnTimer(&displayInfo);
  displayTimer->Start();

  influxTimer->setOnTimer(&sendToInfluxdb);
  influxTimer->Start();

  showSplashScreen();
  connectToWiFi(); // TODO: possible check if we are connected before sending data so we don't stuck with bad wifi
}

void loop() {
  ArduinoOTA.handle();
  environmentTimer->Update();
  rpmTimer->Update();
  displayTimer->Update();
  influxTimer->Update();
}

void adjustPWM() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();

  if (!isnan(temperature)) {
    fan_in_duty = map(temperature * 100, MIN_TEMP * 100, MAX_TEMP * 100, MIN_DUTY, MAX_DUTY);
    fan_in_duty = constrain(fan_in_duty, MIN_DUTY, MAX_DUTY); // Make sure that we are not going under or over max duty cycle value
    fan_ex_duty = map(temperature * 100, MIN_TEMP * 100, MAX_TEMP * 100, MIN_DUTY, MAX_DUTY);
    fan_ex_duty = constrain(fan_ex_duty, MIN_DUTY, MAX_DUTY); // Make sure that we are not going under or over max duty cycle value
    fan_ex_duty -= (DIF_DUTY * fan_ex_duty / 100);
    analogWrite(FAN_IN_PIN, fan_in_duty);
    analogWrite(FAN_EX_PIN, fan_ex_duty);
  }
}

void displayInfo() {
  if (displayStatus != DISPLAY_OK) {
    return;
  }

  char buffer[10];  
  display.clearDisplay();  
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Fntzr");
  display.setCursor(90, 0);
  display.print(WiFi.RSSI());
  display.println("dBm");

  display.setTextSize(2);
  display.setCursor(0, 16);
  sprintf(buffer, "Tmp %.2fC", temperature);
  display.println(buffer);

  display.setCursor(0, 33);
  int fan_in_load = (fan_in_duty  * 100 / MAX_DUTY); // Intake fan speed percentage
  sprintf(buffer, "> %i %i", fan_in_load, fanInRPM);
  display.println(buffer);

  display.setCursor(0, 49);
  int fan_ex_load = (fan_ex_duty  * 100 / MAX_DUTY); // Exhaust fan speed percentage
  sprintf(buffer, "< %i %i", fan_ex_load, fanExRPM);
  display.println(buffer);

  display.display();
}

void sendToInfluxdb() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  String line = String("environment,host=" + String(nodeName) + " temperature=" + String(temperature) + ",humidity=" + String(humidity) + ",inRPM=" + String(fanInRPM) + ",exRPM=" + String(fanExRPM));
  Serial.print("Sending line: ");
  Serial.println(line);

  udp.beginPacket(influxHost, influxPort);
  udp.print(line);
  udp.endPacket();
}

void connectToWiFi() {
  WiFi.hostname(nodeName);
  WiFi.mode(WIFI_STA); // Set ESP8266 mode to act as a Wi-Fi client and attempt to connect with credentials  
  WiFi.begin(ssid, password);

  // Wait up to 10s to connect to WiFi
  for (int i=0; i<=10 && WiFi.status() != WL_CONNECTED; i++) {
    Serial.println("WiFi connection failed!");
    delay(1000);
  }

  // Well, maybe it will connect later, but leave it for now...
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  Serial.print("Connected to ");
  Serial.println(ssid);
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// Draws an erecting penis :)
void showSplashScreen() {
  char base[10] = "";
  base[0] = '8';
  for (int stiff = 1; stiff < 10; ++stiff) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 27);
    display.print(base);
    display.print('>');
    display.display();
    base[stiff] = '=';
    delay(100);
  }
}
