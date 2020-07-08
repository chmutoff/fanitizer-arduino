#include <SPI.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include "ESP8266WiFi.h"

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin # (or -1 if sharing Arduino reset pin)

#define MIN_DUTY 150      // Minimal speed when fan starts to spin
#define MAX_DUTY 1024     // Limit maximum fan speed
#define DIF_DUTY 1.10     // Difference between input and exhaust fan PWM signals (Allows to create positive pressure)

#define FAN_IN_PIN D6     // Intake fan PWM pin
#define FAN_EX_PIN D7     // Exhaust fan PWM pin
#define TACHO_IN_PIN D3   // Intake fan tacho pin
#define TACHO_EX_PIN D4   // Exhaust fan tacho pin

#define MIN_TEMP 25       // Temperature that triggers fan power on
#define MAX_TEMP 35       // Temperature that triggers max fan power

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;
WiFiUDP udp;

float temperature;        // Temperature read from sensor
float humidity;           // Humidity read from sensor

int fan_in_duty;          // Intake fan duty cycle value
int fan_ex_duty;          // Exhaust fan duty cycle value

volatile int inPulses;
volatile int exPulses;
unsigned long lastInRPMmillis = 0;
unsigned long lastExRPMmillis = 0;

const byte influxHost[] = {192, 168, 69, 96};   // Hostname of the server running influxDB
const int influxPort = 8089;                    // UDP port number of the server running influxDB
String nodeName = "NodeName";                   // Host tag reported to influxDB

const char* ssid = "WiFi_SSID";
const char* password = "************";

ICACHE_RAM_ATTR void countInPulse() {
  inPulses++;
}

ICACHE_RAM_ATTR void countExPulse() {
  exPulses++;
}

int calculateInRPM() {
  int RPM;  
  noInterrupts();
  int elapsedMS = (millis() - lastInRPMmillis)/1000;
  int revolutions = inPulses / 2;
  int revPerMS = revolutions / elapsedMS;
  RPM = revPerMS * 60;
  lastInRPMmillis = millis();
  inPulses=0;
  interrupts();
  return RPM;
}

int calculateExRPM() {
  int RPM;  
  noInterrupts();
  int elapsedMS = (millis() - lastExRPMmillis)/1000;
  int revolutions = exPulses / 2;
  int revPerMS = revolutions / elapsedMS;
  RPM = revPerMS * 60;
  lastExRPMmillis = millis();
  exPulses=0;
  interrupts();
  return RPM;
}

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("setup()");

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
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }

    showSplashScreen();
    connectToWiFi(); // TODO: possible check if we are connected and add this function in each loop iteration or before sending data so we don't stuck with bad wifi
}

// Reboot device
void(* resetFunc) (void) = 0;
 
void loop() {
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    
    char buffer[50];
    sprintf(buffer, "Temperature: %.2fC, Humidity: %.2f", temperature, humidity);
    Serial.println(buffer);

    if (!isnan(temperature)) {
      fan_in_duty = map(temperature*100, MIN_TEMP*100, MAX_TEMP*100, MIN_DUTY, MAX_DUTY);
      fan_ex_duty = map(temperature*100, MIN_TEMP*100, MAX_TEMP*100, MIN_DUTY, MAX_DUTY);
      analogWrite(FAN_IN_PIN, fan_in_duty);
      analogWrite(FAN_EX_PIN, fan_ex_duty); // TODO: make this fan slower!!!
    } else {
   
    }

    /*
    Serial.print("IN RPM=");
    Serial.println(calculateRPM(lastInRPMmillis, inPulses));

    Serial.print("EX RPM=");
    Serial.println(calculateRPM(lastExRPMmillis, exPulses));
    */
    
    displayInfo();
    sendToInfluxdb();
    delay(5000);
}

void displayInfo() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    char buffer[10];
    int fan_in_load = (fan_in_duty  * 100 / MAX_DUTY);                     // Intake fan speed percentage
    int fan_ex_load = (fan_ex_duty  * 100 / MAX_DUTY) / DIF_DUTY;          // Exhaust fan speed percentage
    
    display.setCursor(0, 0);
    display.println("Fanitizer");
    
    display.setCursor(0, 16);
    sprintf(buffer, "Tmp %.2fC", temperature);
    display.println(buffer);
  
    display.setCursor(0, 33);
    sprintf(buffer, "IN %i", fan_in_load);
    display.println(buffer);
  
    display.setCursor(0, 49);
    sprintf(buffer, "EX %i", fan_ex_load);
    display.println(buffer);
      
    display.display();
}

void sendToInfluxdb(){
  int inRPM = calculateInRPM();
  int exRPM = calculateExRPM();
  //lastExRPMmillis, exPulses

  Serial.print("IN RPM=");
  Serial.println(inRPM);
  
  Serial.print("EX RPM=");
  Serial.println(exRPM);
  //String line = String("environment,host=" + nodeName + " temperature=" + String(temperature) + ",humidity=" + String(humidity) + ",inRPM=" + String(inRPM) + ",exRPM=" + String(exRPM));
  String line = String("environment,host=" + nodeName + " temperature=" + String(temperature) + ",humidity=" + String(humidity));
  
  Serial.print("Sending line: ");
  Serial.println(line);

  udp.beginPacket(influxHost, influxPort);
  udp.print(line);
  udp.endPacket();
}

void connectToWiFi(){
    WiFi.mode(WIFI_STA); // Set ESP8266 mode to act as a Wi-Fi client and attempt to connect with credentials
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection failed!");
        delay(500);
    }
    
    Serial.println("Connected to network");
    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
    
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}

// Draws an erecting penis :)
void showSplashScreen(){
  char base[10]="";
  base[0]='8';
  for(int stiff=1; stiff<10; ++stiff) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,27);
    display.print(base);
    display.print('>');
    display.display();
    base[stiff]='=';
    delay(100);
  }
}

// Tests the fan from min duty cycle to max duty cycle and backwards
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
