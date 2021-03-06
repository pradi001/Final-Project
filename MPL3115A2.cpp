#include <Wire.h>

#include "MPL3115A2.h"

MPL3115A2::MPL3115A2()
{
  //Set initial values for private vars
}

//Start I2C communication
bool MPL3115A2::begin(void)
{
  Wire.begin();
}


//Returns the number of meters above sea level
//Returns -1 if no new data is available
float MPL3115A2::readAltitude()
{
	toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

	//Wait for PDR bit, indicates we have new pressure data
	int counter = 0;
	while( (IIC_Read(STATUS) & (1<<1)) == 0)
	{
		if(++counter > 600) return(-999); //Error out after max of 512ms for a read
		delay(1);
	}

	// Read pressure registers
	Wire.beginTransmission(MPL3115A2_ADDRESS);
	Wire.write(OUT_P_MSB);  // Address of data to get
	Wire.endTransmission(false); 
	Wire.requestFrom(MPL3115A2_ADDRESS, 3); // Request three bytes

	//Wait for data to become available
	counter = 0;
	while(Wire.available() < 3)
	{
		if(counter++ > 100) return(-999); //Error out
		delay(1);
	}

	byte msb, csb, lsb;
	msb = Wire.read();
	csb = Wire.read();
	lsb = Wire.read();

	// The least significant bytes l_altitude and l_temp are 4-bit,
	// fractional values, so you must cast the calulation in (float),
	// shift the value over 4 spots to the right and divide by 16 (since 
	// there are 16 values in 4-bits). 
	float tempcsb = (lsb>>4)/16.0;

	float altitude = (float)( (msb << 8) | csb) + tempcsb;

	return(altitude);
}

//Returns the number of feet above sea level
float MPL3115A2::readAltitudeFt()
{
  return(readAltitude() * 3.28084);
}

//Reads the current pressure in Pa
//Unit must be set in barometric pressure mode
//Returns -1 if no new data is available
float MPL3115A2::readPressure()
{
	//Check PDR bit, if it's not set then toggle OST
	if(IIC_Read(STATUS) & (1<<2) == 0) toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

	//Wait for PDR bit, indicates we have new pressure data
	int counter = 0;
	while(IIC_Read(STATUS) & (1<<2) == 0)
	{
		if(++counter > 600) return(-999); //Error out after max of 512ms for a read
		delay(1);
	}

	// Read pressure registers
	Wire.beginTransmission(MPL3115A2_ADDRESS);
	Wire.write(OUT_P_MSB);  // Address of data to get
	Wire.endTransmission(false); // Send data to I2C dev with option for a repeated start. THIS IS NECESSARY and not supported before Arduino V1.0.1!
	Wire.requestFrom(MPL3115A2_ADDRESS, 3); // Request three bytes

	//Wait for data to become available
	counter = 0;
	while(Wire.available() < 3)
	{
		if(counter++ > 100) return(-999); //Error out
		delay(1);
	}

	byte msb, csb, lsb;
	msb = Wire.read();
	csb = Wire.read();
	lsb = Wire.read();
	
	toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

	// Pressure comes back as a left shifted 20 bit number
	long pressure_whole = (long)msb<<16 | (long)csb<<8 | (long)lsb;
	pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.

	lsb &= 0b00110000; //Bits 5/4 represent the fractional component
	lsb >>= 4; //Get it right aligned
	float pressure_decimal = (float)lsb/4.0; //Turn it into fraction

	float pressure = (float)pressure_whole + pressure_decimal;

	return(pressure);
}

float MPL3115A2::readTemp()
{
	if(IIC_Read(STATUS) & (1<<1) == 0) toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

	//Wait for TDR bit, indicates we have new temp data
	int counter = 0;
	while( (IIC_Read(STATUS) & (1<<1)) == 0)
	{
		if(++counter > 600) return(-999); //Error out after max of 512ms for a read
		delay(1);
	}

	// Read temperature registers
	Wire.beginTransmission(MPL3115A2_ADDRESS);
	Wire.write(OUT_T_MSB);  // Address of data to get
	Wire.endTransmission(false); // Send data to I2C dev with option for a repeated start. THIS IS NECESSARY and not supported before Arduino V1.0.1!
	Wire.requestFrom(MPL3115A2_ADDRESS, 2); // Request two bytes

	//Wait for data to become available
	counter = 0;
	while(Wire.available() < 2)
	{
		if(counter++ > 100) return(-999); //Error out
		delay(1);
	}

	byte msb, lsb;
	msb = Wire.read();
	lsb = Wire.read();

	toggleOneShot(); //Toggle the OST bit causing the sensor to immediately take another reading

    //Negative temperature fix by D.D.G.
	word foo = 0;
    bool negSign = false;

    //Check for 2s compliment
	if(msb > 0x7F)
	{
        foo = ~((msb << 8) + lsb) + 1;  //2s complement
        msb = foo >> 8;
        lsb = foo & 0x00F0; 
        negSign = true;
	}
	float templsb = (lsb>>4)/16.0; //temp, fraction of a degree

	float temperature = (float)(msb + templsb);

	if (negSign) temperature = 0 - temperature;
	
	return(temperature);
}

//Give me temperature in fahrenheit!
float MPL3115A2::readTempF()
{
  return((readTemp() * 9.0)/ 5.0 + 32.0); // Convert celsius to fahrenheit
}

//Sets the mode to Barometer
//CTRL_REG1, ALT bit
void MPL3115A2::setModeBarometer()
{
  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= ~(1<<7); //Clear ALT bit
  IIC_Write(CTRL_REG1, tempSetting);
}

//Sets the mode to Altimeter
//CTRL_REG1, ALT bit
void MPL3115A2::setModeAltimeter()
{
  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting |= (1<<7); //Set ALT bit
  IIC_Write(CTRL_REG1, tempSetting);
}

//Puts the sensor in standby mode

void MPL3115A2::setModeStandby()
{
  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= ~(1<<0); //Clear SBYB bit for Standby mode
  IIC_Write(CTRL_REG1, tempSetting);
}

//Puts the sensor in active mode

void MPL3115A2::setModeActive()
{
  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting |= (1<<0); //Set SBYB bit for Active mode
  IIC_Write(CTRL_REG1, tempSetting);
}

//Call with a rate from 0 to 7
void MPL3115A2::setOversampleRate(byte sampleRate)
{
  if(sampleRate > 7) sampleRate = 7; //OS cannot be larger than 0b.0111
  sampleRate <<= 3; //Align it for the CTRL_REG1 register
  
  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= 0b11000111; //Clear out old OS bits
  tempSetting |= sampleRate; //Mask in new OS bits
  IIC_Write(CTRL_REG1, tempSetting);
}

//Enables the pressure and temp measurement event flags so that we can
//test against them. 
void MPL3115A2::enableEventFlags()
{
  IIC_Write(PT_DATA_CFG, 0x07); // Enable all three pressure and temp event flags 
}

void MPL3115A2::toggleOneShot(void)
{
  byte tempSetting = IIC_Read(CTRL_REG1); //Read current settings
  tempSetting &= ~(1<<1); //Clear OST bit
  IIC_Write(CTRL_REG1, tempSetting);

  tempSetting = IIC_Read(CTRL_REG1); //Read current settings to be safe
  tempSetting |= (1<<1); //Set OST bit
  IIC_Write(CTRL_REG1, tempSetting);
}


// These are the two I2C functions in this sketch.
byte MPL3115A2::IIC_Read(byte regAddr)
{
  // This function reads one byte over IIC
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(regAddr);  // Address of CTRL_REG1
  Wire.endTransmission(false); 
  Wire.requestFrom(MPL3115A2_ADDRESS, 1); // Request the data
  return Wire.read();
}

void MPL3115A2::IIC_Write(byte regAddr, byte value)
{
  // This function writes one byto over IIC
  Wire.beginTransmission(MPL3115A2_ADDRESS);
  Wire.write(regAddr);
  Wire.write(value);
  Wire.endTransmission(true);
}

