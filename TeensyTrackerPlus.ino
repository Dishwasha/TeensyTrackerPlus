#include "TeensyTrackerPlus.h"

void setupAPRS() {
  aprs_setup(50, // number of preamble flags to send
       0, // Use PTT pin
       0, // ms to wait after PTT to transmit
       0, 0 // No VOX ton
       );
}

void setupLSM9DS1() {
  lsm9ds1 = new LSM9DS1(I2C_PINS_16_17, I2C_PULLUP_INT, I2C_RATE_400);
  delay(5000);
  
  // Set up the interrupt pin, its set as active high, push-pull
  pinMode(lsmIntPin, INPUT);
  lsm9ds1->init();
}

void setupGPS() {
  gps.startSerial(9600);
  delay(1000);
  gps.setSentencesToReceive(OUTPUT_RMC_GGA);
}

void setupSPIFlash() {
  unsigned char id_tab[32];
  SPI.setMOSI(11);
  SPI.setMISO(12);
  SPI.setSCK(13);
  flash_init();
  flash_hard_reset();
  Serial.print("Status = ");
  Serial.println(flash_read_status(),HEX);
  flash_read_id(id_tab);
  for(int i = 0;i < 4;i++) {
    Serial.printf("0x%X ", id_tab[i]);
  }
  Serial.println("\r\n");
}

void setupVEML6070() {
  // Provide alternative power pins to VEML6070
  pinMode(7, OUTPUT);
  pinMode(20, OUTPUT);
  digitalWrite(7, LOW);
  digitalWrite(20, HIGH);

  veml6070 = new VEML6070(I2C_PINS_18_19, I2C_PULLUP_INT, I2C_RATE_400);
  veml6070->init();
}

void aprsCallback() {
  String str = payload_string();
  unsigned int len = str.length();
  char* cstr = new char[len+1];
  str.toCharArray(cstr, len+1);
  
  // If above 5000 feet switch to a single hop path
  int nAddresses;
  if (data.gps_data.altitude > 1500) {
    // APRS recomendations for > 5000 feet is:
    // Path: WIDE2-1 is acceptable, but no path is preferred.
    nAddresses = 3;
    addresses[2].callsign = "WIDE2";
    addresses[2].ssid = 1;
  } else {
    // Below 1500 meters use a much more generous path (assuming a mobile station)
    // Path is "WIDE1-1,WIDE2-2"
    nAddresses = 4;
    addresses[2].callsign = "WIDE1";
    addresses[2].ssid = 1;
    addresses[3].callsign = "WIDE2";
    addresses[3].ssid = 2;
  }

  // For debugging print out the path
  Serial.print("APRS(");
  Serial.print(nAddresses);
  Serial.print("): ");
  for (int i=0; i < nAddresses; i++) {
    Serial.print(addresses[i].callsign);
    Serial.print('-');
    Serial.print(addresses[i].ssid);
    if (i < nAddresses-1)
      Serial.print(',');
  }
  Serial.print(' ');
  Serial.print(SYMBOL_TABLE);
  Serial.print(SYMBOL_CHAR);
  Serial.println();
  Serial.print(data.gps_data.day);
  Serial.print (" ");
  Serial.print(data.gps_data.hour);
  Serial.print (" ");
  Serial.print(data.gps_data.minute);
  Serial.print (" ");
  Serial.print(data.gps_data.latitude);
  Serial.print (" ");
  Serial.print(data.gps_data.longitude);
  Serial.print (" ");
  Serial.print(data.gps_data.altitude);
  Serial.print (" ");
  Serial.print(data.gps_data.heading);
  Serial.print (" ");
  Serial.print(data.gps_data.speed);
  Serial.print (" ");
  Serial.println();

  // Send the packet
  digitalWrite(3, HIGH);
  delay(500);
  aprs_send(addresses, nAddresses,
      data.gps_data.day, data.gps_data.hour, data.gps_data.minute,
      data.gps_data.latitude, data.gps_data.longitude, // degrees
      data.gps_data.altitude, // meters
      data.gps_data.heading,
      data.gps_data.speed,
      SYMBOL_TABLE,
      SYMBOL_CHAR,
      cstr
  );
  digitalWrite(3, LOW);
  //delete cstr;
}

void captureCallback() {
  if(capturing) return;
  capturing = true;
  data.lsm9ds1_data = lsm9ds1->capture();
  Serial.print("Captured LSM9DS1");
  data.gps_data = captureGPS();
  data.veml6070_data = veml6070->getUVdata();
  spiflash->writeBytes((unsigned char*)&data, sizeof(data.lsm9ds1_data) + sizeof(data.gps_data) + sizeof(data.veml6070_data));
  capturing = false;
}

GPS_data captureGPS() {
  GPS_data gps_data = {1,1,1,1,1,1,1.0,1.0,1.0,1,1.0};

  if (gps.sentenceAvailable()) gps.parseSentence();

  gps.dataRead();

  gps_data = (GPS_data){
    gps.hour,gps.minute,gps.seconds,
    gps.year,gps.month,gps.day,
    gps.latitude,gps.longitude,gps.altitude,
    gps.heading,gps.speed
  };

  return gps_data;
}

uint16_t captureVEML6070() {
  Serial.print("Captured VEML6070");
  uint16_t steps = veml6070->getUVdata();
  
  Serial.print("UV raw counts = "); Serial.println(steps);
  Serial.print("UV radiant power = "); Serial.print((float)steps*5.0); Serial.println(" microWatts/cm*cm");

  uint8_t risk_level = veml6070->convert_to_risk_level(steps);
  if(risk_level == 0) Serial.println("UV risk level is low"); 
  if(risk_level == 1) Serial.println("UV risk level is moderate"); 
  if(risk_level == 2) Serial.println("UV risk level is high"); 
  if(risk_level == 3) Serial.println("UV risk level is very high"); 
  if(risk_level == 4) Serial.println("UV risk level is extreme");
  
  return steps; 
}

String payload_string() {
  String str = "HELLOTHEREIAMETHAN";
  return str;
}

void setup() {
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);
  setupGPS();
  setupLSM9DS1();
  setupVEML6070();
  delay(10000);
  setupSPIFlash();
  setupAPRS();
}

uint32_t timeOfAPRS = 0;

void loop() {
  if (timeOfAPRS + 1000 < millis()) {
    captureCallback();
  }
  if (timeOfAPRS + 10000 < millis()) {
    aprsCallback();
    timeOfAPRS = millis();
  }
}

