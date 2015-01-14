This is an early beta and has had minimal testing.  Please provide
feedback about features, any suggestions, and report bugs.

Please read changes.txt

This library provides access to the Arduino serial ports.

To install the this library, copy the SerialPort folder to the
your libraries directory.

See the HowTo.txt file for a quick introduction to usage.

Try the HelloWorld example first.

The API was designed to be backward compatible with Arduino Serial.
A number of functions have been added to the API.

See the html for additional documentation.

--------------------------------------------------------------------------------
A number of examples are included in the SerialPort/examples folder.

ArduinoSize - Print the amount of free RAM using Arduino HardwareSerial.

ArduinoTest - Arduino HardwareSerial version of test sketch

BufferedSize - Print the amount of free RAM using SerialPort with
               buffered RX and TX.

BufferedTest - Test SerialPort with buffered RX and TX.

HelloWorld - Simple first example.

MegaTest - Test all ports on a Mega using SerialPort.

MegaTestArduino - Test all ports on a Mega using Arduino HardwareSerial.

ReadWriteTest - Test that ring buffer overrun can be detected.

UnbufferedSize - Print the amount of free RAM using SerialPort with no buffers.

UnbufferedTest - Test SerialPort with no buffering.

WriteFlash - Test write() for a flash string.

--------------------------------------------------------------------------------
configuration options:

You can can save flash if your buffers are always smaller than 254 bytes
by setting ALLOW_LARGE_BUFFERS zero in SerialPort.h. This will also
increase performance slightly.

You can save substantial flash by disabling the faster versions of
write(const char*) and write(const uint8_t*, size_t) by setting
USE_WRITE_OVERRIDES zero in SerialPort.h.

If you only use unbuffered TX, edit SerialPort.h and set BUFFERED_TX zero.
This will save some flash and RAM by preventing the TX ISR from being
loaded.  TxBufSize must always be zero in SerialPort constructors if
BUFFERED_TX is zero.

If you only do output or if input buffering is not required, edit
SerialPort.h and set BUFFERED_RX zero.  This will save some flash
and RAM by preventing the RX ISR from being loaded. RxBufSize must
always be zero in SerialPort constructors if BUFFERED_RX is zero.

You can disable RX error functions by setting ENABLE_RX_ERROR_CHECKING
zero in SerialPort.h  This will save a tiny bit of flash, RAM, and
speedup receive a little.