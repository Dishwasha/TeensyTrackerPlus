#include <WProgram.h>
#include <GPS.h>
#include <aprs.h>
#include <LSM9DS1.h>
#include <SPIFlash.h>
#include <VEML6070.h>

LSM9DS1* lsm9ds1;
VEML6070* veml6070;
SPIFlash* spiflash;

struct GPS_data {
  uint8_t hour, minute, seconds, year, month, day;
  float latitude, longitude; // degrees
  float altitude; // meters
  uint16_t heading;
  float speed;
};

struct Data {
  LSM9DS1_data lsm9ds1_data;
  GPS_data gps_data;
  uint16_t veml6070_data;
};

// Set your callsign and SSID here. Common values for the SSID are
#define S_CALLSIGN      "W3HAC"
#define S_CALLSIGN_ID   11   // 11 is usually for balloons
// Destination callsign: APRS (with SSID=0) is usually okay.
#define D_CALLSIGN      "APRS"
#define D_CALLSIGN_ID   0
// Symbol Table: '/' is primary table '\' is secondary table
#define SYMBOL_TABLE '/' 
// Primary Table Symbols: /O=balloon, /-=House, /v=Blue Van, />=Red Car
#define SYMBOL_CHAR 'O'

struct PathAddress addresses[] = {
  {(char *)D_CALLSIGN, D_CALLSIGN_ID},  // Destination callsign
  {(char *)S_CALLSIGN, S_CALLSIGN_ID},  // Source callsign
  {(char *)NULL, 0}, // Digi1 (first digi in the chain)
  {(char *)NULL, 0}  // Digi2 (second digi in the chain)
};

HardwareSerial &gpsSerial = Serial1;
GPS gps(&gpsSerial,true);

Data data;
int lsmIntPin = 15;
int myLed  = 13;
IntervalTimer captureTimer;
int aprsInterval = 10;
int aprsCounter = 0;
int captureTimerInMicroseconds = 1000000;
bool capturing = false;
