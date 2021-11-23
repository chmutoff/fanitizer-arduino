# fanitizer - smart and tiny fan control

This project is designed to cool a closed space using 2 fans, intake and exhaust.
- The intake fan has more speed than the exhaust
- The temperature, humidity and fan RMP speeds are reported to InfluxDB
- OTA with Arduino IDE
- Small LCD display to see the current temperature, PWM and RPM

Part List:
- Board: Wemos D1 mini (ESP8266)
- Display: SSD1306
- Sensor: BME280 (Temperature and Humidity)
- 2x 4 pin PWM fan
- 12V power supply (for fans)
- 5v usb power supply (for wemos)

Wiring:
- 4pin PWM fan
    - Pin 1: GND
    - Pin 2: 12V
    - Pin 3: D5(intake), D7(exhause) Tach
    - Pin 4: D6(intake), D8(exhause) PWM
- I2C
    - SDA: D2
    - SCL: D1

Rename example-config.h to config.h, change it and move to the fanitizer project
