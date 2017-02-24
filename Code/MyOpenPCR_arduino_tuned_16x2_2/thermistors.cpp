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
28704,27417,26197,25039,23940,22897,21906,20964,20070,19219,
18410,17641,16909,16212,15548,14916,14313,13739,13192,12669,
12171,11696,11242,10809,10395,10000,9622,9261,8916,8585,
8269,7967,7678,7400,7135,6881,6637,6403,6179,5965,
5759,5561,5372,5189,5015,4847,4686,4531,4382,4239,
4101,3969,3842,3719,3601,3488,3379,3274,3172,3075,
2981,2890,2803,2719,2638,2559,2484,2411,2341,2273,
2207,2144,2083,2024,1967,1912,1858,1807,1757,1709,
1662,1617,1574,1532,1491,1451,1413,1376,1340,1305,
1272,1239,1208,1177,1147,1118,1091,1063,1037,1012,
987,963,940,917,895,874,853,833,814,795,
776,758,741,724,708,692,676,661,646,632,
618,604,591,578,566,554
};

// plate resistance table, in 0.1 Ohms
PROGMEM const unsigned long PLATE_RESISTANCE_TABLE[] = {
248277,233136,219036,205897,193648,182221,171556,161596,152290,143590,
135452,127837,120707,114028,107768,101898,96391,91222,86369,81809,
77523,73492,69701,66132,62771,59606,56623,53810,51157,48654,
46290,44058,41950,39957,38072,36290,34603,33006,31494,30062,
28704,27417,26197,25039,23940,22897,21906,20964,20070,19219,
18410,17641,16909,16212,15548,14916,14313,13739,13192,12669,
12171,11696,11242,10809,10395,10000,9622,9261,8916,8585,
8269,7967,7678,7400,7135,6881,6637,6403,6179,5965,
5759,5561,5372,5189,5015,4847,4686,4531,4382,4239,
4101,3969,3842,3719,3601,3488,3379,3274,3172,3075,
2981,2890,2803,2719,2638,2559,2484,2411,2341,2273,
2207,2144,2083,2024,1967,1912,1858,1807,1757,1709,
1662,1617,1574,1532,1491,1451,1413,1376,1340,1305,
1272,1239,1208,1177,1147,1118,1091,1063,1037,1012,
987,963,940,917,895,874
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
