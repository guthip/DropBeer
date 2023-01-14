/*Written by Hans Spanjaart, 2017, http://lat13.com .  info@lat13.com

 Drop Beer control for board computer

 7 Segment LED lib: http://playground.arduino.cc/Main/SevenSegmentLibrary
 Timer lib: http://playground.arduino.cc/Code/Timer
 LiquidCrystal lib: standard Arduino

 */

#ifndef DROPBEER_H_
#define DROPBEER_H_

// Macros to simplify programming the digital ports
// no more mixing Arduino nano pins and Digital ports
#define DP0 0 // Rx
#define DP1 1 // Tx
#define DP2 2
#define DP3 3
#define DP4 4
#define DP5 5
#define DP6 6
#define DP7 7
#define DP8 8
#define DP9 9
#define DP10 10
#define DP11 11
#define DP12 12
#define DP13 13


// SRT1 board support package

#define LCD_PRESENT
//#define NC_BUTTON // normally closed buttons used for menu/horn/enter

#ifdef LCD_PRESENT
#include <LiquidCrystal.h>
#define LCD_CONNECTIONS DP10, DP9, DP8, A0, A1, A2, A3, A4, A5, DP12, DP7
//#define CONTRAST 0 // 0 = max
#define CONTRAST 100
#endif

#define menuSwitch DP6 // from menu switch
#define enterSwitch A6 // Using up/down/rest feature
#define Vbat_in A7 // from horn switch
#define PiPwr DP13 // enable power to raspberry pi


//#define VBAT_MINIMUM 11.00 // 11V = minimum battery voltage for board computer to start
#define VBAT_MINIMUM 4.00


#endif /* DROPBEER_H_ */
