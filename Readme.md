# Blackbox firmware for the OpenLog

This modified version of [OpenLog 3 Light][] discards the "CONFIG.TXT" system that is normally used to configure the
OpenLog. Instead it hard-codes the correct settings for the Blackbox (115200 baud). This makes it more robust to power
glitches during OpenLog startup that might mess with the settings.

You will find a zip file containing the required libraries, the source-code and the compiled hex file on the "releases"
page above.

You'll need to copy the required libraries to your Arduino IDE's library path in order to build this from source. Copy
the `libs/SdFatBeta20120108/SdFat` and `libs/SerialPortBeta20120106/SerialPort` folders to your libraries folder directly
(don't copy the outer folders "SdFatBeta20120108" or "SerialPortBeta20120106".)

To flash the firmware to the OpenLog, you can use an FTDI programmer like the [FTDI Basic Breakout][] along with some
way of switching the Tx and Rx pins over (since the OpenLog has them switched) like the [FTDI crossover][] .

In the Arduino IDE, choose the "Arduino Uno" as your board and select your serial port (if you use an FTDI cable) or 
your programmer if you're using one. Then click Sketch -> Verify/Compile and File -> Upload to send it to the OpenLog.

[OpenLog 3 Light]: https://github.com/sparkfun/OpenLog/tree/master/firmware/OpenLog_v3_Light
[FTDI Basic Breakout]: https://www.sparkfun.com/products/9716
[FTDI crossover]: https://www.sparkfun.com/products/10660
