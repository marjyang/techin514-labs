#include <Arduino.h>

const int pin1 = D0; 
const int pin2 = D1;

void setup() {
  Serial.begin(115200); 
}

void loop() {
  // read analog values
  int adcValue1 = analogRead(pin1);
  int adcValue2 = analogRead(pin2);

  // ADC values -> voltages
  float voltage1 = (adcValue1 / 4095.0) * 3.3;
  float voltage2 = (adcValue2 / 4095.0) * 3.3;

  // print values to serial monitor
  Serial.print("VOUT1 Voltage: ");
  Serial.println(voltage1);
  Serial.print("VOUT2 Voltage: ");
  Serial.println(voltage2);

  delay(1000); // delay between readings
}