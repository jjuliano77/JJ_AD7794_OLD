/*
Ad7799.cpp - Library for using the AD7799 ADC
Created by Jaimy Juliano, December 28, 2010

Assumes SPI has been already been initialized with:
  SPI.setDataMode(3);
  SPI.setBitOrder(MSBFIRST);
*/

#include "WProgram.h"
#include "Ad7799.h"
#include "SPI.h"


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

Ad7799::Ad7799(byte csPin)
{
  pinMode(csPin, OUTPUT);
  CS = csPin;
  
  vRef = 2.44; //!!! Need to make this set-able !!!!
  
  //Default register settings
  modeReg    = 0x2001; //Single conversion mode, Fadc = 470Hz
  confReg = 0x0010;    //CH 0 - Bipolar, Gain = 1, Input buffer enabled   
    
  isSnglConvMode = true;
  isConverting   = false;
  //currentCh = 0;   
  
}

void Ad7799::begin()
{
  reset();
  delay(2); //4 times the recomended period
  
  //Apply the defaults that were set up in the constructor
  //Should add a begin(,,) method that lets you override the defaults
  for(byte i = 0; i < 3; i++){
    setActiveCh(i);
    writeModeReg();
    writeConfReg();
  }
  
  setActiveCh(0); //Set channel back to 0
  
  //TODO: add any other init code here
}
////////////////////////////////////////////////////////////////////////
//Write 32 1's to reset the chip
void Ad7799::reset()
{  
  digitalWrite(CS, LOW); //Assert CS 
  for(byte i=0;i<4;i++)
  {
    SPI.transfer(0xFF);
  }
  digitalWrite(CS, HIGH);
}

//Sets bipolar/unipolar mode for currently selected channel
void Ad7799::setBipolar(boolean isBipolar)
{    
  Channel[currentCh].isUnipolar = false;
  buildConfReg(currentCh);
  writeConfReg();  
}

void Ad7799::setInputBuffer(boolean isEnabled)
{
  //TODO: Something ;)
}

void Ad7799::setGain(byte g)
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
void Ad7799::setUpdateRate(byte bitMask)
{
  modeReg &= 0xFF00; //Zero off low byte
  modeReg |= bitMask;
  
  writeModeReg();
  //TODO: Put table from datasheet in comments, in header file
}

void Ad7799::setConvMode(boolean isSingle)
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

unsigned long Ad7799::getConvResult()
{
  byte inByte;
  unsigned long result = 0;
  
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
  
  return result;
}

float Ad7799::getVolts()
{
  //Lets the conversion result   
  unsigned long adcRaw = getConvResult();
  
  //And convert to Volts, note: no error checking  
  if(Channel[currentCh].isUnipolar){
    return (adcRaw * vRef) / (ADC_MAX_UP * Channel[currentCh].gain); //Unipolar formula
  }
  else{
    return (((float)adcRaw / ADC_MAX_BP - 1) * vRef) / Channel[currentCh].gain; //Bipolar formula
  }
}

void Ad7799::setActiveCh(byte ch)
{   
  if(ch < CHANNEL_COUNT){ 
    currentCh = ch;
    buildConfReg(currentCh);
    writeConfReg();
  }
}

void Ad7799::startConv()
{
  //Write out the mode reg, but leave CS asserted (LOW)
  digitalWrite(CS,LOW);  
  SPI.transfer(WRITE_MODE_REG);
  SPI.transfer(highByte(modeReg));
  SPI.transfer(lowByte(modeReg)); 
}


//////// Private helper functions/////////////////
void Ad7799::buildConfReg(byte ch)
{
  confReg = (Channel[currentCh].getGainBits() << 8) | ch;
  bitWrite(confReg,12,Channel[currentCh].isUnipolar);
  bitWrite(confReg,4,Channel[currentCh].isBuffered);
}

void Ad7799::writeConfReg()
{
  digitalWrite(CS,LOW);  
  SPI.transfer(WRITE_CONF_REG);
  SPI.transfer(highByte(confReg));
  SPI.transfer(lowByte(confReg)); 
  digitalWrite(CS,HIGH);
}

void Ad7799::writeModeReg()
{
  digitalWrite(CS,LOW);  
  SPI.transfer(WRITE_MODE_REG);
  SPI.transfer(highByte(modeReg));
  SPI.transfer(lowByte(modeReg)); 
  digitalWrite(CS,HIGH);
}
