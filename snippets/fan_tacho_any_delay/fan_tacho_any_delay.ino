// https://www.baldengineer.com/pwm-3-pin-pc-fan-arduino.html

#define TACHO_PIN D5   // Tacho pin
#define FAN_PIN D6     // PWM pin

volatile int pulses;
unsigned long previousRPMMillis;
unsigned long lastRPMmillis = 0;

ICACHE_RAM_ATTR void countPulse() {
  pulses++;
}

unsigned long calculateRPM() {
  unsigned long RPM;
  noInterrupts();
  float elapsedMS = (millis() - lastRPMmillis)/1000.0;
  unsigned long revolutions = pulses/2;
  float revPerMS = revolutions / elapsedMS;
  RPM = revPerMS * 60.0;
  lastRPMmillis = millis();
  pulses=0;
  interrupts();
  return RPM;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize PWM settings
  analogWriteFreq(25000); // 25KHz
  pinMode(FAN_PIN, OUTPUT);
  analogWrite(FAN_PIN, 1024);

  pinMode(TACHO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACHO_PIN), countPulse, RISING);
}

void loop() {
  Serial.print("RPM=");
  Serial.println(calculateRPM());
  delay(3000);
}
