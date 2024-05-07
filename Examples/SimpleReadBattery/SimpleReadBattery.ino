#include <BQ25672.h>

//BQ25672 charger = BQ25672(&Serial); // Print error flags to Serial when charger.readFlags() is called
BQ25672 charger = BQ25672(); // Do not print error flags when charger.readFlags() is called

void setup() {
  Serial.begin(115200);

  bool error = charger.begin(); // Begin I2C bus with default I2C pins
  //  bool error = charger.begin(/*SDA = */21, /*SCL = */22); // Begin I2C bus with specific pins (possible on e.g. ESP32, RPi Pico, etc.)


  if (error) { // .begin returns 1 or higher if error occured
    Serial.println("BQ25672 Not found");
    while (1); // Do nothing if sensor cannot be found
  }
  delay(1000);
  Serial.println("BQ25672 Started");

  charger.setWatchdogTimerTime(0);                // Writing 0 disables watchdog timer, default it is set to 5, meaning 40s
  charger.setAdcEnabled(true);                    // Enable ADC
  charger.setBatteryCurrentSensingEnabled(true);  // Enable battery current sensing during discharge

}


void loop() {
  //    charger.setAdcEnabled(true);      // Call this in the loop if watchdog timer is enabled
  Serial.println();
  Serial.println("Battery voltage: " + String(charger.getBatteryVoltage()) + "mV");
  Serial.println("Battery current: " + String(charger.getBatteryCurrent()) + "mA");
  Serial.println("Die temperature: " + String(charger.getDieTemperature()) + "C");
  Serial.println();
  delay(3000); // Wait 3000ms
}
