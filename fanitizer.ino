#include <SPI.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "ESP8266WiFi.h"
#include "ota.hpp"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;
WiFiUDP udp;

float temperature;        // Temperature read from sensor
float humidity;           // Humidity read from sensor

int fan_in_duty;          // Intake fan duty cycle value
int fan_ex_duty;          // Exhaust fan duty cycle value

volatile int inPulses;    // Intake fan counter of tacho pulses
volatile int exPulses;    // Exhaust fan counter of tacho pulses
int fanInRPM;             // Intake fan calculated RPM
int fanExRPM;             // Exhaust fan calculated RPM
unsigned long lastInRPMmillis = 0;  // Last time we counted In fan RPM
unsigned long lastExRPMmillis = 0;  // Last time we counted Ex fan RPM

ICACHE_RAM_ATTR void countInPulse() {
  inPulses++;
}

ICACHE_RAM_ATTR void countExPulse() {
  exPulses++;
}

int calculateRPM(unsigned long &lastRPMmillis, volatile int &pulses) {
  int RPM;
  noInterrupts();
  int elapsedMS = (millis() - lastRPMmillis) / 1000;
  int revolutions = pulses / 2;
  int revPerMS = revolutions / elapsedMS;
  RPM = revPerMS * 60;
  lastRPMmillis = millis();
  pulses = 0;
  interrupts();
  return RPM;
}

void setup() {
  Serial.begin(115200);
  delay(100);

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
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  InitOTA();

  showSplashScreen();
  connectToWiFi(); // TODO: possible check if we are connected before sending data so we don't stuck with bad wifi
}

// Reboot device
void(* resetFunc) (void) = 0;

void loop() {
  ArduinoOTA.handle();
  
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();

  if (!isnan(temperature)) {
    fan_in_duty = map(temperature * 100, MIN_TEMP * 100, MAX_TEMP * 100, MIN_DUTY, MAX_DUTY);
    fan_ex_duty = map(temperature * 100, MIN_TEMP * 100, MAX_TEMP * 100, MIN_DUTY, MAX_DUTY);
    fan_ex_duty -= (DIF_DUTY * fan_ex_duty / 100);
    analogWrite(FAN_IN_PIN, fan_in_duty);
    analogWrite(FAN_EX_PIN, fan_ex_duty);
  }

  fanInRPM = calculateRPM(lastInRPMmillis, inPulses);
  fanExRPM = calculateRPM(lastExRPMmillis, exPulses);

  displayInfo();
  sendToInfluxdb();
  delay(5000);
}

void displayInfo() {
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
  sprintf(buffer, "IN %i %i", fan_in_load, fanInRPM);
  display.println(buffer);

  display.setCursor(0, 49);
  int fan_ex_load = (fan_ex_duty  * 100 / MAX_DUTY); // Exhaust fan speed percentage
  sprintf(buffer, "EX %i %i", fan_ex_load, fanExRPM);
  display.println(buffer);

  display.display();
}

void sendToInfluxdb() {
  String line = String("environment,host=" + nodeName + " temperature=" + String(temperature) + ",humidity=" + String(humidity) + ",inRPM=" + String(fanInRPM) + ",exRPM=" + String(fanExRPM));

  Serial.print("Sending line: ");
  Serial.println(line);

  udp.beginPacket(influxHost, influxPort);
  udp.print(line);
  udp.endPacket();
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA); // Set ESP8266 mode to act as a Wi-Fi client and attempt to connect with credentials
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed!");
    delay(500);
  }

  Serial.println("Connected to network");
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
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

// Tests the fan from min duty cycle to max duty cycle and backwards
/*
  void testFanPwm() {
    for(int dutyCycle = MIN_DUTY; dutyCycle < MAX_DUTY; dutyCycle+=10){
      Serial.print("dutyCycle: ");
      Serial.println(dutyCycle);
      analogWrite(FAN_IN_PIN, dutyCycle);
      delay(1000);
    }

    for(int dutyCycle = MAX_DUTY; dutyCycle > MIN_DUTY; dutyCycle-=10){
      Serial.print("dutyCycle: ");
      Serial.println(dutyCycle);
      analogWrite(FAN_IN_PIN, dutyCycle);
      delay(1000);
    }
  }
*/
