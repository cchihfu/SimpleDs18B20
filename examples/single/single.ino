// Include the libraries we need
#include <SimpleDs18b20.h>
// Data wire is plugged into port 7 on the Arduino
uint8_t dq_pin=7;

SimpleDs18b20 ds01(dq_pin);

void setup() {
	Serial.begin(9600);
}
void loop() {
	Serial.print("Ds18B20's Temperature --> ");
	// print the temperature 
	Serial.println(ds01.GetTemperature());
	delay(1000);
}
