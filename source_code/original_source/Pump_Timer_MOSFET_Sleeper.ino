
//Arthor- Vincent Moriarty
//Date- June 29 2016


/*License:

  This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 United 
  States License. To view a copy of this license, visit: 
    http://creativecommons.org/licenses/by-sa/3.0/us/ 
  or send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, 
  California, 94041, USA.

  */
//The bellow code controlls two different peristaltic pumps (using MOSFETs)
//A RTC clock is used to keep time, with Start HR:MM and Stop HH:MM for each Pump
// Timer can handle overnight (passes 24 HR mark) controlling
//Power can be interupted to unit and pumps (as long as battery is good on RTC)


// Date and time functions using a DS3231 RTC connected via I2C and Wire Lib

#include <Wire.h>
#include <RTClib.h>  // Uses DateTime values
#include <TimeLib.h>
#include <stdio.h>
//#include <DS1307RTC.h>  // Uses time_t values
#include <DS3231.h> // Uses time_t values

//For sleep modes
#include <avr/sleep.h>
#include <avr/wdt.h>


// watchdog interrupt
ISR(WDT_vect) {
  wdt_disable();  // disable watchdog
}

void myWatchdogEnable(const byte interval) {
  MCUSR = 0;                       // reset various flags
  WDTCSR |= 0b00011000;            // see docs, set WDCE, WDE
  WDTCSR = 0b01000000 | interval;  // set WDIE, and appropriate delay

  wdt_reset();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();  // now goes to Sleep and waits for the interrupt
}




//This is from the RTClib, not the DS1307RTC.h
DS3231 RTC;  // Only use if using a DS1307 (do not 'include DS1307' if using this- causes conflict)



// Start Time and End Times for PUMP 1 (NO LEADIN ZEROS--"8" not "08")
byte startHour1 = 10;  //10
byte startMinute1 = 0;
byte endHour1 = 16;  //16
byte endMinute1 = 0;


// Start and End Time for PUMP 2
byte startHour2 = 22;  //22
byte startMinute2 = 0;
byte endHour2 = 4;  //4
byte endMinute2 = 0;



//Define MOSFET Pin Locations (must be PWM pins)
int pump1 = 3;  //Define Pin for PUMP 1 (Day Time
int pump2 = 5;  //  Define Pin for PUMP 2 (Night Time)

//Define LED pin location
int LED = 12;



//Set Boolean values if Pump is running
boolean pump1Running = true;
boolean pump2Running = true;


//Define type "tmElements_t" and type "time_t"
//time_t variables store a the timestamps since 01/01/1970 (Unix epoch) which are easier to work with
//tmElements structures are more human friendly

//'tm' variable stores current time (used for both Pump 1 and Pump 2 controlls)
tmElements_t tm;
time_t now_;


//For Pump 1
tmElements_t tm_hour_start1;
tmElements_t tm_hour_end1;

time_t t_hour_start1;
time_t t_hour_end1;


//For Pump 2
tmElements_t tm_hour_start2;
tmElements_t tm_hour_end2;

time_t t_hour_start2;
time_t t_hour_end2;


//boolean for TRUE (1) or False (0) Whether or not to update time stamp
boolean update_tm1 = 1;

boolean update_tm2 = 1;




void setup() {


  Serial.begin(9600);
  Wire.begin();
  RTC.begin();


  //The 'isrunning check' only work when using RTClib (not #include <DS1307RTC.h>)
  // Notify if the RTC isn't running
  if (!RTC.isrunning()) {
    Serial.println("RTC is NOT running");
  }




  //Grab time from computer if need to (remove // slashes)
  RTC.adjust(DateTime(__DATE__, __TIME__));  //UPDATES FROM COMPUTER








  // Set all pump controll pins as output
  pinMode(pump1, OUTPUT);
  pinMode(pump2, OUTPUT);

  //Define LED Pin

  pinMode(LED, OUTPUT);
}

void loop() {


  //get current timestamp
  time_t now_ = now();  //RTC.now() gives a date time, so must convert to a time_t value with "now();"



  // make current date and time structure
  //breaktime converts timestamp value to tmElements structure
  breakTime(now_, tm);


  //Will update the time if update_tm=1
  if (update_tm1) {

    // make auxiliary structures to be more human editable and friendly
    //copy contents of tm to the other auxiliary structures

    memcpy(&tm_hour_start1, &tm, sizeof(tm));
    memcpy(&tm_hour_end1, &tm, sizeof(tm));


    // change auxiliary structures to meet your start and end schedule
    tm_hour_start1.Hour = startHour1;
    tm_hour_start1.Minute = startMinute1;
    tm_hour_start1.Second = 0;
    tm_hour_end1.Hour = endHour1;
    tm_hour_end1.Minute = endMinute1;
    tm_hour_end1.Second = 0;


    // reverse process to get timestamps
    // Now have the more human friendly start and end structures
    // Use the makeTime function to convert tmElements_t structures to time_t

    t_hour_start1 = makeTime(tm_hour_start1);
    t_hour_end1 = makeTime(tm_hour_end1);


    // check if end time is past midnight and correct if needed
    if (startHour1 > endHour1)  //past midnight correction
      t_hour_end1 = t_hour_end1 + SECS_PER_DAY;
  }


  //Controll Structure Pump 1
  if ((t_hour_start1 <= now_) && (now_ <= t_hour_end1)) {
    /* if we got a valid schedule, don't change the tm_hour structures and the 
    respective t_hour_start and t_hour_end timestamps. They should be updated 
    after exiting the valid schedule */
    if (update_tm1) {
      update_tm1 = 0;
    }

    pump1GO();
    pump1Running = true;

  } else {
    if (update_tm1 == 0) {
      update_tm1 = 1;
    }

    pump1STOP();
    pump1Running = false;
  }





  //Will update the time if update_tm=1
  if (update_tm2) {

    // make auxiliary structures to be more human editable and friendly
    //copy contents of tm to the other auxiliary structures

    memcpy(&tm_hour_start2, &tm, sizeof(tm));
    memcpy(&tm_hour_end2, &tm, sizeof(tm));


    // change auxiliary structures to meet your start and end schedule
    tm_hour_start2.Hour = startHour2;
    tm_hour_start2.Minute = startMinute2;
    tm_hour_start2.Second = 0;
    tm_hour_end2.Hour = endHour2;
    tm_hour_end2.Minute = endMinute2;
    tm_hour_end2.Second = 0;

    // reverse process to get timestamps
    // Now have the more human friendly start and end structures
    // Use the makeTime function to convert tmElements_t structures to time_t

    t_hour_start2 = makeTime(tm_hour_start2);
    t_hour_end2 = makeTime(tm_hour_end2);

    // check if end time is past midnight and correct if needed

    if (startHour2 > endHour2)  //past midnight correction
      t_hour_end2 = t_hour_end2 + SECS_PER_DAY;
  }

  //Controll Structure Pump 2
  if ((t_hour_start2 <= now_) && (now_ <= t_hour_end2)) {
    /* if we got a valid schedule, don't change the tm_hour structures and the 
    respective t_hour_start and t_hour_end timestamps. They should be updated 
    after exiting the valid schedule */
    if (update_tm2) {
      update_tm2 = 0;
    }

    pump2GO();
    pump1Running = true;

  } else {
    if (update_tm2 == 0) {
      update_tm2 = 1;
    }

    pump2STOP();
    pump2Running = false;
  }



  //Controll Structure for sleeping


  if ((pump1Running == false) && (pump2Running == false)) {
    int i;
    for (i = 0; i < 1; i++)  // <1= 8 seconds. 8*75=  set to 600 seconds = 10min if want to set to  wake up every 10min to see if should be running
    {
      myWatchdogEnable(0b100001);  //8 seconds
    }

    //LED on
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
  }

  //For TroubleShooting- print out clock times

  //Serial.println(t_hour_start1);
  //Serial.println(t_hour_end1);
  //Serial.println(now_);

  //Serial.println(t_hour_start2);
  //Serial.println(t_hour_end2);
  //Serial.println(now_);


  DateTime nowsy = RTC.now();
  setTime(nowsy.hour(), nowsy.minute(), nowsy.second(), nowsy.day(), nowsy.month(), nowsy.year());

  Serial.print("Current time: ");
  Serial.print(nowsy.year(), DEC);
  Serial.print('/');
  Serial.print(nowsy.month(), DEC);
  Serial.print('/');
  Serial.print(nowsy.day(), DEC);
  Serial.print(' ');
  Serial.print(nowsy.hour(), DEC);
  Serial.print(':');
  Serial.print(nowsy.minute(), DEC);
  Serial.print(':');
  Serial.print(nowsy.second(), DEC);
  Serial.println();

  Serial.println();



  delay(1000);


  //END VOID LOOP
}


//MOTOR Functions (Lower Speeds= Lower Voltages to pumps)

void pump1GO() {
  //Use analog cuz PWM can controll speed
  analogWrite(pump1, 210);  //255 is highest speed. Can set lower to run at lower RPMs. O is off.

  //Blink LED Faster
  digitalWrite(LED, HIGH);
  delay(250);
  digitalWrite(LED, LOW);
  delay(3000);
}

void pump2GO() {

  analogWrite(pump2, 210);  //255 is highest speed. Can set lower to run at lower RPMs. O is off.

  //Blink LED Faster
  digitalWrite(LED, HIGH);
  delay(250);
  digitalWrite(LED, LOW);
  delay(3000);
}

void pump1STOP() {

  // Use digital cuz just turnning off (could use analogWrite and set speed to 0)
  digitalWrite(pump1, LOW);
}

void pump2STOP() {


  digitalWrite(pump2, LOW);
}
