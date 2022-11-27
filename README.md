# CSUN Lagrangian Water Sampler
Microcontroller code for the CSUN Lagrangian Water Sampler Project

This project was to fix a maker device created for California State University, Northridge.  The device runs using a Arduino compatible Mini Ultra 8Mhz microcontroller with ATmega328P processor.  The device controlls two peristaltic water pumps for two periods of time during each day.  Based on comments in the original code, it was originally created by Vincent Moriarty on June 29, 2016.  Since that time, the real time clock (DS3231) used to keep time in-between board power failures had run out of power causing the clock to reset.  As such, the pumps stopped working on their intended schedule.  During troubleshooting, the device was rendered inoperable and users were unable to reset the time.

The device was also created without any technical documentation.  This project scope was to understand the original circuit, troubleshoot the software and hardware, and restore it to original functionality.  Additional tasks are to document the circuit, components, and code and create a manual for how to reflash the code and restore it to operating state.

The code was completely rewritten but the original functionality was mostly retained.  The major change is that the new code will check for the Oscillator Stop Flag bit every time the controller boots.  This bit keeps track of when the real time clock oscillator stops, meaning the time is most likely not accurate.  If the OSF bit is detected as logical 1, the board goes into a LED flash loop to alert the user there is a problem.

Several other changes have been made to make the code easier to understand and modify.  The original code is retained in this repository for historical records.
