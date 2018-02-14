/*
AD7794.h - Library for using the AD7794 ADC
Created by Jaimy Juliano, December 28, 2010

UPDATED 2-2018 include SPI transactions and tested on SAMD21 and Teensy 3.2

Assumes SPI has been already been initialized with:
  SPI.setDataMode(3);
  SPI.setBitOrder(MSBFIRST);
*/

#ifndef AD7794_h
#define AD7794_h

//#include "WProgram.h"
#include <SPI.h>

#define CHANNEL_COUNT  8 //6 + temp and AVDD Monitor
//Communications register settings
#define WRITE_MODE_REG 0x08 //selects mode reg for writing
#define WRITE_CONF_REG 0x10 //selects conf reg for writing
#define READ_DATA_REG  0x58 //selects data reg for reading
#define ADC_MAX_UP     16777216
#define ADC_MAX_BP     8388608
#define READ_DELAY     10   //delay (in mS) to wait for conversion

class ChannelClass
{
  private:
    byte gainBits;

  public:
    byte gain;
    boolean isBuffered;
    boolean isUnipolar;

    ChannelClass();
    void init(byte gn, boolean isBuff, boolean isUni);
    byte getGainBits();
};

class AD7794
{
  public:
    AD7794(byte csPin, unsigned long spiFrequency);
    void begin();
    void reset();
    void setBipolar(boolean isBipolar);
    void setInputBuffer(boolean isEnabled);
    void setGain(byte gain); //might want to just require the bit mask to save code size????
    void setUpdateRate(byte bitMask);
    void setConvMode(boolean isSingleConv);
    unsigned long getReadingRaw(int ch);
    float getReadingVolts(int ch);
    void setActiveCh(byte ch);

  private:
    //Private helper functions
    void startConv();
    unsigned long getConvResult();
    void writeConfReg();
    void writeModeReg();
    void buildConfReg(byte ch);

    byte CS;
    byte currentCh;
    //unsigned long frequency;
    //byte gain[3];
    float vRef;

    ChannelClass Channel[CHANNEL_COUNT];
    SPISettings spiSettings;

    word modeReg; //holds value for 16 bit mode register
    word confReg; //holds value for 16 bit configuration register

    boolean isSnglConvMode;
    boolean isConverting;
};

#endif
