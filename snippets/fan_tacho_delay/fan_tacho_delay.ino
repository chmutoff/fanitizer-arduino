#define TACHO_PIN D5   // Tacho pin
#define FAN_PIN D6     // PWM pin

volatile int pulses;

ICACHE_RAM_ATTR void countPulse() {
  pulses++;
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
  pulses = 0;
  delay(1000);
  Serial.print("RPM=");
  Serial.println((pulses*60)/2);
}
