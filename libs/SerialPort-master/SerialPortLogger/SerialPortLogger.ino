// Serial data logger example.
// Maximum baud rate for a 328 Arduino is 57600.
// Maximum baud rate for a Mega Arduino is 115200.
const uint32_t BAUD_RATE = 57600;

// Maximum time between sync() calls in milliseconds.  If Serial is always
// active, you must provide a way to stop the program and close the file.
const uint32_t MAX_SYNC_TIME_MSEC = 1000;

// Pin number for error blink LED.
// Set ERROR_LED_PIN to -1 for no error LED.
const int8_t ERROR_LED_PIN = 3;

#include <SdFat.h>
#include <SerialPort.h>

#if defined(__AVR_ATmega1280__)\
|| defined(__AVR_ATmega2560__)
// Mega, use 4096 byte RX buffer
SerialPort<0, 4096, 0> NewSerial;
#else // Mega
// not a Mega, use 1024 RX byte buffer
SerialPort<0, 1024, 0> NewSerial;
#endif

SdFat sd;
SdFile file;

//------------------------------------------------------------------------------
// Error codes repeat as errno short blinks with a delay between codes.
const uint8_t ERROR_INIT   = 1;  // SD init error
const uint8_t ERROR_OPEN   = 2;  // file open error
const uint8_t ERROR_SERIAL = 3;  // serial error
const uint8_t ERROR_WRITE  = 4;  // SD write or sync error
void errorBlink(uint8_t errno) {
  uint8_t i;
  while (ERROR_LED_PIN < 0);
  while (1) {
    for (i = 0; i < errno; i++) {
      digitalWrite(ERROR_LED_PIN, HIGH);
      delay(200);
      digitalWrite(ERROR_LED_PIN, LOW);
      delay(200);
    }
    delay(1600);
  }
}
//------------------------------------------------------------------------------
void setup() {
  pinMode(ERROR_LED_PIN, OUTPUT);  
  NewSerial.begin(BAUD_RATE);

  if (!sd.begin()) {
    errorBlink(ERROR_INIT);
  }
  if (!file.open("SERIAL.BIN", O_WRITE | O_CREAT | O_AT_END)) {
    errorBlink(ERROR_OPEN);
  }
  if (file.fileSize() == 0) {
    // Make sure first cluster is allocated.
    file.write((uint8_t)0);
    file.rewind();
    file.sync();
  }
}
//------------------------------------------------------------------------------
// Time of last sync call.
uint32_t syncTime = 0;

uint8_t buf[32];
void loop() {
  if (NewSerial.getRxError()) {
    errorBlink(ERROR_SERIAL);    
  }
  uint8_t n = NewSerial.read(buf, sizeof(buf));
  if (n > 0) {
    if (file.write(buf, n) != n) {
      errorBlink(ERROR_WRITE);
    }
    // Don't sync if active.
    return;
  }
  if ((millis() - syncTime) < MAX_SYNC_TIME_MSEC) return;
    
  if (!file.sync()) {
    errorBlink(ERROR_WRITE);
  }
  syncTime = millis();  
}
