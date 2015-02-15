// This program sends 280000 bytes over serial port zero.
// Maximum baud rate for a 328 Arduino is 57600.
// Maximum baud rate for a Mega Arduino is 115200
const uint32_t BAUD_RATE = 57600;

#include <SerialPort.h>

// Define NewSerial as serial port zero.
USE_NEW_SERIAL;

void setup() {
  NewSerial.begin(BAUD_RATE);
  pinMode(13, OUTPUT);
  // light pin 13 LED on Arduino Board
  digitalWrite(13, HIGH);
  
  // write start time
  NewSerial.println(millis());
  
  for (uint16_t i = 0; i < 10000; i++) {
    NewSerial.write("ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n");
  }
  // write end time
  NewSerial.println(millis());
  
  // turn off pin 13 LED
  digitalWrite(13, LOW);
}
void loop() {}
