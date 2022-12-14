
/*
Lagrangian Water Sampler
This code is written for California Sate University, Northridge.
This code was tested on the Mini Ultra 8Mhz microcontroller with ATmegaA328P processor.
It controls two independent 12v pumps on a schedule, using the DS3231 real time clock.
This code was shipped with electronic schematics and a troubleshooting / maintenance manual.

License:

This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 United 
States License. To view a copy of this license, visit: 
http://creativecommons.org/licenses/by-sa/3.0/us/ 
or send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, 
California, 94041, USA.

Refactored code based on the original sketch by Vincent Moriarty, dated June 29 2016

Author:  Richard Scafidi
Date: 27 November 2022
richard@scafidi.dev
Compiled with Arduino IDE v2.0.2-nightly-20221112


*/
#include <RTClib.h>                 // Adafruit RTClib library      v2.1.1
#include <Adafruit_SleepyDog.h>     // Adafruit SleepyDog library   v1.6.3
#include <Wire.h>                   // Standard Wire library

/* =================== CONTROL CONSTANTS (uncomment these to change program behavior) =================
=======================================================================================================
Defining CODE_DEBUG_MODE will compile additional Serial prints and information
to aide in the troubleshooting process.  Uncomment this line and recompile 
*/ 
// *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  
//#define CODE_DEBUG_MODE // UNCOMMENT THIS FOR SERIAL DEBUG MESSAGES
// *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  

/*
Defining CIRCUIT_DEBUG_MODE will enter the board into a loop for troubleshooting board circuits.  This mode
will activate each pin/pump circuit and LED on a short recurring loop, allowing you to use a milti-meter to test the physical
circuit to ensure the MOSFETs are working correctly, as well as the pins on the board to activate them.
*/
// *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  
//#define CIRCUIT_DEBUG_MODE // UNCOMMENT THIS FOR CIRCUIT TROUBLESHOOTING
// *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  

/*
Defining CLOCK_SYNC_MODE will synchronize the clock with the last compiled time, and
attempt to reset the real time clock DS3231 registries to clear any invalid power state flags.
WARNING: After syncing clock with CLOCK_SYNC_MODE uncommented, recompile the code again after
commenting this line out.  This will ensure the registry is only reset once and not every time
the microcontroller reboots.
*/
// *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  
//#define CLOCK_SYNC_MODE // UNCOMMENT THIS TO SYNC CLOCK AND CLEAR OSCILLATOR REGISTRY FLAGS ** MUST COMMENT BACK OUT AND RECOMPILE AFTER SUCCESSFUL SYNC!
// *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  
// ==================== END CONTROL CONSTANTS =======================================================





// ==================== CONSTANTS ===================================================================
// Constants defined for the DS3231 registry and I2C slave addresses.  These come from the official
// data sheet for the DS3231.
#define REAL_TIME_CLOCK_SLAVE_ADDRESS 0x68
#define OSF_REGISTER_ADDRESS 0xF
#define EOSC_REGISTER_ADDRESS 0xE

// The FLAG_NORMAL_STATE is what the registry value should be when the oscillator is working normally.
// Per the data sheet, when OSF reads 1, the oscillator has been interrupted.  EOSC value of 1 means the
// oscillator will not run on battery power.
const int FLAG_NORMAL_STATE = 0;
const int START_HOUR_1 = 10; // 10am
const int STOP_HOUR_1 = 16; // 4pm
const int START_HOUR_2 = 22;  // 10pm
const int STOP_HOUR_2 = 4; // 4am
const int SLEEP_SECONDS = 600; // 10 minutes of sleep, adjust to the time you want the board to sleep for when pumps are off.
const int MAX_SLEEP_TIME_ALLOWED_BY_CONTROLLER = 8; // do not change this, it is a limitation of the microcontroller

// ================== Object Instantiations =========================================================

// Gives a handle for the RTC library, used for the adjust() function
// to grab time from local PC and to store time for checking when to activate pumps
RTC_DS1307 RTC;

// ==================== PIN MAPPINGS =================================================================
int LED = 12;
int pump1 = 3;
int pump2 = 5;



// =================== FUNCTION DEFINITIONS ========================================================== 
/*
Below are a set of functions for checking and setting the Oscillator Stop Flag (OSF) bit, the Enable OScillator (EOSC) bit,
and dealing with sleeping/waking and checking the time for the pumps and activating/deactivating the pumps.
The OSF bit is set to logical 1 when the real time clock oscillator has stopped.  This indicates a power failure or interruption
on the real time clock, and likely the battery needs to be checked.  The EOSC bit controls whether the oscillator will run when
the real time clock is powered by battery.  With EOSC set to 1, the clock will NOT progress when on battery only voltage, meaning
the clock will not keep time.  Both the OSF and EOSC bit must be set to logical 0 in order to persist time between main board power cycling.
*/

/*
The flashLEDCode is a helper function that flashes the LED to act as a visual indicator for exceptions.
LED pattern definitions:
- Rapid blinking, not stopping
    The microcontroller has encountered an error state, or the clock oscillator has been interrupted.
    Both cases need manual intervention to remedy.
- 5 sets of 3 bursts
    The microcontroller is in circuit debugging mode or clock sync mode.  Program execution will not continue in these modes.  Code must
    be recompiled and flashed with the control variables commented out.
- 20 bursts
    The pumps are reporting as on during the schedule hours they should be inactive.  Troubleshooting required.
- Single blink every ~10 minutes, seemingly no blink (depending on SLEEP_SECONDS constant)
    The microcontroller is operating normally and the pumps are both disabled.
- Single blink every ~5 seconds
    The microcontroller is operating normally and one of the pumps is active.
*/
void flashLEDCode(int indicatorCycles, int indicatorBursts) {
    
    for (int i = 0; i < indicatorCycles; ++i) {
        for (int j = 0; j < indicatorBursts; ++j) {
            digitalWrite(LED, HIGH);
            delay(100);
            digitalWrite(LED, LOW);
            delay(100);
        }
        delay(500);
    }
}

/*
The clearBit function takes an I2C address, registry address, specific bit, and buffer read size and sets
the bit to the FLAG_NORMAL_STATE bit.  It will check for the successful bit writing after attempting to change.
This is a helper function for the OSF and EOSC bit resetting.
Returns TRUE if the bit was successfully set to the FLAG_NORMAL_STATE value
*/ 
bool clearBit(int I2CslaveAddress, int registryAddress, int bitNumber, int readSize) {
    bool clearSuccess = true;

    #ifdef CODE_DEBUG_MODE
        Serial.println("Attempting to clear registry bits...");
    #endif

    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress);
    Wire.endTransmission(false);
    Wire.requestFrom(I2CslaveAddress,readSize);
    byte buf = Wire.read(); // storing the registry value in a buffer so we can manipulate the bits
    
    #ifdef CODE_DEBUG_MODE
        Serial.print("Registry bit value: ");
        Serial.println(bitRead(buf, bitNumber));
    #endif
    
    bitWrite(buf, bitNumber, FLAG_NORMAL_STATE); // Write the FLAG_NORMAL_STATE bit value to the buffer
    
    #ifdef CODE_DEBUG_MODE
        Serial.print("New value before write: ");
        Serial.println(bitRead(buf, bitNumber));
    #endif

    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress);
    Wire.write(buf);
    byte error = Wire.endTransmission();
    if (error) {
        #ifdef CODE_DEBUG_MODE
            Serial.println("Error on I2C Bus, registry bit status unknown.");
        #endif
        
        clearSuccess = false;
    }

    // Verify the bit was successfully written by checking the register one more time
    #ifdef CODE_DEBUG_MODE
        Serial.println("Verifying registry bit set...");
    #endif

    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress);
    Wire.endTransmission(false);
    Wire.requestFrom(I2CslaveAddress,readSize);
    buf = Wire.read();
    int bitFlag = bitRead(buf, bitNumber);
    if (bitFlag != FLAG_NORMAL_STATE) {
        #ifdef CODE_DEBUG_MODE
            Serial.println("bit value does not match FLAG_NORMAL_STATE, failed to set bit.");
        #endif
        clearSuccess = false;
    }
    Wire.endTransmission();

    return clearSuccess;
}

/*
The clearOscillatorBitFlags is a helper function for resetting the OSF and EOSC registry bits.
Returns TRUE on successful bit clearing.
*/
bool clearOscillatorBitFlags() {
    // call clear bit passing both the OSF and EOSC registers
    // returns true only IFF both bits successfully clear to logical value 0
    return (clearBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, OSF_REGISTER_ADDRESS, 7, 1) && clearBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, EOSC_REGISTER_ADDRESS, 7, 1));
}

/*
checkBit is a helper function for checking the state of the OSF and EOSC registry bits.
Returns TRUE if the flags match the defined FLAG_NORMAL_STATE.
*/
bool checkBit(int I2CslaveAddress, int registryAddress, int bitNumber, int readSize) {
    bool flagNormal = true;

    #ifdef CODE_DEBUG_MODE
        Serial.print("Reading bit - value is: ");
    #endif

    // This is the standard method to read a register on the I2C bus communication bus
    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress); // set the register to prepare to read
    Wire.endTransmission(false);  
    Wire.requestFrom(I2CslaveAddress, readSize); // read readSize byte(s) from the register
    byte registryBuffer = Wire.read();
    int bitFlag = bitRead(registryBuffer,bitNumber); // read the bitNumber'th bit of the register
    
    #ifdef CODE_DEBUG_MODE
        Serial.println(bitFlag);
    #endif
    
    if (bitFlag != FLAG_NORMAL_STATE) { // true if registry Buffer is 1, indicating oscillator was interrupted
        #ifdef CODE_DEBUG_MODE
        Serial.println(bitFlag);
        Serial.println("Bit flag indicates abnormal clock oscillator state.");
        #endif
        flagNormal = false;
    }

    return flagNormal;
}

/*
OSFBitIsNormal is a helper function for checking the OSF bit.
Returns TRUE if the OSF flag (bit 7 of the OSF registry) is 0 which indicates a normal state.
*/
bool OSFBitIsNormal() {
    return checkBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, OSF_REGISTER_ADDRESS, 7, 1);
}

/*
EOSCBitIsNormal is a helper function for checking the OSF bit.
Returns TRUE if the OSF flag (bit 7 of the EOSC registry) is 0 which indicates a normal state.
*/
bool EOSCBitIsNormal() {
    return checkBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, EOSC_REGISTER_ADDRESS, 7, 1);
}

/*
checkOscillatorNormalcy checks if either the OSF bit flag or EOSC bit flag have entered a non-normal state
which indicates a power interruption and clock time may be inaccurate.
Returns TRUE if the oscillator is functioning normally.
*/
bool checkOscillatorNormalcy() {
    bool oscillatorIsNormal = true;

    if (OSFBitIsNormal() && EOSCBitIsNormal()) { // Check both bits, if either returns false (abnormal state), alert
        oscillatorIsNormal = true;
    } else {
        oscillatorIsNormal = false;
    }
    return oscillatorIsNormal;
}

/*
startPump is a helper function to enable a pump on the pin passed to the function.
*/
void startPump(int pin) {
    //Use analog write because Pulse Width Modulation can be used to control speed
    analogWrite(pin, 210);  //255 is highest speed. Can set lower to run at lower RPMs. O is off.
}

/*
stopPump is a helper function to enable a pump on the pin passed to the function.
*/
void stopPump(int pin) {
    // Use digital write because it's just turning off equivalent to using analogWrite(pin, 0))
    digitalWrite(pin, LOW);
}

/*
getPumpRunningStatus returns the integer corresponding to the pump pin if a pump is running.
Returns 0 if no pump is running.
*
*/
int getPumpRunningStatus() {
    int pumpRunning = 0; // default to 0 / false
    if (digitalRead(pump1)) {
        pumpRunning = pump1; // return pin for pump 1 if it is running
    } else if (digitalRead(pump2)) {
        pumpRunning = pump2; // return pin for pump 2 if it is running
    }

    return pumpRunning;  //returns false if neither pump is running
}

// ====================== END FUNCTION DEFINITIONS ===================================================================



// ====================== EXECUTION START ============================================================================

// Setup runs every time the board reboots / repowered
// The setup() function in this sketch prepares serial if debugging is on, sets up the pins for output, and checks for clock sync mode
// to call the clock reset/registry clearing if necessary.  setup() also checks the registry flags for the real time clock
// and enters an infinite loop if it detects that the oscillator has stopped, flashing the external LED for awareness of the user.
void setup() {
    
    Serial.begin(115200);
    Wire.begin();

    // initialize pins as outputs
    pinMode(LED, OUTPUT);
    pinMode(pump1, OUTPUT);
    pinMode(pump2, OUTPUT);

    #ifdef CODE_DEBUG_MODE
        flashLEDCode(5, 3);
        Serial.println("Execution Start from setup() function...");
    #endif

    // =================== CLOCK_SYNC_MODE ==============================================================================
    // CLOCK_SYNC_MODE only compiles when the variable is defined in constants above.
    #ifdef CLOCK_SYNC_MODE
        while (true) {
            // Call the flashLEDCode() function, which flashes the LED in a special pattern to alert the user we're in a special mode.
            flashLEDCode(5, 3);
            Serial.println("CLOCK_SYNC_MODE is active.  Attempting to reset registers.  Click will sync to last compile time.");
            
            #ifdef CODE_DEBUG_MODE
                Serial.println("Clear OSF bit ...");
            #endif

            bool OSFresetSuccessful = clearBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, OSF_REGISTER_ADDRESS, 7, 1);
            
            #ifdef CODE_DEBUG_MODE
                Serial.println("Clear EOSC bit ...");
            #endif
            
            bool EOSCresetSuccessful = clearBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, EOSC_REGISTER_ADDRESS, 7, 1);
            if (!OSFresetSuccessful || !EOSCresetSuccessful) {
                Serial.println("Error setting registries or detecting valid state.  Troubleshoot clock registers.");
                while (true) {
                    flashLEDCode(1, 100);
                }
            } else {
            // Grab time from computer, last compile time is used.  
            RTC.adjust(DateTime(__DATE__, __TIME__));
                Serial.println("Registers reset, reflash code with CLOCK_SYNC_MODE off!");            
                while(true) {
                    flashLEDCode(5,3);
                }
            }
        }
    #endif

    // ================== END CLOCK_SYNC_MODE =========================================================================

    // =================== CIRCUIT_DEBUG_MODE ==============================================================================
    // CIRCUIT_DEBUG_MODE is for troubleshooting the physical electrical circuit.  It loops through each pin to check the LED and
    // MOSFETs.  Only compiles when the constant is defined above.
    #ifdef CIRCUIT_DEBUG_MODE
        while (true) {
            // Call the flashLEDCode() function, which flashes the LED in a special pattern to alert the user we're in a special mode.
            flashLEDCode(5, 3);

            //Test pump1 voltage:
            analogWrite(pump1, 210);
            delay(5000);
            digitalWrite(pump1, LOW);
            delay(1000);

            //Test pump2 voltage:
            analogWrite(pump2, 210);
            delay(5000);
            digitalWrite(pump2, LOW);
            delay(1000);
        }
    #endif
    // =================== END CIRCUIT_DEBUG_MODE ==============================================================================
    
    #ifdef CODE_DEBUG_MODE
        Serial.println("Checking oscillator bit status...");
    #endif

    // First check the state of the oscillator in case oscillator lost power
    bool oscillatorIsNormal = checkOscillatorNormalcy();

    if (!oscillatorIsNormal) {
        #ifdef CODE_DEBUG_MODE
            Serial.println("Power failture on Clock Oscillator detected. Check Clock Battery!");
            Serial.println("Entering infinite blinking loop. Program must be reflashed with clock sync to progress.");
        #endif

        // Flash LED on infinite loop, alert for clock check state
        // WARNING:  This will drian battery since the program will never enter deep sleep.
        //  Clock must be resynced and registers cleared to exit this state!
        while (true) {
            flashLEDCode(1, 100);
        }
    } else {
        #ifdef CODE_DEBUG_MODE
            DateTime now = RTC.now();
            Serial.println("Clock states read good.  Proceed with Setup loop.");
            Serial.print("Current time on RTC: ");
            Serial.print(now.year(), DEC);
            Serial.print('/');
            Serial.print(now.month(), DEC);
            Serial.print('/');
            Serial.print(now.day(), DEC);
            Serial.print(' ');
            Serial.print(now.hour(), DEC);
            Serial.print(':');
            Serial.print(now.minute(), DEC);
            Serial.print(':');
            Serial.print(now.second(), DEC);
            Serial.println();
        #endif
        delay(500);
    }

}

void loop() {

    DateTime now = RTC.now();

     #ifdef CODE_DEBUG_MODE
        Serial.print("Current time: ");
        Serial.print(now.year(), DEC);
        Serial.print('/');
        Serial.print(now.month(), DEC);
        Serial.print('/');
        Serial.print(now.day(), DEC);
        Serial.print(' ');
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.print(now.second(), DEC);
        Serial.println();
    #endif

    /*
    Per the requirements of pump functionality, pump1 should be active from 10am / 10:00
    until 4pm / 16:00.  Pump2 should be active from 10pm / 22:00 until 4am / 04:00.  This control
    structure checks the hour and starts the pump if it should be started.  
    */
    if ((now.hour() >= START_HOUR_1) && (now.hour() < STOP_HOUR_1)) {
        // Pump1 should be running
        startPump(pump1);
        #ifdef CODE_DEBUG_MODE
            Serial.println("Pump 1 should be running.");
        #endif
    } else if ((now.hour() >= START_HOUR_2) || (now.hour() < STOP_HOUR_2)) {
        // Pump2 should be running
        startPump(pump2);
        #ifdef CODE_DEBUG_MODE
            Serial.println("Pump 2 should be running.");
        #endif
    } else {
        stopPump(pump1);
        stopPump(pump2);
        #ifdef CODE_DEBUG_MODE
            Serial.println("No pumps should be running.");
        #endif
        // no pumps should be running, but check just in case...
        if (getPumpRunningStatus()) {
            #ifdef CODE_DEBUG_MODE
                Serial.println("Pump reported running during off hours - troubleshooting required");
            #endif
            // Flash the LED for abnormal condition
            flashLEDCode(1, 20);
        }

        #ifdef CODE_DEBUG_MODE
            Serial.print("Sleeping for ");
            Serial.print(SLEEP_SECONDS);
            Serial.println(" seconds.");
        #endif

        delay(10);  // small delay is needed before sleep to finish serial prints
        int sleepMS = 0; // for serial prints to see how long slept
        for (int i = 0; i <  SLEEP_SECONDS / MAX_SLEEP_TIME_ALLOWED_BY_CONTROLLER; ++i) {
            sleepMS += (Watchdog.sleep() / 1000); // maximum sleep for this board is 8 seconds, so keep sleeping on a loop
            
            
            #ifdef CODE_DEBUG_MODE
                delay(10);
                Serial.print("Been asleep for ");
                Serial.print(sleepMS);
                Serial.println(" seconds.");
                delay(10);
            #endif
        }
        
        #ifdef CODE_DEBUG_MODE
            Serial.print("slept for total: ");
            Serial.println(sleepMS, DEC);
        #endif
        
    }

    // Flash LED once per loop.  On sleep loops, LED will only flash every 10 min
    // While pump is running, LED will flash every 5 seconds.
    flashLEDCode(1, 1);

    // Brief pause before next loop
    delay(5000);
}
