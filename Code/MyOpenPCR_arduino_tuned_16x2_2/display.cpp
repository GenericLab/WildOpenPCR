/*
 *  display.cpp - OpenPCR control software.
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
#include "display.h"

#include "thermocycler.h"
#include "thermistors.h"
#include "program.h"

#define RESET_INTERVAL 30000 //ms

//progmem strings
const char HEATING_STR[] PROGMEM = "Heating";
const char COOLING_STR[] PROGMEM = "Cooling";
const char LIDWAIT_STR[] PROGMEM = "HeatLid";
const char STOPPED_STR[] PROGMEM = "Ready";
const char RUN_COMPLETE_STR[] PROGMEM = "*Done*";
const char OPENPCR_STR[] PROGMEM = "OpenPCR";
const char POWERED_OFF_STR[] PROGMEM = "Off";
const char ETA_OVER_1000H_STR[] PROGMEM = "MaxETA";

const char LID_FORM_STR[] PROGMEM = "%3d\x01";
const char CYCLE_FORM_STR[] PROGMEM = "%2d/%2d";
const char ETA_HOURMIN_FORM_STR[] PROGMEM = "ETA: %d:%02d";
const char ETA_SEC_FORM_STR[] PROGMEM = "ETA:  %2ds";
const char BLOCK_TEMP_FORM_STR[] PROGMEM = "%s\x02";
const char STATE_FORM_STR[] PROGMEM = "%-7.7s";
const char VERSION_FORM_STR[] PROGMEM = "v%s";

 byte lid[8] = {
  B10011,
  B00100,
  B00100,
  B00011,
  B00000,
  B11111,
  B10001,
};

 byte block[8] = {
  B10011,
  B00100,
  B00100,
  B00011,
  B00000,
  B10001,
  B11111,
};

Display::Display():
  iLcd(6, 7, 8, A5, 16, 17),
  iLastState(Thermocycler::EStartup) {

  iLcd.begin(16, 2);
  iLastReset = millis();
#ifdef DEBUG_DISPLAY
  iszDebugMsg[0] = '\0';
#endif
  
  // Set contrast
  iContrast = ProgramStore::RetrieveContrast();
  analogWrite(5, iContrast);
  
  iLcd.createChar(1, lid);
    iLcd.createChar(2, block);
}

void Display::Clear() {
  iLastState = Thermocycler::EClear;
}

void Display::SetContrast(uint8_t contrast) {
  iContrast = contrast;
  analogWrite(5, iContrast);
  iLcd.begin(16, 2);
    iLcd.createChar(1, lid);
    iLcd.createChar(2, block);
}
  
void Display::SetDebugMsg(char* szDebugMsg) {
#ifdef DEBUG_DISPLAY
  strcpy(iszDebugMsg, szDebugMsg);
#endif
  iLcd.clear();
  Update();
}

void Display::Update() {
  char buf[16];
  
  Thermocycler::ProgramState state = GetThermocycler().GetProgramState();
  if (iLastState != state)
    iLcd.clear();
  iLastState = state;
  
  // check for reset
  if (millis() - iLastReset > RESET_INTERVAL) {  
    iLcd.begin(16, 2);
      iLcd.createChar(1, lid);
    iLcd.createChar(2, block);
    iLastReset = millis();
  }
  
  switch (state) {
  case Thermocycler::ERunning:
  case Thermocycler::EComplete:
  case Thermocycler::ELidWait:
  case Thermocycler::EStopped:
    iLcd.setCursor(8, 0);
 #ifdef DEBUG_DISPLAY
    iLcd.print(iszDebugMsg);
 #else
    iLcd.print(GetThermocycler().GetProgName());
 #endif
           
    DisplayLidTemp();
    DisplayBlockTemp();
    DisplayState();

    if (state == Thermocycler::ERunning && !GetThermocycler().GetCurrentStep()->IsFinal()) {
      DisplayCycle();
      DisplayEta();
    } else if (state == Thermocycler::EComplete) {
      iLcd.setCursor(0, 0);
      iLcd.print(rps(RUN_COMPLETE_STR));
    }
    break;
  
  case Thermocycler::EStartup:
    iLcd.setCursor(0, 0);
    iLcd.print(rps(OPENPCR_STR));

      iLcd.setCursor(8, 0);
      sprintf_P(buf, VERSION_FORM_STR, OPENPCR_FIRMWARE_VERSION_STRING);
      iLcd.print(buf);
    break;
  }
}

void Display::DisplayEta() {
  char timeString[16];
  unsigned long timeRemaining = GetThermocycler().GetTimeRemainingS();
  int hours = timeRemaining / 3600;
  int mins = (timeRemaining % 3600) / 60;
  int secs = timeRemaining % 60;
  
  if (hours >= 1000)
    strcpy_P(timeString, ETA_OVER_1000H_STR);
  else if (mins >= 1 || hours >= 1)
    sprintf_P(timeString, ETA_HOURMIN_FORM_STR, hours, mins);
  else
    sprintf_P(timeString, ETA_SEC_FORM_STR, secs);
  
 // iLcd.setCursor(20 - strlen(timeString), 3);
 // iLcd.print(timeString);
}

void Display::DisplayLidTemp() {
  char buf[16];
  sprintf_P(buf, LID_FORM_STR, (int)(GetThermocycler().GetLidTemp() + 0.5));

  iLcd.setCursor(6, 1);
  iLcd.print(buf);
}

void Display::DisplayBlockTemp() {
  char buf[16];
  char floatStr[16];
  
  sprintFloat(floatStr, GetThermocycler().GetPlateTemp(), 1, true);
  sprintf_P(buf, BLOCK_TEMP_FORM_STR, floatStr);
 
  iLcd.setCursor(0, 1);
  iLcd.print(buf);
}

void Display::DisplayCycle() {
  char buf[16];
  
  iLcd.setCursor(11, 1);
  sprintf_P(buf, CYCLE_FORM_STR, GetThermocycler().GetCurrentCycleNum(), GetThermocycler().GetNumCycles());
  iLcd.print(buf);
}

void Display::DisplayState() {
  char buf[32];
  char* stateStr;
  
  switch (GetThermocycler().GetProgramState()) {
  case Thermocycler::ELidWait:
    stateStr = rps(LIDWAIT_STR);
    break;
    
  case Thermocycler::ERunning:
  case Thermocycler::EComplete:
    switch (GetThermocycler().GetThermalState()) {
    case Thermocycler::EHeating:
      stateStr = rps(HEATING_STR);
      break;
    case Thermocycler::ECooling:
      stateStr = rps(COOLING_STR);
      break;
    case Thermocycler::EHolding:
      stateStr = GetThermocycler().GetCurrentStep()->GetName();
      break;
    case Thermocycler::EIdle:
      stateStr = rps(STOPPED_STR);
      break;
    }
    break;
    
  case Thermocycler::EStopped:
    stateStr = rps(STOPPED_STR);
    break;
  }
  
  iLcd.setCursor(0, 0);
  sprintf_P(buf, STATE_FORM_STR, stateStr);
  iLcd.print(buf);
}
