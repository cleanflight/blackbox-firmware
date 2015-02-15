/* Arduino SerialPort Library
 * Copyright (C) 2011 by William Greiman
 *
 * This file is part of the Arduino SerialPort Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino SerialPort Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
/**
\mainpage Arduino %SerialPort Library
<CENTER>Copyright &copy; 2011 by William Greiman
</CENTER>

\section Intro Introduction
This library provide more options for buffering, character size, parity and
error checking than the standard Arduino HardwareSerial class for AVR Arduino
boards.

The API was designed to be backward compatible with Arduino Serial.
A number of functions have been added to the API.

To install the this library, copy the SerialPort folder to the
your libraries folder.

Please explore the above tabs for detailed documentation.

@section howto Quick How To

This was posted in the Arduino forum by user sixeyes.
It is a nice introduction to to the SerialPort library.

The core Arduino based serial code looks like this:

@code
void setup()
{
    Serial.begin(9600);
    Serial.println("Using Arduino supplied HardwareSerial");
}

void loop()
{
}
@endcode

So if you've copied the SerialPort folder from the zip file into your
libraries folder (So you have Arduino/libraries/SerialPort/SerialPort.h
and Arduino/libraries/SerialPort/SerialPort.cpp)
you can write the following code instead

@code
#include <SerialPort.h>

SerialPort<0, 32, 256> port;

void setup()
{
    port.begin(9600);
    port.println("Using SerialPort class");
}

void loop()
{
}
@endcode

Just remember to do the following and you should be alright:

 - Include <SerialPort.h>
 - Declare new Serial port including its parameters
 - Initialise (call begin()) and use the new serial port (NOT Serial)
 .
The configuration of the serial port looks odd to anyone not familiar with
C++ templates, but it's easy to explain. The first parameter inside the
angle brackets is the serial port number. Unless you've got a board with
more than one serial port (e.g. Arduino Mega) this will always be 0. The
second parameter is the size of the receive buffer in bytes and the third
is the size of the transmit buffer in bytes.

You can ignore the stuff about editing SerialPort.h. You'll only need this
if you're short of flash memory at which point you can come back and ask
more questions.

If you do by chance refer to both the new SerialPort and HardwareSerial in
the same sketch you'll get some error messages about duplicate interrupt
vectors.

Hope that helps.

Iain

@section logger SerialPort Sd Logger

The folder, SerialPortLogger, contains a demo data logging sketch that is
capable of logging serial data to an SD card at up to 115200 baud.

The two programs, SerialPortLogger.ino and SerialDataSource.ino, demonstrate
high speed logging of serial data.

For more information see the readme.txt file in the SerialPortLogger folder.

@section examples Examples

A number of examples are included in the SerialPort/examples folder.

ArduinoSize - Print the amount of free RAM using Arduino HardwareSerial.

ArduinoTest - Arduino HardwareSerial version of test sketch.

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

@section config Configuration Options

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
*/