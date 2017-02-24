/*
 *  program.cpp - OpenPCR control software.
 *  Copyright (C) 2010-2012 Josh Perfetto. All Rights Reserved.
 *
 *  OpenPCR control software is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenPCR control software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  the OpenPCR control software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pcr_includes.h"
#include "thermistors.h"

// lid resistance table, in Ohms
PROGMEM const unsigned int LID_RESISTANCE_TABLE[] = {  
33890,32138,30487,28933,27468,26088,24785,23557,22397,21302,
20268,19291,18367,17493,16667,15885,15145,14444,13780,13151
12554,11988,11452,10942,10459,10000,9564,9150,8756,8382
8026,7687,7365,7058,6765,6487,6222,5969,5728,5499
5279,5070,4871,4680,4498,4324,4158,4000,3848,3703
3564,3431,3304,3183,3066,2955,2848,2746,2648,2554
2463,2377,2294,2215,2138,2065,1995,1927,1862,1800
1740,1682,1627,1574,1522,1473,1426,1380,1336,1294
1253,1214,1176,1140,1105,1071,1038,1007,977,947
919,892,866,840,816,792,769,747,726,705
685,666,648,630,612,595,579,563,548,533
519,505,492,479,466,454,442,431,420,409
399,389,379,369,360,351





};

// plate resistance table, in 0.1 Ohms
PROGMEM const unsigned long PLATE_RESISTANCE_TABLE[] = {
411749,382827,356157,331548,308825,287832,268423,250469,233850,218457
204192,190964,178691,167297,156712,146875,137727,129215,121291,113910
107031,100617,94633,89048,83831,78958,74402,70141,66154,62421
58925,55649,52578,49698,46995,44458,42075,39836,37731,35752
33890,32138,30487,28933,27468,26088,24785,23557,22397,21302
20268,19291,18367,17493,16667,15885,15145,14444,13780,13151
12554,11988,11452,10942,10459,10000,9564,9150,8756,8382
8026,7687,7365,7058,6765,6487,6222,5969,5728,5499
5279,5070,4871,4680,4498,4324,4158,4000,3848,3703
3564,3431,3304,3183,3066,2955,2848,2746,2648,2554
2463,2377,2294,2215,2138,2065,1995,1927,1862,1800
1740,1682,1627,1574,1522,1473,1426,1380,1336,1294
1253,1214,1176,1140,1105,1071,1038,1007,977,947
919,892,866,840,816,792,769,747,726,705
685,666,648,630,612,595

};
  
//spi
#define DATAOUT 11//MOSI
#define DATAIN  12//MISO 
#define SPICLOCK  13//sck
#define SLAVESELECT 10//ss

//------------------------------------------------------------------------------
float TableLookup(const unsigned long lookupTable[], unsigned int tableSize, int startValue, unsigned long searchValue) {
  //simple linear search for now
  int i;
  for (i = 0; i < tableSize; i++) {
    if (searchValue >= pgm_read_dword_near(lookupTable + i))
      break;
  }
  
  if (i > 0) {
    unsigned long high_val = pgm_read_dword_near(lookupTable + i - 1);
    unsigned long low_val = pgm_read_dword_near(lookupTable + i);
    return i + startValue - (float)(searchValue - low_val) / (float)(high_val - low_val);
  } else {
    return startValue;
  }
}
//------------------------------------------------------------------------------
float TableLookup(const unsigned int lookupTable[], unsigned int tableSize, int startValue, unsigned long searchValue) {
  //simple linear search for now
  int i;
  for (i = 0; i < tableSize; i++) {
    if (searchValue >= pgm_read_word_near(lookupTable + i))
      break;
  }
  
  if (i > 0) {
    unsigned long high_val = pgm_read_word_near(lookupTable + i - 1);
    unsigned long low_val = pgm_read_word_near(lookupTable + i);
    return i + startValue - (float)(searchValue - low_val) / (float)(high_val - low_val);
  } else {
    return startValue;
  }
}

////////////////////////////////////////////////////////////////////
// Class CLidThermistor
CLidThermistor::CLidThermistor():
  iTemp(0.0) {
}
//------------------------------------------------------------------------------
void CLidThermistor::ReadTemp() {
  unsigned long voltage_mv = (unsigned long)analogRead(1) * 5000 / 1024;
  unsigned long resistance = voltage_mv * 2200 / (5000 - voltage_mv);
  
  iTemp = TableLookup(LID_RESISTANCE_TABLE, sizeof(LID_RESISTANCE_TABLE) / sizeof(LID_RESISTANCE_TABLE[0]), 0, resistance);
}

////////////////////////////////////////////////////////////////////
// Class CPlateThermistor
CPlateThermistor::CPlateThermistor():
  iTemp(0.0) {

  //spi setup
  pinMode(DATAOUT, OUTPUT);
  pinMode(DATAIN, INPUT);
  pinMode(SPICLOCK,OUTPUT);
  pinMode(SLAVESELECT,OUTPUT);
  digitalWrite(SLAVESELECT,HIGH); //disable device 
}
//------------------------------------------------------------------------------
void CPlateThermistor::ReadTemp() {
  digitalWrite(SLAVESELECT, LOW);

  //read data
  while(digitalRead(DATAIN)) {}
  
  uint8_t spiBuf[4];
  memset(spiBuf, 0, sizeof(spiBuf));

  digitalWrite(SLAVESELECT, LOW);  
  for(int i = 0; i < 4; i++)
    spiBuf[i] = SPITransfer(0xFF);

  unsigned long conv = (((unsigned long)spiBuf[3] >> 7) & 0x01) + ((unsigned long)spiBuf[2] << 1) + ((unsigned long)spiBuf[1] << 9) + (((unsigned long)spiBuf[0] & 0x1F) << 17); //((spiBuf[0] & 0x1F) << 16) + (spiBuf[1] << 8) + spiBuf[2];
  
  unsigned long adcDivisor = 0x1FFFFF;
  float voltage = (float)conv * 5.0 / adcDivisor;

  unsigned int convHigh = (conv >> 16);
  
  digitalWrite(SLAVESELECT, HIGH);
  
  unsigned long voltage_mv = voltage * 1000;
  unsigned long resistance = voltage_mv * 2200 / (5000 - voltage_mv); // in hecto ohms
 
  iTemp = TableLookup(PLATE_RESISTANCE_TABLE, sizeof(PLATE_RESISTANCE_TABLE) / sizeof(PLATE_RESISTANCE_TABLE[0]), -40, resistance);
}
//------------------------------------------------------------------------------
char CPlateThermistor::SPITransfer(volatile char data) {
  SPDR = data;                    // Start the transmission
  while (!(SPSR & (1<<SPIF)))     // Wait the end of the transmission
  {};
  return SPDR;                    // return the received byte
}
