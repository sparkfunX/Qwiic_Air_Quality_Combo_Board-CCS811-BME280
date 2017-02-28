
/*
  CCS811 Air Quality Sensor Example Code
  By: Nathan Seidle
  SparkFun Electronics
  Date: February 7th, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Sends the humidity and temperature from a separate sensor to the CCS811 so
  that the CCS811 can adjust its algorithm.

  Hardware Connections (Breakoutboard to Arduino):
  3.3V = 3.3V
  GND = GND
  SDA = A4
  SCL = A5
  WAKE = D2

  Serial.print it out at 9600 baud to serial monitor.
*/

#include <Wire.h>
#include <SPI.h>
#include <SparkFunBME280.h>

//Register addresses
#define CSS811_STATUS 0x00
#define CSS811_MEAS_MODE 0x01
#define CSS811_ALG_RESULT_DATA 0x02
#define CSS811_RAW_DATA 0x03
#define CSS811_ENV_DATA 0x05
#define CSS811_NTC 0x06
#define CSS811_THRESHOLDS 0x10
#define CSS811_BASELINE 0x11
#define CSS811_HW_ID 0x20
#define CSS811_HW_VERSION 0x21
#define CSS811_FW_BOOT_VERSION 0x23
#define CSS811_FW_APP_VERSION 0x24
#define CSS811_ERROR_ID 0xE0
#define CSS811_APP_START 0xF4
#define CSS811_SW_RESET 0xFF

#define CCS811_ADDR 0x5B //7-bit unshifted default I2C Address
#define WAKE 2 //!Wake on breakout connected to pin 2 on Arduino

//These are the air quality values obtained from the sensor
unsigned int tVOC = 0;
unsigned int CO2 = 0;
float tempf = 0;
float humidity = 0;
float pressureInHg = 0;


  float BMEtempC = 0;
  float BMEhumid = 0;

  
//Global sensor object
BME280 mySensor;
//---------------------------------------------------------------
void setup()
{
  Serial.begin(9600);
  Serial.println("CCS811 Read Example");

  pinMode(WAKE, OUTPUT);
  digitalWrite(WAKE, LOW);

  Wire.begin();

  configureCCS811(); //Turn on sensor

  //For I2C, enable the following and disable the SPI section
  mySensor.settings.commInterface = I2C_MODE;
  mySensor.settings.I2CAddress = 0x77;

    //Initialize BME280
    //For I2C, enable the following and disable the SPI section
    mySensor.settings.commInterface = I2C_MODE;
    mySensor.settings.I2CAddress = 0x77;
    mySensor.settings.runMode = 3; //Normal mode
    mySensor.settings.tStandby = 0;
    mySensor.settings.filter = 4;
    mySensor.settings.tempOverSample = 5;
    mySensor.settings.pressOverSample = 5;
    mySensor.settings.humidOverSample = 5;
    
    //Calling .begin() causes the settings to be loaded
    delay(10);  //Make sure sensor had enough time to turn on. BME280 requires 2ms to start up.
    mySensor.begin();

  unsigned int result = getBaseline();

  Serial.print("baseline for this sensor: 0x");
  if(result < 0x100) Serial.print("0");
  if(result < 0x10) Serial.print("0");
  Serial.println(result, HEX);
  Serial.println();

  Serial.print("Envoronmental Register Initial Reading:");
  checkEnvReg();
}
//---------------------------------------------------------------
void loop()
{
  if (dataAvailable())
  {
    readAlgorithmResults(); //Calling this function updates the global tVOC and CO2 variables
    printInfoSerial();

  BMEtempC = mySensor.readTempC();
  BMEhumid = mySensor.readFloatHumidity();

  Serial.print("BME Readings:");
  Serial.print(BMEtempC);
  Serial.print(",");
  Serial.println(BMEhumid);


    setEnvironmentalData(BMEhumid, BMEtempC);
    checkEnvReg();
  }
  else if (checkForError())
  {
    printError();
  }

  delay(2000); //Wait for next reading
}
//---------------------------------------------------------------
void checkEnvReg()
{


  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_ENV_DATA);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 4); //Get four bytes

  byte tempMSB = Wire.read();
  byte tempLSB = Wire.read();
  byte humMSB = Wire.read();
  byte humLSB = Wire.read();

  unsigned int temp = ((unsigned int)tempMSB << 8) | tempLSB;
  unsigned int humid = ((unsigned int)humMSB << 8) | humLSB;

  Serial.print("TempByte: ");
  Serial.print(temp, HEX);
  Serial.print("  HumidityByte: ");
  Serial.println(humid, HEX);
  Serial.println();
}
//---------------------------------------------------------------
void printInfoSerial()
{

  Serial.print("CO2 concentration : "); 
  Serial.print(CO2);
  Serial.println(" ppm");

  Serial.print("TVOC concentration : "); 
  Serial.print(tVOC);
  Serial.println(" ppb");

  Serial.print("Temperature: ");
  Serial.print(mySensor.readTempC(), 2);
  Serial.println(" degrees C");

  Serial.print("Temperature: ");
  Serial.print(mySensor.readTempF(), 2);
  Serial.println(" degrees F");

  Serial.print("Pressure: ");
  Serial.print(mySensor.readFloatPressure(), 2);
  Serial.println(" Pa");
  
  Serial.print("Pressure: ");
  Serial.print((mySensor.readFloatPressure() * 0.0002953), 2);
  Serial.println(" InHg");

  Serial.print("Altitude: ");
  Serial.print(mySensor.readFloatAltitudeMeters(), 2);
  Serial.println("m");

  Serial.print("Altitude: ");
  Serial.print(mySensor.readFloatAltitudeFeet(), 2);
  Serial.println("ft");

  Serial.print("%RH: ");
  Serial.print(mySensor.readFloatHumidity(), 2);
  Serial.println(" %");

  Serial.println();


}
//---------------------------------------------------------------
//Updates the total voltatile organic compounds (TVOC) in parts per billion (PPB)
//and the CO2 value
//Returns nothing
void readAlgorithmResults()
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_ALG_RESULT_DATA);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 4); //Get four bytes

  byte co2MSB = Wire.read();
  byte co2LSB = Wire.read();
  byte tvocMSB = Wire.read();
  byte tvocLSB = Wire.read();

  CO2 = ((unsigned int)co2MSB << 8) | co2LSB;
  tVOC = ((unsigned int)tvocMSB << 8) | tvocLSB;
}
//---------------------------------------------------------------
//Turns on the sensor and configures it with default settings
void configureCCS811()
{
  //Verify the hardware ID is what we expect
  byte hwID = readRegister(0x20); //Hardware ID should be 0x81
  if (hwID != 0x81)
  {
    Serial.println("CCS811 not found. Please check wiring.");
    while (1); //Freeze!
  }

  //Check for errors
  if (checkForError() == true)
  {
    Serial.println("Error at Startup");
    printError();
    while (1); //Freeze!
  }

  //Tell App to Start
  if (appValid() == false)
  {
    Serial.println("Error: App not valid.");
    while (1); //Freeze!
  }

  //Write to this register to start app
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_APP_START);
  Wire.endTransmission();

  //Check for errors
  if (checkForError() == true)
  {
    Serial.println("Error at AppStart");
    printError();
    while (1); //Freeze!
  }

  //Set Drive Mode
  setDriveMode(1); //Read every second

  //Check for errors
  if (checkForError() == true)
  {
    Serial.println("Error at setDriveMode");
    printError();
    while (1); //Freeze!
  }
}
//---------------------------------------------------------------
//Checks to see if error bit is set
boolean checkForError()
{
  byte value = readRegister(CSS811_STATUS);
  return (value & 1 << 0);
}
//---------------------------------------------------------------
//Displays the type of error
//Calling this causes reading the contents of the ERROR register
//This should clear the ERROR_ID register
void printError()
{
  byte error = readRegister(CSS811_ERROR_ID);

  Serial.print("Error: ");
  if (error & 1 << 5) Serial.print("HeaterSupply ");
  if (error & 1 << 4) Serial.print("HeaterFault ");
  if (error & 1 << 3) Serial.print("MaxResistance ");
  if (error & 1 << 2) Serial.print("MeasModeInvalid ");
  if (error & 1 << 1) Serial.print("ReadRegInvalid ");
  if (error & 1 << 0) Serial.print("MsgInvalid ");

  Serial.println();
}
//---------------------------------------------------------------
//Returns the baseline value
//Used for telling sensor what 'clean' air is
//You must put the sensor in clean air and record this value
unsigned int getBaseline()
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_BASELINE);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 2); //Get two bytes

  byte baselineMSB = Wire.read();
  byte baselineLSB = Wire.read();

  unsigned int baseline = ((unsigned int)baselineMSB << 8) | baselineLSB;

  Serial.print("Baseline: ");
  Serial.println(baseline);

  return (baseline);
}
//---------------------------------------------------------------
//Checks to see if DATA_READ flag is set in the status register
boolean dataAvailable()
{
  byte value = readRegister(CSS811_STATUS);
  return (value & 1 << 3);
}
//---------------------------------------------------------------
//Checks to see if APP_VALID flag is set in the status register
boolean appValid()
{
  byte value = readRegister(CSS811_STATUS);
  return (value & 1 << 4);
}
//---------------------------------------------------------------
//Enable the nINT signal
void enableInterrupts(void)
{
  byte setting = readRegister(CSS811_MEAS_MODE); //Read what's currently there
  setting |= (1 << 3); //Set INTERRUPT bit
  writeRegister(CSS811_MEAS_MODE, setting);
}
//---------------------------------------------------------------
//Disable the nINT signal
void disableInterrupts(void)
{
  byte setting = readRegister(CSS811_MEAS_MODE); //Read what's currently there
  setting &= ~(1 << 3); //Clear INTERRUPT bit
  writeRegister(CSS811_MEAS_MODE, setting);
}
//---------------------------------------------------------------
//Mode 0 = Idle
//Mode 1 = read every 1s
//Mode 2 = every 10s
//Mode 3 = every 60s
//Mode 4 = RAW mode
void setDriveMode(byte mode)
{
  if (mode > 4) mode = 4; //Error correction

  byte setting = readRegister(CSS811_MEAS_MODE); //Read what's currently there

  setting &= ~(0b00000111 << 4); //Clear DRIVE_MODE bits
  setting |= (mode << 4); //Mask in mode
  writeRegister(CSS811_MEAS_MODE, setting);
}
     /*
//---------------------------------------------------------------
//Given a temp and humidity, write this data to the CSS811 for better compensation
//This function expects the humidity and temp to come in as floats
void setEnvironmentalData(float relativeHumidity, float temperature)
{
  int rH = relativeHumidity * 1000; //42.348 becomes 42348
  int temp = temperature * 1000; //23.2 becomes 23200

  byte envData[4];

  //Split value into 7-bit integer and 9-bit fractional
  envData[0] = ((rH % 1000) / 100) > 7 ? (rH / 1000 + 1) << 1 : (rH / 1000) << 1;
  envData[1] = 0; //CCS811 only supports increments of 0.5 so bits 7-0 will always be zero
  if (((rH % 1000) / 100) > 2 && (((rH % 1000) / 100) < 8))
  {
    envData[0] |= 1; //Set 9th bit of fractional to indicate 0.5%
  }

  temp += 25000; //Add the 25C offset
  //Split value into 7-bit integer and 9-bit fractional
  envData[2] = ((temp % 1000) / 100) > 7 ? (temp / 1000 + 1) << 1 : (temp / 1000) << 1;
  envData[3] = 0;
  if (((temp % 1000) / 100) > 2 && (((temp % 1000) / 100) < 8))
  {
    envData[2] |= 1;  //Set 9th bit of fractional to indicate 0.5C
  }
  Serial.println("Env_Date_read:");
  Serial.println(envData[0], HEX);
  Serial.println(envData[1], HEX);
  Serial.println(envData[2], HEX);
  Serial.println(envData[03], HEX);

  
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_ENV_DATA); //We want to write our RH and temp data to the ENV register
  Wire.write(envData[0]);
  Wire.write(envData[1]);
  Wire.write(envData[2]);
  Wire.write(envData[3]);
}
   

*/
void setEnvironmentalData(float t, float rh)    // compensate for temperature and relative humidity
{
  digitalWrite(WAKE, LOW);

  int _temp, _rh;
  if(t>0)
    _temp = (int)t + 0.5;  // this will round off the floating point to the nearest integer value
  else if(t<0)
    _temp = (int)t - 0.5;
  _temp = _temp + 25;  // temperature high byte is stored as T+25°C so the value of byte is positive
  _rh = (int)rh + 0.5;  // this will round off the floating point to the nearest integer value

  Serial.println("Env_Date_read:");
  Serial.println(_rh, HEX);
  Serial.println(_temp, HEX);



  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(CSS811_ENV_DATA);
  Wire.write(_rh);           // 7 bit humidity value
  Wire.write(0);            // most significant fractional bit. Using 0 here - gives us accuracy of +/-1%. Current firmware (2016) only supports fractional increments of 0.5
  Wire.write(_temp);
  Wire.write(0);
  Wire.endTransmission();

  digitalWrite(WAKE, HIGH);
}


//---------------------------------------------------------------
//Reads from a give location from the CSS811
byte readRegister(byte addr)
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(addr);
  Wire.endTransmission();

  Wire.requestFrom(CCS811_ADDR, 1);

  return (Wire.read());
}
//---------------------------------------------------------------
//Write a value to a spot in the CCS811
void writeRegister(byte addr, byte val)
{
  Wire.beginTransmission(CCS811_ADDR);
  Wire.write(addr);
  Wire.write(val);
  Wire.endTransmission();
}

