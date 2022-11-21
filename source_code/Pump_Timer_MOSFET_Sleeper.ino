// Lagrangian Water Sampler
// MOSFET based 12v pump activation using DS3231 Real Time Clock
// Refactored code based on the original sketch by Vincent Moriarty, Date- June 29 2016

// Author:  Richard Scafidi
// Date: 25 November 2022
// richard@scafidi.dev
// This code was shipped with electronic schematics and a troubleshooting / maintenance manual

#include <RTClib.h>
#include <Wire.h>

/* =================== CONTROL CONSTANTS (uncomment these to change program behavior) =================
=======================================================================================================
Defining DEBUG_MODE will compile additional Serial prints and information
to aide in the troubleshooting process.  Uncomment this line and recompile 
*/ 
#define DEBUG_MODE // UNCOMMENT THIS FOR SERIAL DEBUG MESSAGES

/*
Defining CLOCK_SYNC_MODE will synchronize the clock with the last compiled time, and
attempt to reset the real time clock DS3231 registries to clear any invalid power state flags.
WARNING: After syncing clock with CLOCK_SYNC_MODE uncommented, recompile the code again after
commenting this line out.  This will ensure the registry is only reset once and not every time
the microcontroller reboots.
*/ 
//#define CLOCK_SYNC_MODE // UNCOMMENT THIS TO SYNC CLOCK AND CLEAR OSCILLATOR REGISTRY FLAGS ** MUST COMMENT BACK OUT AND RECOMPILE AFTER SUCCESSFUL SYNC!

// ==================== END CONTROL CONSTANTS =======================================================





// ==================== CONSTANTS ===================================================================
// Constants defined for the DS3231 registry and I2C slave addresses.  These come from the official
// data sheet for the DS3231.
#define REAL_TIME_CLOCK_SLAVE_ADDRESS 0x68
#define OSF_REGISTER_ADDRESS 0xF
#define EOSC_REGISTER_ADDRESS 0xE
const int FLAG_NORMAL_STATE = 0;

// ================== Object Instantiations =========================================================

// Gives a handle for the RTC library, used primary for the adjust() function
// to grab time from local PC.
RTC_DS1307 RTC;

// ==================== PIN MAPPINGS =================================================================
int LED = 12;
int pump1 = 3;
int pump2 = 5;



// =================== FUNCTIONS ====================================================================

/*
This function checks the OSF and EOSC registers on the DS3231 real time clock
The I2C Slave address for the DS3231 is 0x68
The OSF register address is 0xF
The EOSC register address is 0xE
The OSF register bit 7 indicates if the real time clock oscillator has stopped
A OSF bit 7 of logical value 1 indicates the oscillator has been interrupted
  which means that the current time being reported by the real time clock likely
  is not accurate.  The device should be checked and recalibrated.
The EOSC register bit 7 sets the oscillator mode.  When logical 1 is set, the oscillator
  will not operate when VCC is disconnected, even if a battery is connected and good.
  EOSC register bit 7 set to 0 ensures the oscillator will continue to operate on battery mode.
This function will check the bits and reset both the OSF and EOSC to 0.  Note that this should
    only be done when the sketch is being recompiled while connected to a local machine to sync time.
*/ 

/*
The clear bit function takes an I2C address, registry address, specific bit, and buffer read size and sets
the bit to the FLAG_NORMAL_STATE bit.  It will check for the successful bit writing after attempting to change.
This is a helper function for the OSF and EOSC bit resetting.
*/ 
bool clearBit(int I2CslaveAddress, int registryAddress, int bitNumber, int readSize) {
    bool clearSuccess = true;
    
    #ifdef DEBUG_MODE
        Serial.println("Attempting to clear registry bits...");
    #endif

    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress);
    Wire.endTransmission(false);
    Wire.requestFrom(I2CslaveAddress,readSize);
    byte buf = Wire.read();
    #ifdef DEBUG_MODE
        Serial.print("Registry bit value: ");
        Serial.println(bitRead(buf, bitNumber));
    #endif
    bitWrite(buf, bitNumber, FLAG_NORMAL_STATE);
    #ifdef DEBUG_MODE
        Serial.print("New value before write: ");
        Serial.println(bitRead(buf, bitNumber));
    #endif
    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress);
    Wire.write(buf);
    byte error = Wire.endTransmission();
    if (error) {
        #ifdef DEBUG_MODE
            Serial.println("Error on I2C Bus, registry bit status unknown.");
        #endif
        clearSuccess = false;
    }
    // Verify the bit was successfully written by checking the register one more time
    #ifdef DEBUG_MODE
        Serial.println("Verifying registry bit set...");
    #endif
    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress);
    Wire.endTransmission(false);
    Wire.requestFrom(I2CslaveAddress,readSize);
    buf = Wire.read();
    int bitFlag = bitRead(buf, bitNumber);
    if (bitFlag != FLAG_NORMAL_STATE) {
        #ifdef DEBUG_MODE
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

    #ifdef DEBUG_MODE
        Serial.print("Reading bit: ");
    #endif
    
    // This is the standard method to read a register on the I2C bus communication bus
    Wire.beginTransmission(I2CslaveAddress);
    Wire.write(registryAddress); // set the register to prepare to read
    Wire.endTransmission(false);  
    Wire.requestFrom(I2CslaveAddress, readSize); // read readSize byte(s) from the register
    byte registryBuffer = Wire.read();
    int bitFlag = bitRead(registryBuffer,bitNumber); // read the bitNumber'th bit of the register
    #ifdef DEBUG_MODE
        Serial.println(bitFlag);
    #endif
    if (bitFlag != FLAG_NORMAL_STATE) { // true if registry Buffer is 1, indicating oscillator was interrupted
        #ifdef DEBUG_MODE
            Serial.println(bitFlag);
            Serial.println("Bit flag indicates abnormal clock oscillator state.");
        #endif
        flagNormal = false;
    }

    return flagNormal;
}

/*
OFSBitIsNormal is a helper function for checking the OSF bit.
Returns TRUE if the OSF flag (bit 7 of the OSF registry) is 0 which indicates a normal state.
*/
bool OFSBitIsNormal() {
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
    
    if (OFSBitIsNormal() && EOSCBitIsNormal()) { // Check both bits, if either returns false (abnormal state), alert
        oscillatorIsNormal = true;
    } else {
        oscillatorIsNormal = false;
    }
    return oscillatorIsNormal;
}
// ====================== END FUNCTION DEFINITIONS ===================================================================

// ====================== EXECUTION START ============================================================================

// Setup runs every time the board reboots / repowered
void setup() {
    Serial.begin(9600);
    Wire.begin();

    // initialize pins as outputs
    pinMode(LED, OUTPUT);
    pinMode(pump1, OUTPUT);
    pinMode(pump2, OUTPUT);

    #ifdef DEBUG_MODE
        Serial.println("Execution Start from setup() function...");
    #endif

// =================== CLOCK_SYNC_MODE ==============================================================================
// CLOCK_SYNC_MODE only compiles when the variable is defined in constants above.

    #ifdef CLOCK_SYNC_MODE
        Serial.println("CLOCK_SYNC_MODE is active.  Attempting to reset registers.  Click will sync to last compile time.");
        #ifdef DEBUG_MODE
            Serial.println("Clear OSF bit ...");
        #endif
        bool OSFresetSuccessful = clearBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, OSF_REGISTER_ADDRESS, 7, 1);
        #ifdef DEBUG_MODE
            Serial.println("Clear EOSC bit ...");
        #endif
        bool EOSCresetSuccessful = clearBit(REAL_TIME_CLOCK_SLAVE_ADDRESS, EOSC_REGISTER_ADDRESS, 7, 1);
        if (!OSFresetSuccessful || !EOSCresetSuccessful) {
            Serial.println("Error setting registries or detecting valid state.  Troubleshoot clock registers.");
        } else {
            // Grab time from computer, last compile time is used.  
            RTC.adjust(DateTime(__DATE__, __TIME__));
        }
    #endif

// ================== END CLOCK_SYNC_MODE =========================================================================
    #ifdef DEBUG_MODE
        Serial.println("Checking oscillator bit status...");
    #endif
    // First check the state of the oscillator in case oscillator lost power
    bool oscillatorIsNormal = checkOscillatorNormalcy();
    
    if (!oscillatorIsNormal) {
        #ifdef DEBUG_MODE
            Serial.println("Power failture on Clock Oscillator detected. Check Clock Battery!");
            Serial.println("Entering infinite blinking loop. Program must be reflashed with clock sync to progress.");
        #endif
        // Flash LED on infinite loop, alert for clock check state
        // WARNING:  This will drian battery since the program will never enter deep sleep.
        //  Clock must be resynced and registers cleared to exit this state!
        while (true) {
            digitalWrite(LED, HIGH);
            delay(100);
            digitalWrite(LED, LOW);
            delay(100);
        }
    } else {
        #ifdef DEBUG_MODE
            Serial.println("Clock states read good.  Proceed with Setup loop.");
        #endif
        delay(500);
    }

}

void loop() {
    
    DateTime now = RTC.now();
    
    #ifdef DEBUG_MODE
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
        delay(10000);
    #endif

}
