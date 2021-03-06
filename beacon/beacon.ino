// Adafruit Ultimate GPS module using MTK33x9 chipset
// http://www.adafruit.com/products/746
// Arduino Leonardo

#include <limits.h>
#include <TinyGPS.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <inttypes.h>
#include <LCDi2cNHD.h>

#include "stations.h"
#include "beacon.h"

// RxD, TxD
SoftwareSerial gps_serial(GPSRxD, GPSTxD);
TinyGPS gps;
// rows, columns, I2C address, display (not use, set to zero)
LCDi2cNHD fp_lcd = LCDi2cNHD(2,16,0x50>>1,0);

#define GPSECHO false
// turn on only the second sentence (GPRMC)
#define PMTK_SET_NMEA_OUTPUT_RMCONLY F("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29")
#define PMTK_SET_NMEA_UPDATE_1HZ  F("$PMTK220,1000*1F")
#define PMTK_API_SET_FIX_CTL_1HZ  F("$PMTK300,1000,0,0,0,0*1C")

volatile boolean ticked = false;

// wall_ticks counts seconds and goes from 0-180 because there is a 3-minute beacon schedule.
volatile byte wall_ticks = 0;

// Set this to the click second at which you want the tick() interrupt handler to key the transmitter.
volatile byte next_tx_click = 255;

// Set this to true if the tick() interrupt handler to measure update millis_per_second to the most recent value.
// It should never be on if there is a chance that other interrupts will interfere or if interrupts are disabled, such as during SoftwareSerial output (input is OK).
volatile boolean disciplining_milliclock = false;

// The disciplining value for the milliclock, which really means measuring it and recording millis_per_second.
volatile int millis_per_second = 0;

// Outside the interrupt, when disciplining the milliclock, update this with the value needed to achieve 750ms.  It starts at 750 and disciplining the milliclock sets this number.
int seven_fifty_millis = 750;

volatile int last_millis = 0;

// set on id send, reset on band change.  prevents starting up in the middle of the schedule.
volatile boolean id_sent = false;

// tick interrupt
// keep track of wall_ticks; use GPS PPS to discipline millis_per_second when it's safe to do so (no interrupts masked).
void tick() {
  wall_ticks = (wall_ticks+1) % (3*60);
  if ((wall_ticks - station.start_time) == next_tx_click) {
    txon();
  }

  if (disciplining_milliclock) {
    int t = last_millis;
    last_millis = millis();
    millis_per_second = millis() - t;
  }

  ticked = true;
}

void setup()  {

	// Kill radio TX as soon as we wake up
	pinMode(LED, OUTPUT);	// TX on LED
	pinMode(PTTLINE, OUTPUT);
	txoff();

	// FP LCD set up
	pinMode(BLBLUE , OUTPUT);
	pinMode(BLGREEN, OUTPUT);
        pinMode(BLRED,   OUTPUT);   
	
	FPBLBLUE
	fp_lcd.init();
        fp_lcd.cursor_off();
        FPPRINTRC(0,0,"V2.3      ");
        FPPRINTRC(0,7,station.call);
        FPPRINTRC(1,0,"QRX Serial CNSOL");

  // Serial debug output to desktop computer.  For product, send to LCD.
  {
    Serial.begin(115200);

    // Leonardo requires this.
    while (!Serial) {
    }
  }

  Serial.println(F("NCDXC/IARU Beacon IBPV2.3"));
  Serial.println(station.call);

  FPPRINTRC(1,0,"QRX INIT      ")
  
  // PPS interrupt from GPS on pin 3 (Int.0) on Arduino Leonardo
  pinMode(3, INPUT_PULLUP);         // PPS is 2.8V so give it pullup help
  attachInterrupt(digitalPinToInterrupt(PPS), tick, RISING); // tick happens 0.5s after GPS serial sends the time.


  // GPS Setup 
  {
    gps_serial.begin(9600);
    gps_serial.println(PMTK_SET_NMEA_OUTPUT_RMCONLY);
    gps_serial.println(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz NMEA sentence rate
    gps_serial.println(PMTK_API_SET_FIX_CTL_1HZ);   // 1 Hz fix rate

    delay(1000);
  }

  FPPRINTRC(1,0,"QRX INIT GPS DO")
  do {
    Serial.println(F("*** GPS Discipline clock"));
  } while (! gps_discipline_clock(LONG_MAX));
  Serial.println(F("*** GPS Locked "));

  Serial.println(F("*** GPS Discipline Milliclock"));
  {
    gps_begin_milliclock_discipline(); 
    delay(5000);
    gps_end_milliclock_discipline();
  }
  Serial.println(F("*** Milliclock disciplined "));
  
  FPPRINTRC(1,0,"QRX INIT RADIO")
  Serial.println(F("Radio init"));
  radioSetup();
  CWSetup();
  
  FPPRINTRC(1,0,"               ");
  FPPRINTRC(1,0,"OPER");
  FPBLGREEN

}

void loop()
{
  boolean tocked = false;
  if (ticked) {
    ticked = false;
    tocked = true;
  }

  if (tocked) {
    handle_tick();
  }

}



