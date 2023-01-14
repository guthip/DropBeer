/* ================================================================================
  Drop Beer motor compartment sensor hub

  Feb 7, 2020

  Version 3.0



  Sensors:
  - V/I battery 1 over I2C
  - V/I battery 2 over I2C
  - RPM using open collector hall effect sensor
  - temperature over 1wire
  - flood detection using flood switch (normally open) // currently disabled as there's no way to switch off an alarm

  Outputs:
  - ESP8266 wifi, generating Signal K messages
    - updates every 2 seconds
  - LED matrix display 4x8x8
    - shows startup message "Drop Beer"
    - shows RPM when > 0
    - shows "noWiFi" when no Wifi connection
    - shows spinning wheel when active and connected
   ================================================================================
*/

#define debug 0
#define am_on_boat 1


/* ================================================================================
   Secrets
   ================================================================================
*/


#if am_on_boat
#define MY_SSID  "GoFree-5670"
#define MY_PASSWORD "XXXXXX"
#else
#define MY_SSID  "CIA HotSpot"
#define MY_PASSWORD "XXXXXX"
#endif


/* ================================================================================
   Globals
   ================================================================================
*/

// Simplify debug message instantiation
#if debug
#define DBG_println(X) Serial.println(X)
#define DBG_print(X) Serial.print(X)
#else
#define DBG_println(X)
#define DBG_print(X)
#endif


#include "Arduino.h"
#include <stdio.h>


// WiFi
#include "ESP8266WiFi.h"
#include "WiFiUdp.h"
// Instanciate UDP object
WiFiUDP Udp;

// Over The Air (OTA) update
// https://github.com/scottchiefbaker/ESP-WebOTA
#include <WebOTA.h>

// Wifi info
IPAddress staticIP(10, 10, 10, 101);
IPAddress gateway(10, 10, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns( 10, 10, 10, 1);
const char * ssid = MY_SSID;
const char * pwd = MY_PASSWORD;
const uint16_t port = 55557;      // SignalK uses this port.
const char * host = "10.10.10.1"; // ip number of the SignalK server.


// Float switch alarm
#define FLOAT_SWITCH_INPUT 12

// I2C lib, INA219 ADC
#include <Adafruit_INA219.h>
#include <Wire.h>
Adafruit_INA219 ina219A(0x40);
Adafruit_INA219 ina219B(0x41);


// One wire temp sensor
#include <DallasTemperature.h>
#include <OneWire.h>
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temp_sensor(&oneWire);  // Pass the oneWire reference to Dallas Temperature.


// Revolution sensor
#define RPM_SENSOR_PIN 3 // input pin
// RPM measured and diaplyed always
#include <Ticker.h>
Ticker rpm_ticker;

// LED matrix 4 x 8x8, MAX7219
#include <MD_MAX72xx.h> // Added Drop Beer HardWare options
#include <SPI.h>
#define HARDWARE_TYPE MD_MAX72XX::DB_HW
#define MAX_DEVICES 4
#define CS_PIN    16
// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Setup watchdog timer - will force hard reset through WDT when things appear to hang
// when reaching MAXLOOPS: force a watchdog reset for a fresh start
// loops are about 1 second each
#define MAXLOOPS (60*60) // number of loop() cycles before a reset is forced
#define MAX_NBR_WIFI_TRIES 100 // number of cycles to wait for WIFI to connect before reset is forced

// Global variables
int loopcounter = 0;

// RPM measurement
volatile float time_delta = 0;
volatile float time_last = 0;
volatile bool motor_running = false;

// rpm
volatile int rpm = 0;


/* ================================================================================
  Send datapackage through WiFi to host (null)
   ================================================================================
*/

void Send_to_SignalK_null(String path) {
  // Settings for SignalK port and SignalK server.
  String cmd;

  cmd = "{\"updates\": [{\"$source\": \"ENGINE\",\"values\":[ {\"path\":\"";
  cmd += path; cmd += "\","; cmd += "\"value\": \0";
  cmd += "}]}]}\0";

  DBG_println("Send_to_SignalK (string)");
  DBG_println(cmd);
  DBG_println(cmd.length());

  char cmdc[cmd.length() + 1];      // Convert the String to an array of characters.
  strncpy(cmdc, cmd.c_str(), sizeof(cmdc)); // Convert from String to array of characters.
  Udp.beginPacket(host, port);      // Connect to to server and prepare for UDP transfer.
  Udp.write(cmdc);                  // Send the message to the SignalK server.
  Udp.endPacket();                  // End the connection.
  webota.delay(10);                        // Short delay to recover.
}


/* ================================================================================
  Send datapackage through WiFi to host (String)
   ================================================================================
*/

void Send_to_SignalK_string(String path, String value) {
  // Settings for SignalK port and SignalK server.
  String cmd;

  cmd = "{\"updates\": [{\"$source\": \"ENGINE\",\"values\":[ {\"path\":\"";
  cmd += path; cmd += "\","; cmd += "\"value\":";
  cmd += value;
  cmd += "}]}]}\0";

 DBG_println("Send_to_SignalK (string)");
  DBG_println(cmd);
  DBG_println(cmd.length());

  char cmdc[cmd.length() + 1];      // Convert the String to an array of characters.
  strncpy(cmdc, cmd.c_str(), sizeof(cmdc)); // Convert from String to array of characters.
  Udp.beginPacket(host, port);      // Connect to to server and prepare for UDP transfer.
  Udp.write(cmdc);                  // Send the message to the SignalK server.
  Udp.endPacket();                  // End the connection.
  webota.delay(10);                        // Short delay to recover.
}


/* ================================================================================
  Send datapackage through WiFi to host (float)
   ================================================================================
*/

void Send_to_SignalK(String path, float value) {
  // Settings for SignalK port and SignalK server.
  String cmd;
  char valuestring[6];

  cmd = "{\"updates\": [{\"$source\": \"ENGINE\",\"values\":[ {\"path\":\"";
  cmd += path; cmd += "\","; cmd += "\"value\":";
  dtostrf((double)value, 3, 2, valuestring); // Convert double to a string
  cmd += valuestring;
  cmd += "}]}]}\0";
  DBG_println("Send_to_SignalK");
  DBG_println(cmd);
  DBG_println(cmd.length());

  char cmdc[cmd.length() + 1];      // Convert the String to an array of characters.
  strncpy(cmdc, cmd.c_str(), sizeof(cmdc)); // Convert from String to array of characters.
  Udp.beginPacket(host, port);      // Connect to to server and prepare for UDP transfer.
  Udp.write(cmdc);                  // Send the message to the SignalK server.
  Udp.endPacket();                  // End the connection.
  webota.delay(10);                        // Short delay to recover.

} /* End Send_to_SignalK */


/* ================================================================================
   Capture revolutions
   ================================================================================
*/

void ICACHE_RAM_ATTR rpm_ISR()
{
  DBG_print("*");

  time_delta = (micros() - time_last);
  time_last = micros();
  motor_running = true;
}

/* ================================================================================
   Show revolutions on display every second (tick)
   ================================================================================
*/

void  rpm_tick() {
  calculate_rpm();
  show_rpm();
}


/* ================================================================================
   Calculate revolutions
   ================================================================================
*/

void  calculate_rpm()
{
  unsigned long copyTime;

  if (motor_running)
  {
    //Update The copy RPM sample
    noInterrupts();
    copyTime = time_delta;
    interrupts();
    rpm = 60 * 1000000 / copyTime;
    motor_running = false; //assume stopped
  } else
  {
    rpm = 0; // not running
  }

  // cover error conditions
  if (rpm > 9999) rpm = 9999;
  if (rpm < 0) rpm = 9999;

  DBG_print(rpm);
  DBG_println(" rpm");

}


/* ================================================================================
  Send revolutions through WiFi
   ================================================================================
*/

void send_rpm() {
  // Send over WiFi
  Send_to_SignalK_string("propulsion.p0.state", (rpm == 0) ? "\"stopped\"" : "\"started\"" );
  Send_to_SignalK_string("propulsion.p0.label", "\"engine\"" );
  Send_to_SignalK("propulsion.p0.revolution", rpm);
}


/* ================================================================================
  Format revolution message for LED MATRIX display
   ================================================================================
*/

// Show on LED matrix display
// rpm -> show on display
// no rpm, no wifi -> show nowifi on display
// no rpm, wifi -> clear

void  show_rpm() {

  char message[10] = {"noWiFi"};

  if (rpm > 0) {
    sprintf(message, "%04d\0", rpm);
    printText(0, MAX_DEVICES - 1, message, 3);
  }
  if ((rpm == 0) && ((WiFi.status() != WL_CONNECTED))) {
    // default message
    printText(0, MAX_DEVICES - 1, message, 1);
  }
  if ((rpm == 0) && ((WiFi.status() == WL_CONNECTED))) {
    (loopcounter % 2 == 0) ? sprintf(message, "%5s", "x") : sprintf(message, "%5s", "+");
    printText(0, MAX_DEVICES - 1, message, 1);
  }
}


/* ================================================================================
  Print on LED matrix display
   ================================================================================
*/

void  printText(uint8_t modStart, uint8_t modEnd, char *pMsg, uint8_t char_spacing)
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
{
  uint8_t   state = 0;
  uint8_t   curLen = 0;
  uint16_t  showLen = 0;
  uint8_t   cBuf[8];
  int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;

  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  do     // finite state machine to print the characters in the space available
  {
    switch (state)
    {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0')
        {
          showLen = col - (modEnd * COL_SIZE);  // padding characters
          state = 2;
          break;
        }

        // retrieve the next character form the font file
        showLen = mx.getChar(*pMsg++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
      // !! deliberately fall through to next state to start displaying

      case 1: // display the next part of the character
        mx.setColumn(col--, cBuf[curLen++]);

        // done with font character, now display the space between chars
        if (curLen == showLen)
        {
          showLen = char_spacing;
          state = 2;
        }
        break;

      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++;
      // fall through

      case 3:  // display inter-character spacing or end of message padding (blank columns)
        mx.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;

      default:
        col = -1;   // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));

  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}


/* ================================================================================
  Capture battery data, send through WiFi
   ================================================================================
*/

void send_batterydata()
{
  Send_to_SignalK("electrical.batteries.main.voltage", ina219A.getBusVoltage_V());
  Send_to_SignalK("electrical.batteries.main.current", ina219A.getCurrent_mA());
  Send_to_SignalK("electrical.batteries.main.lifetimeDischarge", ina219A.getPower_mW());

  Send_to_SignalK("electrical.batteries.house.voltage", ina219B.getBusVoltage_V());
  Send_to_SignalK("electrical.batteries.house.current", ina219B.getCurrent_mA());
  Send_to_SignalK("electrical.batteries.house.lifetimeDischarge", ina219B.getPower_mW());
  // ;
  // ina219B.getShuntVoltage_mV();
}


/* ================================================================================
  Capture temperature, send through WiFi
   ================================================================================
*/

float ICACHE_RAM_ATTR get_temperaturedata()
{
  float temp0;
  temp_sensor.requestTemperatures();
  temp0 = temp_sensor.getTempCByIndex(0);
  temp0 = temp0 + 273.15; // conversion Centigrade -> Kelvin
  return temp0;
}


void send_temperaturedata()
{
  float temp0 = get_temperaturedata();
  Send_to_SignalK("propulsion.p0.temperature", temp0);

  DBG_print("Temp sensor 0: ");
  DBG_println(temp0);
}


/* ================================================================================
  Capture flood state from flood sensor
   ================================================================================
*/

void send_floodingmessage()
{
  bool flooded_state = !digitalRead(FLOAT_SWITCH_INPUT);

  if (flooded_state)
  {
    Send_to_SignalK_string("notifications.sinking", "\"SINKING\"");
  } else
  { // BUG: how to kill an alarm
//    Send_to_SignalK_null("notifications.sinking");
  }


#if debug
  if (flooded_state) Serial.println("ALARM: FLOODING");
#endif

}


/* ================================================================================
  Setup Wifi channel
   ================================================================================
*/

void setupWiFi() {
  
  // Setting up the wifi
  WiFi.disconnect();
  WiFi.config(staticIP, subnet, gateway, dns);
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, pwd);

  DBG_println();
  DBG_println();
  DBG_print("Connecting to ");
  DBG_println(ssid);
  DBG_print("Wait for WiFi... ");

  // Wait for association with the access point and connection to the network.
  // This will timeout into a watchdog reset after exhausted
  int nbr_wifi_tries = 0;
  while (WiFi.status() != WL_CONNECTED)  {
    DBG_print(".");
    nbr_wifi_tries++;
    if (nbr_wifi_tries > MAX_NBR_WIFI_TRIES)  while (1) {} ; // force WDT reset
    webota.delay(100);
  }
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  DBG_println("");
  DBG_println("WiFi connected");
  DBG_println("IP address: ");
  DBG_println(WiFi.localIP());

}


/* ================================================================================
  General setup
   ================================================================================
*/

void setup() {

  // Setup WDT for every 8 seconds
  wdt_enable(WDTO_8S);
  loopcounter = 0;
  
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level

  webota.delay(1000) ; // seems that the LCD MATRIX needs some time to power up

  Serial.begin(115200);
  while (!Serial) {
    webota.delay(1); // wait for serial port to connect. Needed for native USB port only
  }


  DBG_println("Setup ...");

  // Setup I2C
  Wire.begin(D2, D1);

  // Setup both Volt/Amp sensors
  ina219A.begin();
  ina219B.begin();
  ina219A.setCalibration_16V_160A();
  ina219B.setCalibration_16V_160A();

  // Dallas one-wire temperature sensor
  temp_sensor.begin();

  // Setup LED MATRIX display
  webota.delay(1000) ; // seems that the LCD MATRIX needs some time to power up
  mx.begin(); //LED matrix
  char message[12] = {" DBBC  v2.0"};
  printText(0, MAX_DEVICES - 1, message, 2);
  webota.delay(3000);
  printText(0, MAX_DEVICES - 1, message+6, 2);
  webota.delay(3000);

  // Setup input for float switch
  pinMode(FLOAT_SWITCH_INPUT, INPUT_PULLUP);     // Initialize the LED_BUILTIN pin as an output

  // RPM sensor
  // GPIO 3 (RX) swap the pin to a GPIO.
  pinMode(RPM_SENSOR_PIN, FUNCTION_3); 
  // Attach RPM sensor pin to rpm_ticker
  noInterrupts();
  attachInterrupt(RPM_SENSOR_PIN, rpm_ISR, FALLING);
  rpm_ticker.attach( 1.0, rpm_tick); // show RPM every second

  // Make sure to do this last in Setup(), or possible crash
  interrupts();

  setupWiFi();

}


/* ================================================================================
  Main loop

  - Assure Wifi is up
  - Get and send all data

  - RPM measurement and LED MATRIX display is done through ISR and ticker

   ================================================================================
*/

void loop() {

  DBG_print("Loop:");
  DBG_println(loopcounter);

  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  webota.delay(50); //short flash only

  // (re)connect if required
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  webota.handle(); // OTA update maybe?

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  webota.delay(950);

  send_batterydata();

  send_rpm();

  send_temperaturedata();

  send_floodingmessage();

  loopcounter++;

  // Keep watchdog at bay
  wdt_reset();

  if (loopcounter==MAXLOOPS) while (1) {} ; // force WDT reset after so many loops



}
