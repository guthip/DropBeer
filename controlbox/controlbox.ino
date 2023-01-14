
/* Manually set the correct IDE parameters...
 *
 *  Arduino IDE Parameters:
 *  #ide_param:board=Arduino Nano
 *  #ide_param:processor=ATmega328P
 *  #ide_param:port=ttyUSB1 (or whatever ttyOP_ard points to)
 */

#include <LiquidCrystal.h>

/*Written by Hans Spanjaart, 2017, http://lat13.com .  info@lat13.com

  Drop Beer Board Computer Control Unit

 */

#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include "controlbox.h"

void TaskState( void *pvParameters);
void TaskVbat( void *pvParameters);
void TaskReceive( void *pvParameters);
void TaskRpiStatus( void *pvParameters);

const char *ANALOG_IN0 = "$AI0:";
const char *DEV_STATUS = "$STT:";

//*****************************************
// Buttons/switches
//*****************************************


//*****************************************
// initialize the library with the numbers of the interface pins for LED char 16x2
//*****************************************
LiquidCrystal lcd(LCD_CONNECTIONS);

//*****************************************
// Menu state machine
//*****************************************
enum DeviceStateType {
	logo = 0, bootup = 10, active = 20, shuttingdown = 30
};

DeviceStateType deviceState;

//*****************************************
// Global variable
//*****************************************
SemaphoreHandle_t xSemaRpiActive; // msg indicating $MSG:ACT has just been received
SemaphoreHandle_t xSemaSerialUse; // exclusive use of serial device (USB)
volatile bool RPI_active; // true if RPI has sent $MSG:ACT recently
volatile float vbat; // average battery voltage over 4 samples
volatile int timeout=0; // check if activity has been detecetd on RPi


//*****************************************
// Init function
//*****************************************
void setup() {
	Serial.begin(9600);
	while (!Serial) {};

	// Init values
	deviceState = logo;

	pinMode(PiPwr, OUTPUT);
	digitalWrite(PiPwr, 0); // high level powers RPI

	pinMode(menuSwitch, INPUT_PULLUP);

	// set up the LCD's number of columns and rows:
	lcd.begin(16, 2);
	// allow big characters as well
	// note limited space on 16x2 LCD
	pinMode(11, OUTPUT); // Set Vo - contrast
	analogWrite(11, CONTRAST); //maximum contrast

	lcd.print("Drop Beer");
	lcd.setCursor(0, 1);
	lcd.print("computer V2.0");

	xSemaRpiActive = xSemaphoreCreateBinary();
	if ( xSemaRpiActive == NULL ) while (1); // Hang if sem space unavailable

	xSemaSerialUse = xSemaphoreCreateMutex();
	if ( xSemaSerialUse == NULL ) while (1); // Hang if sem space unavailable

	xTaskCreate(
			TaskState
			,  "DeviceState"   // A name just for humans
			,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
			,  NULL
			,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
			,  NULL );

	xTaskCreate(
			TaskVbat
			,  "Vbat measure"   // A name just for humans
			,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
			,  NULL
			,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
			,  NULL );

	xTaskCreate(
			TaskReceive
			,  "Display"   // A name just for humans
			,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
			,  NULL
			,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
			,  NULL );

	xTaskCreate(
			TaskRpiStatus
			,  "RpiStatus"   // A name just for humans
			,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
			,  NULL
			,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
			,  NULL );
}


//*****************************************
// Loop function
//*****************************************
void loop()
{
	// Empty. Things are done in Tasks.
}


//*****************************************
// Receive message  from host
//*****************************************
int readline(int readch, char *buffer, int len)
{
	static int pos = 0;
	int rpos;

	if (readch > 0) {
		switch (readch) {
		case '\n': // Ignore new-lines
			break;
		case '\r': // Return on CR
			rpos = pos;
			pos = 0;  // Reset position index ready for next time
			return rpos;
		default:
			if (pos < len - 1) {
				buffer[pos++] = readch;
				buffer[pos] = 0;
			}
		}
	}
	// No end of line has been found, so return -1.
	return -1;
}


void TaskReceive( void *pvParameters)
{
	(void) pvParameters;

	for (;;)
	{
		char datafrompi[32];
		if (readline(Serial.read(), datafrompi, 32) > 0) {

			if (strncmp(datafrompi, "$CLR", 4) == 0)
			{
				lcd.clear();
			}

			if (strncmp(datafrompi, "$PRN", 4) == 0)
			{
				lcd.print(datafrompi + 5);
			}

			if (strncmp(datafrompi, "$LOC", 4) == 0)
			{
				int x = datafrompi[5] - '0';
				int y = datafrompi[7] - '0';
				if ((x < 16) && (y < 3))
					lcd.setCursor( x , y );
			}

			if (strncmp(datafrompi, "$MSG", 4) == 0)
			{ // RPI state = active
				if (strncmp(datafrompi + 5, "ACT", 3) == 0)
				{
					xSemaphoreGive( xSemaRpiActive );
				};
				// RPI state = shutting down
				if (strncmp(datafrompi + 5, "SHT", 3) == 0)
				{
				};
			}

			vTaskDelay(1);
		}
	}
}

//*****************************************
// Check if RPI is alive
// Assume RPI is dead when $MSG:ACT has not been received for XX seconds
//*****************************************
void TaskRpiStatus( void *pvParameters)
{
	(void) pvParameters;

	for (;;)
	{
		if ( xSemaphoreTake( xSemaRpiActive, ( TickType_t ) 10 ) == pdTRUE )
		{
			RPI_active = 1; // RPI is active - let everyone know
			timeout = 10; // and RPi has this many time units to keep alive
		}

		if (timeout > 0)
		{
			timeout--;
			// Show count down bar
			if (timeout < 6)
			{
				lcd.setCursor(0, 0);
				lcd.print("Shutting down");
				lcd.setCursor(0, 1);
				int i;
				for ( i = 0 ; i < timeout ; i++) lcd.print("*");
				for ( i = timeout ; i < 16 ; i++) lcd.print(" ");
			}
			if (timeout == 0)
			{
				RPI_active = 0;
			}
		}

		vTaskDelay(100);

	}
}


//*****************************************
// Check Vbat against threshold
//*****************************************
boolean VbatHighEnough()
{
	return (vbat > VBAT_MINIMUM);
}

//*****************************************
// Measure Vbat (house battery voltage) periodically
//*****************************************
void TaskVbat( void *pvParameters)
{
	(void) pvParameters;

	for (;;)
	{
		float x = 0;
		for ( int i = 0 ; i < 4 ; i++ )
		{
			x = x + analogRead( Vbat_in) * 3.54 / 260;
			vTaskDelay(10);
		}

		// average out
		x = x / 4;

		if ( xSemaphoreTake( xSemaSerialUse, ( TickType_t ) 10 ) == pdTRUE )
		{
			Serial.print(ANALOG_IN0);
			Serial.println(x);
			xSemaphoreGive( xSemaSerialUse );
		}

		vbat = x; //global

		vTaskDelay(500);
	}
}


//*****************************************
// Advance through deviceState depending on menuButton
//*****************************************
void shareStatus(const char* status)
{
	if ( xSemaphoreTake( xSemaSerialUse, ( TickType_t ) 10 ) == pdTRUE )
	{
		Serial.print(DEV_STATUS);
		Serial.println(status);
		xSemaphoreGive( xSemaSerialUse );
	}
}


void TaskState( void *pvParameters)
{
	(void) pvParameters;

	xSemaphoreGive( xSemaSerialUse ); // uart available as default

	for (;;)
	{
		switch (deviceState) {
		//*****************************************
		case logo:
		{
			if (!digitalRead(menuSwitch))
			{
				lcd.clear();
				lcd.print("Switch to start");
			}
			else if (VbatHighEnough())
			{
				digitalWrite(PiPwr, 1);
				deviceState = bootup;
				lcd.clear();
				lcd.print("Starting up");
				shareStatus("STA");
				vTaskDelay(400);
			}
			else
			{
				lcd.clear();
				lcd.print("BATTERY LOW !!!");
				lcd.setCursor(0, 1);
				lcd.print(vbat);
			}
		}
		vTaskDelay(100);
		break;

		//*****************************************
		case bootup:
			if (!digitalRead(menuSwitch)) // user changes his mind during startup
			{
				lcd.clear();
				lcd.print("Emergency");
				lcd.setCursor(0, 1);
				lcd.print("shutdown");
				shareStatus("STP");
				vTaskDelay(1000); // Give Rpi time to either boot or stay dead
				deviceState = shuttingdown;
			}
			else if (RPI_active)
			{
				deviceState = active;
				lcd.clear();;
				lcd.print("Active");
				shareStatus("ACT");
				digitalWrite(PiPwr, 1);
			}
			break;

			//*****************************************
		case active:
			//        if ((RPI_active == 0) || scanButton() || !VbatHighEnough())
			if ((RPI_active == 0) || !digitalRead(menuSwitch) || !VbatHighEnough())
			{
				deviceState = shuttingdown;
				lcd.clear();;
				lcd.print("Shutting down");
				// Try multiple times to tell rpi to shut down, in case it is hard of hearing
				for ( int i = 0; i < 5; i++ )
				{
					shareStatus("STP");
					vTaskDelay(10);
				}
				digitalWrite(PiPwr, 1);
				vTaskDelay(100);
			}
			break;

			//*****************************************
		case shuttingdown:
			if (RPI_active == 0) {
				deviceState = logo;
				shareStatus("NUL");
				lcd.clear();
				lcd.print("Powered down");
				vTaskDelay(200);
				digitalWrite(PiPwr, 0);
				vTaskDelay(100); // you should really never come past this point, unless Vbat is up again/still
			}
			break;

			//*****************************************
		default:
			break;
		}
	}
}
