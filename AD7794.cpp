/*
AD7794.cpp - Library for using the AD7794 ADC
Created by Jaimy Juliano, December 28, 2010

UPDATED 2-2018 include SPI transactions and tested on SAMD21 and Teensy 3.2

Assumes SPI has been already been initialized with:
  SPI.setDataMode(3);
  SPI.setBitOrder(MSBFIRST);
*/

#include "Ad7794.h"
#include <SPI.h>


ChannelClass::ChannelClass()
{
  //Set some defaults
  gainBits = 0x00;
  isBuffered = true;
  isUnipolar = false;
}
void ChannelClass::init(byte gn, boolean isBuff, boolean isUni)
{
  gain = gn;
  isBuffered = isBuff;
  isUnipolar = isUni;
}

byte ChannelClass::getGainBits()
{
  switch(gain){
    case 1:
      gainBits |= 0x00;
      break;
    case 2:
      gainBits |= 0x01;
      break;
    case 4:
      gainBits |= 0x02;
      break;
    case 8:
      gainBits |= 0x03;
      break;
    case 16:
      gainBits |= 0x04;
      break;
    case 32:
      gainBits |= 0x05;
      break;
    case 64:
      gainBits |= 0x06;
      break;
    case 128:
      gainBits |= 0x07;
      break;
    default:
      //OOPS!, shouldn't be here, well just set it to 0x00 (gain = 1)
      gainBits |= 0x00;
      break;
  }
  return gainBits;
}

AD7794::AD7794(byte csPin, unsigned long spiFrequency)
{
  //pinMode(csPin, OUTPUT);
  CS = csPin;
  spiSettings = SPISettings(spiFrequency,MSBFIRST,SPI_MODE3);

  vRef = 2.50; //!!! Need to make this set-able !!!!

  //Default register settings
  modeReg    = 0x2001; //Single conversion mode, Fadc = 470Hz
  confReg = 0x0010;    //CH 0 - Bipolar, Gain = 1, Input buffer enabled

  isSnglConvMode = true;
  isConverting   = false;
  //currentCh = 0;

}

void AD7794::begin()
{
  SPI.begin();

  reset();
  delay(2); //4 times the recomended period

  //Apply the defaults that were set up in the constructor
  //Should add a begin(,,) method that lets you override the defaults
  for(byte i = 0; i < CHANNEL_COUNT-2; i++){ //<-- Channel count stuff needs to be handled better!!
    setActiveCh(i);
    writeModeReg();
    writeConfReg();
  }

  setActiveCh(0); //Set channel back to 0

  //TODO: add any other init code here
}
////////////////////////////////////////////////////////////////////////
//Write 32 1's to reset the chip
void AD7794::reset()
{
  // Speed set to 4MHz, SPI mode set to MODE 3 and Bit order set to MSB first.
  SPI.beginTransaction(spiSettings);
  digitalWrite(CS, LOW); //Assert CS
  for(byte i=0;i<4;i++)
  {
    SPI.transfer(0xFF);
  }
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
}

//Sets bipolar/unipolar mode for currently selected channel
void AD7794::setBipolar(boolean isBipolar)
{
  Channel[currentCh].isUnipolar = false;
  buildConfReg(currentCh);
  writeConfReg();
}

void AD7794::setInputBuffer(boolean isEnabled)
{
  //TODO: Something ;)
}

void AD7794::setGain(byte g)
{
  Channel[currentCh].gain = g;

  buildConfReg(currentCh);
  writeConfReg();
}

/******************************************************
Sets low byte of the mode register. The high nibble
must be 0, the low nibble sets the mode (FS3->FS0)
[EXAMPLE '\x01' = 470Hz]
refer to datasheet for modes
*/
void AD7794::setUpdateRate(byte bitMask)
{
  modeReg &= 0xFF00; //Zero off low byte
  modeReg |= bitMask;

  writeModeReg();
  //TODO: Put table from datasheet in comments, in header file
}

void AD7794::setConvMode(boolean isSingle)
{
  if(isSingle == true){
    isSnglConvMode = true;
    modeReg &= 0x00FF;
    modeReg |= 0x2000;
  }
  else{
    isSnglConvMode = false;
    modeReg &= 0x00FF;
  }

  writeModeReg();
}

// OK, for now I'm not going to handle the spi transaction inside the startConversion()
// and getConvResult() functions. They are very low level and the behavior is different
// depending on conversion mode (and other things ?). think I will make these private
// and create a higher level functions to get readings.
unsigned long AD7794::getConvResult()
{
  byte inByte;
  unsigned long result = 0;

  //SPI.beginTransaction(spiSettings); //We should still be in our transaction
  SPI.transfer(READ_DATA_REG);

  //Read 24 bits one byte at a time, and put in an unsigned long
  inByte = SPI.transfer(0xFF); //dummy byte
  result = inByte;
  inByte = SPI.transfer(0xFF); //dummy byte
  result = result << 8;
  result = result | inByte;
  inByte = SPI.transfer(0xFF); //dummy byte
  result = result << 8;
  result = result | inByte;

  //De-assert CS if not in continous conversion mode
  if(isSnglConvMode == true){
    digitalWrite(CS,HIGH);
  }

  //SPI.endTransaction(); //Need to look into how to handle continous conversion mode
  return result;
}

float AD7794::getReadingVolts(int ch)
{
  //Lets the conversion result
  unsigned long adcRaw = getReadingRaw(ch);

  //And convert to Volts, note: no error checking
  if(Channel[currentCh].isUnipolar){
    return (adcRaw * vRef) / (ADC_MAX_UP * Channel[currentCh].gain); //Unipolar formula
  }
  else{
    return (((float)adcRaw / ADC_MAX_BP - 1) * vRef) / Channel[currentCh].gain; //Bipolar formula
  }
}

// This function is BLOCKING. I'm not sure if it is even possible to make a
// no blocking version with this chip on a shared SPI bus.
unsigned long AD7794::getReadingRaw(int ch)
{
  SPI.beginTransaction(spiSettings);

  setActiveCh(ch);
  startConv();
  delay(READ_DELAY);
  unsigned long adcRaw = getConvResult();

  SPI.endTransaction();
  return adcRaw;
}

void AD7794::setActiveCh(byte ch)
{
  if(ch < CHANNEL_COUNT){
    currentCh = ch;
    buildConfReg(currentCh);
    writeConfReg();
  }
}

// OK, for now I'm not going to handle the spi transaction inside the startConversion()
// and getConvResult() functions. They are very low level and the behavior is different
// depending on conversion mode (and other things ?). think I will make these private
// and create a higher level functions to get readings.
void AD7794::startConv()
{
  //Write out the mode reg, but leave CS asserted (LOW)
  //SPI.beginTransaction(spiSettings);
  digitalWrite(CS,LOW);
  SPI.transfer(WRITE_MODE_REG);
  SPI.transfer(highByte(modeReg));
  SPI.transfer(lowByte(modeReg));
  //Don't end the transaction yet. for now this is going to have to hold on
  //to the buss until the conversion is complete. not sure if there is a way around this
}


//////// Private helper functions/////////////////
void AD7794::buildConfReg(byte ch)
{
  confReg = (Channel[currentCh].getGainBits() << 8) | ch;
  bitWrite(confReg,12,Channel[currentCh].isUnipolar);
  bitWrite(confReg,4,Channel[currentCh].isBuffered);
}

void AD7794::writeConfReg()
{
  SPI.beginTransaction(spiSettings);
  digitalWrite(CS,LOW);
  SPI.transfer(WRITE_CONF_REG);
  SPI.transfer(highByte(confReg));
  SPI.transfer(lowByte(confReg));
  digitalWrite(CS,HIGH);
  SPI.endTransaction();
}

void AD7794::writeModeReg()
{
  SPI.beginTransaction(spiSettings);
  digitalWrite(CS,LOW);
  SPI.transfer(WRITE_MODE_REG);
  SPI.transfer(highByte(modeReg));
  SPI.transfer(lowByte(modeReg));
  digitalWrite(CS,HIGH);
  SPI.endTransaction();
}
