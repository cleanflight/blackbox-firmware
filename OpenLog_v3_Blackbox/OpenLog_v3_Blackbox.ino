/*
 12-3-09
 Nathan Seidle
 SparkFun Electronics 2012
 
 OpenLog hardware and firmware are released under the Creative Commons Share Alike v3.0 license.
 http://creativecommons.org/licenses/by-sa/3.0/
 Feel free to use, distribute, and sell varients of OpenLog. All we ask is that you include attribution of 'Based on OpenLog by SparkFun'.
 
 OpenLog is based on the work of Bill Greiman and sdfatlib: http://code.google.com/p/sdfatlib/
 
 OpenLog is a simple serial logger based on the ATmega328 running at 16MHz. The ATmega328
 is able to talk to high capacity (larger than 2GB) SD cards. The whole purpose of this
 logger was to create a logger that just powered up and worked. OpenLog ships with an Arduino/Optiboot 
 115200bps serial bootloader running at 16MHz so you can load new firmware with a simple serial
 connection.
 
 OpenLog automatically works with 512MB, 1GB, 2GB, 4GB, 8GB, and 16GB microSD cards. We recommend FAT16 for 2GB and smaller cards. We
 recommend FAT32 for 4GB and larger cards.
 
 During power up, you will see '12<'. '1' indicates the serial connection is established. '2' indicates
 the SD card has been successfully initialized. '<' indicates OpenLog is ready to receive serial characters.
 
 Recording constant 115200bps datastreams are supported. Throw it everything you've got! To acheive this maximum record rate, please use the
 SD card formatter from : http://www.sdcard.org/consumers/formatter/. The fewer files on the card, the faster OpenLog is able to begin logging.
 200 files is ok. 2GB worth of music and pictures is not.
 
 Please note: The preloaded Optiboot serial bootloader is 0.5k, and begins at 0x7E00 (32,256). If the code is
 larger than 32,256 bytes, you will get verification errors during serial bootloading.
 
 STAT1 LED is sitting on PD5 (Arduino D5) - toggles when character is received
 STAT2 LED is sitting on PB5 (Arduino D13) - toggles when SPI writes happen
 
 LED Flashing errors @ 2Hz:
 No SD card - 3 blinks
 
 OpenLog regularly shuts down to conserve power. If after 0.5 seconds no characters are received, OpenLog will record any unsaved characters
 and go to sleep. OpenLog will automatically wake up and continue logging the instant a new character is received.
 
 1.55mA idle
 15mA actively writing
 
 Input voltage on VCC can be 3.3 to 12V. Input voltage on RX-I pin must not exceed 6V. Output voltage on
 TX-O pin will not be greater than 3.3V. This may cause problems with some systems - for example if your
 attached microcontroller requires 4V minimum for serial communication (this is rare).
 
 v3 Blackbox edition
 
 This version has been modified for use with the Blackbox feature for Baseflight / Cleanflight. This version 
 discards the CONFIG.TXT system and instead hard-codes the correct settings (115200 baud).
 
 */

#include <SdFat.h> //We do not use the built-in SD.h file because it calls Serial.print
#include <SerialPort.h> //This is a new/beta library written by Bill Greiman. You rock Bill!
#include <EEPROM.h>

SerialPort<0, 800, 0> NewSerial;
//This is a very important buffer declaration. This sets the <port #, rx size, tx size>. We set
//the TX buffer to zero because we will be spending most of our time needing to buffer the incoming (RX) characters.
//1100 fails on card init and causes FAT table corruption
//1000 works on light,
//900 works on light and is able to create config file

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers

//Debug turns on (1) or off (0) a bunch of verbose debug statements. Normally use (0)
//#define DEBUG  1
#define DEBUG  0

//The bigger the receive buffer, the less likely we are to drop characters at high speed. However, the ATmega has a limited amount of
//RAM. This debug mode allows us to view available RAM at various stages of the program
//#define RAM_TESTING  1 //On
#define RAM_TESTING  0 //Off

#define SEQ_FILENAME "SEQLOG00.TXT" //This is the name for the file when you're in sequential modeu

//Internal EEPROM locations for the user settings
#define LOCATION_FILE_NUMBER_LSB	0x03
#define LOCATION_FILE_NUMBER_MSB	0x04

#define BAUD_MIN  300
#define BAUD_MAX  1000000

#define MODE_NEWLOG	0
#define MODE_SEQLOG     1

//STAT1 is a general LED and indicates serial traffic
#define STAT1  5 //On PORTD
#define STAT1_PORT  PORTD
#define STAT2  5 //On PORTB
#define STAT2_PORT  PORTB
const byte statled1 = 5;  //This is the normal status LED
const byte statled2 = 13; //This is the SPI LED, indicating SD traffic

//Blinking LED error codes
#define ERROR_SD_INIT	  3
#define ERROR_NEW_BAUD	  5
#define ERROR_CARD_INIT   6
#define ERROR_VOLUME_INIT 7
#define ERROR_ROOT_INIT   8
#define ERROR_FILE_OPEN   9

#define OFF   0x00
#define ON    0x01

Sd2Card card;
SdVolume volume;
SdFile currentDirectory;

long setting_uart_speed; //This is the baud rate that the system runs at, default is 9600. Can be 300 to 1,000,000
byte setting_system_mode; //This is the mode the system runs in, default is MODE_NEWLOG

//Passes back the available amount of free RAM
int freeRam() {
#if RAM_TESTING
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
#else
    return 0;
#endif
}
void printRam() {
#if RAM_TESTING
    NewSerial.print(F(" RAM:"));
    NewSerial.println(freeRam());
#endif
}

//Handle errors by printing the error type and blinking LEDs in certain way
//The function will never exit - it loops forever inside blink_error
void systemError(byte error_type) {
    NewSerial.print(F("Error "));
    switch (error_type) {
    case ERROR_CARD_INIT:
        NewSerial.print(F("card.init"));
        blink_error(ERROR_SD_INIT);
        break;
    case ERROR_VOLUME_INIT:
        NewSerial.print(F("volume.init"));
        blink_error(ERROR_SD_INIT);
        break;
    case ERROR_ROOT_INIT:
        NewSerial.print(F("root.init"));
        blink_error(ERROR_SD_INIT);
        break;
    case ERROR_FILE_OPEN:
        NewSerial.print(F("file.open"));
        blink_error(ERROR_SD_INIT);
        break;
    }
}

void setup(void) {
    pinMode(statled1, OUTPUT);

    //Power down various bits of hardware to lower power usage  
    set_sleep_mode (SLEEP_MODE_IDLE);
    sleep_enable();

    //Shut off TWI, Timer2, Timer1, ADC
    ADCSRA &= ~(1 << ADEN); //Disable ADC
    ACSR = (1 << ACD); //Disable the analog comparator
    DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
    DIDR1 = (1 << AIN1D) | (1 << AIN0D); //Disable digital input buffer on AIN1/0

    power_twi_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_adc_disable();

    init_system_settings();

    //Setup UART
    NewSerial.begin(setting_uart_speed);
    if (setting_uart_speed < 500)      // check for slow baud rates
            {
        //There is an error in the Serial library for lower than 500bps. 
        //This fixes it. See issue 163: https://github.com/sparkfun/OpenLog/issues/163
        // redo USART baud rate configuration
        UBRR0 = (F_CPU / (16UL * setting_uart_speed)) - 1;
        UCSR0A &= ~_BV(U2X0);
    }
    NewSerial.print(F("1"));

    //Setup SD & FAT
    if (!card.init(SPI_FULL_SPEED))
        systemError(ERROR_CARD_INIT);
    if (!volume.init(&card))
        systemError(ERROR_VOLUME_INIT);
    currentDirectory.close(); //We close the cD before opening root. This comes from QuickStart example. Saves 4 bytes.
    if (!currentDirectory.openRoot(&volume))
        systemError(ERROR_ROOT_INIT);

    NewSerial.print(F("2"));

    printRam(); //Print the available RAM
}

void loop(void) {
    //If we are in new log mode, find a new file name to write to
    if (setting_system_mode == MODE_NEWLOG) {
        //new_file_name = newlog();
        append_file (newlog()); //Append the file name that newlog() returns
}
        //If we are in sequential log mode, determine if seqlog.txt has been created or not, and then open it for logging
        if(setting_system_mode == MODE_SEQLOG)
        seqlog();

        while(1);//We should never get this far
    }

//Log to a new file everytime the system boots
//Checks the spots in EEPROM for the next available LOG# file name
//Updates EEPROM and then appends to the new log file.
//Limited to 65535 files but this should not always be the case.
char* newlog(void) {
    byte msb, lsb;
    uint16_t new_file_number;

    SdFile newFile; //This will contain the file for SD writing

    //Combine two 8-bit EEPROM spots into one 16-bit number
    lsb = EEPROM.read(LOCATION_FILE_NUMBER_LSB);
    msb = EEPROM.read(LOCATION_FILE_NUMBER_MSB);

    new_file_number = msb;
    new_file_number = new_file_number << 8;
    new_file_number |= lsb;

    //If both EEPROM spots are 255 (0xFF), that means they are un-initialized (first time OpenLog has been turned on)
    //Let's init them both to 0
    if ((lsb == 255) && (msb == 255)) {
        new_file_number = 0; //By default, unit will start at file number zero
        EEPROM.write(LOCATION_FILE_NUMBER_LSB, 0x00);
        EEPROM.write(LOCATION_FILE_NUMBER_MSB, 0x00);
    }

    //If we made it this far, everything looks good - let's start testing to see if our file number is the next available

    //Search for next available log spot
    //char new_file_name[] = "LOG00000.TXT";
    static char new_file_name[13];
    while (1) {
        sprintf_P(new_file_name, PSTR("LOG%05d.TXT"), new_file_number); //Splice the new file number into this file name

        //Try to open file, if fail (file doesn't exist), then break
        if (newFile.open(&currentDirectory, new_file_name,
                O_CREAT | O_EXCL | O_WRITE))
            break;

        //Try to open file and see if it is empty. If so, use it.
        if (newFile.open(&currentDirectory, new_file_name, O_READ)) {
            if (newFile.fileSize() == 0) {
                newFile.close();     // Close this existing file we just opened.
                return (new_file_name);  // Use existing empty file.
            }
            newFile.close(); // Close this existing file we just opened.
        }

        //Try the next number (wrapping to zero if we overflow)
        new_file_number++;
    }
    newFile.close(); //Close this new file we just opened

    new_file_number++; //Increment so the next power up uses the next file #

    //Record new_file number to EEPROM
    lsb = (byte)(new_file_number & 0x00FF);
    msb = (byte)((new_file_number & 0xFF00) >> 8);

    EEPROM.write(LOCATION_FILE_NUMBER_LSB, lsb); // LSB

    if (EEPROM.read(LOCATION_FILE_NUMBER_MSB) != msb)
        EEPROM.write(LOCATION_FILE_NUMBER_MSB, msb); // MSB

#if DEBUG
    NewSerial.print(F("\nCreated new file: "));
    NewSerial.println(new_file_name);
#endif

    //  append_file(new_file_name);
    return (new_file_name);
}

//Log to the same file every time the system boots, sequentially
//Checks to see if the file SEQLOG.txt is available
//If not, create it
//If yes, append to it
//Return 0 on error
//Return anything else on sucess
void seqlog(void) {
    SdFile seqFile;

    char sequentialFileName[strlen(SEQ_FILENAME)]; //Limited to 8.3
    strcpy_P(sequentialFileName, PSTR(SEQ_FILENAME)); //This is the name of the config file. 'config.sys' is probably a bad idea.

    //Try to create sequential file
    if (!seqFile.open(&currentDirectory, sequentialFileName,
            O_CREAT | O_WRITE)) {
        NewSerial.print(F("Error creating SEQLOG\n"));
        return;
    }

    seqFile.close(); //Close this new file we just opened

    append_file(sequentialFileName);
}

//This is the most important function of the device. These loops have been tweaked as much as possible.
//Modifying this loop may negatively affect how well the device can record at high baud rates.
//Appends a stream of serial data to a given file
//Assumes the currentDirectory variable has been set before entering the routine
void append_file(char* file_name) {
    SdFile workingFile;

    // O_CREAT - create the file if it does not exist
    // O_APPEND - seek to the end of the file prior to each write
    // O_WRITE - open for write
    if (!workingFile.open(&currentDirectory, file_name, O_CREAT | O_APPEND | O_WRITE))
        systemError(ERROR_FILE_OPEN);

    if (workingFile.fileSize() == 0) {
        //This is a trick to make sure first cluster is allocated - found in Bill's example/beta code
        workingFile.rewind();
        workingFile.sync();
    }

    NewSerial.print(F("<")); //give a different prompt to indicate no echoing
    digitalWrite(statled1, HIGH); //Turn on indicator LED

    const byte LOCAL_BUFF_SIZE = 128; //This is the 2nd buffer. It pulls from the larger NewSerial buffer as quickly as possible.
    byte localBuffer[LOCAL_BUFF_SIZE];

    const uint16_t MAX_IDLE_TIME_MSEC = 500; //The number of milliseconds before unit goes to sleep
    const uint16_t MAX_TIME_BEFORE_SYNC_MSEC = 5000;
    uint32_t lastSyncTime = millis(); //Keeps track of the last time the file was synced

    printRam(); //Print the available RAM

    //Start recording incoming characters
    while (1) { //Infinite loop

        byte n = NewSerial.read(localBuffer, sizeof(localBuffer)); //Read characters from global buffer into the local buffer
        if (n > 0) {
            workingFile.write(localBuffer, n); //Record the buffer to the card

            STAT1_PORT ^= (1 << STAT1); //Toggle the STAT1 LED each time we record the buffer

            //This will force a sync approximately every 5 seconds
            if ((millis() - lastSyncTime) > MAX_TIME_BEFORE_SYNC_MSEC) { 
                //This is here to make sure a log is recorded in the instance
                //where the user is throwing non-stop data at the unit from power on to forever
                workingFile.sync(); //Sync the card
                lastSyncTime = millis();
            }
        }
        //No characters recevied?
        else if ((millis() - lastSyncTime) > MAX_IDLE_TIME_MSEC) { //If we haven't received any characters for a while, go to sleep
            workingFile.sync(); //Sync the card before we go to sleep

            STAT1_PORT &= ~(1 << STAT1); //Turn off stat LED to save power

            power_timer0_disable(); //Shut down peripherals we don't need
            power_spi_disable();
            sleep_mode(); //Stop everything and go to sleep. Wake up if serial character received

            power_spi_enable(); //After wake up, power up peripherals
            power_timer0_enable();

            lastSyncTime = millis(); //Reset the last sync time to now
        }
    }
}

//The following are system functions needed for basic operation
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Blinks the status LEDs to indicate a type of error
void blink_error(byte ERROR_TYPE) {
    while (1) {
        for (int x = 0; x < ERROR_TYPE; x++) {
            digitalWrite(statled1, HIGH);
            delay(200);
            digitalWrite(statled1, LOW);
            delay(200);
        }

        delay(2000);
    }
}

//Set up system settings
void init_system_settings(void) {
    setting_uart_speed = 115200;
    setting_system_mode = MODE_NEWLOG;
}
