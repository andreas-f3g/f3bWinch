//
/* 
--     Winch Control
--     with supplement digital amperemeter
--    
--    This program is free software: you can redistribute it and/or modify
--    it under the terms of the GNU General Public License as published by
--    the Free Software Foundation; see <http://www.gnu.org/licenses/>.
--     
--    This program is distributed in the hope that it will be useful,
--    but WITHOUT ANY WARRANTY! Without even the implied warranty of
--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE is the program
--    published. See the GNU General Public License for more details.
--     
--                                   Copyright (c) 2026 Andreas Stockhaus
*/

#include <Adafruit_ADS1X15.h>

// Set AD Converter
Adafruit_ADS1115 adc;
// channel mappind AD1115
#define CHAN_VOLTAGE 0
#define CHAN_CURRENT 1

#define MODEPIN 2

#include <LiquidCrystal_I2C.h>

// Set the LCD I2C address
// initialize the lcd for 20 chars 4 lines
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// measure data
float Ub = 0;         // battery voltage (V) before start
int   UminVal = 99;   // battery voltage (V) minimum
char  Umin[6];        // battery voltage (V) minimum
int   UmaxVal = 0;    // battery voltage (V) maximum
char  Umax[6];        // battery voltage (V) maximum
float U300 = 0;       // voltage (V) 300 msec after start
int   I300 = 0;       // current (A) 300 msec after start
int   Imax = 0;       // current (A) maximum
float Rtot = 0;       // resistor result
bool failed = false;  // fail flag
long startTime = 0;   // timestamp start
long endTime = 0;     // timestamp end
bool wMode = false;    // winch test mode = true, digital instrument = false

void setup(void)
{
  char lcdLine[21];
  char lcdValue[5];

  Serial.begin(9600);
  Serial.println("WinchControl");

  // start LCD display
  lcd.init(); 
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("Start");
  delay(1000);

  Serial.println("ADC Range: +/- 6.144V (1 bit = 0.1875mV/ADS1115)");

  if (!adc.begin()) {
    Serial.println("Failed to initialize ADS1115.");
    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print("Fail init ADS1115");
    while (1);
  }

  // Trigger Pin
  pinMode(A0, INPUT);
  pinMode(MODEPIN, INPUT);

  // Modeswitch
  if (digitalRead(MODEPIN) == 0)
  {
    // switch 2 winch mode
      wMode = true;

      // display intro
      lcd.clear();
      lcd.setCursor(0,0); 
      lcd.print("Windentest");
      lcd.setCursor(0,1); 
      lcd.print("Ub Messung");
      lcd.setCursor(0,2); 
      lcd.print("in 5 sec.");
      lcd.setCursor(0,3); 
      int i=20;
      while(i--) {
        lcd.print(".");
        delay(500);
      }

      // Ub calc + init test
      measureUb();

      // LCD display Ub result and readyness
      dtostrf(Ub,5,2,lcdValue);
      lcd.clear();
      lcd.setCursor(0,0);
      sprintf(lcdLine,"Ub=%sV",lcdValue);
      lcd.print(lcdLine);
      lcd.setCursor(0,1);
      lcd.print("Fertig zur Messung!");
      lcd.setCursor(0,2);
      lcd.print("warte auf Start ...");
  }

}

bool measureUb () // process voltage measurement
{
  Ub = adc.computeVolts(adc.readADC_SingleEnded(CHAN_VOLTAGE))*3;
  return true;
}

bool measureI300 () // process measurement
{
  I300 = (int)round(adc.computeVolts(adc.readADC_SingleEnded(CHAN_CURRENT))*1000);
  U300 = adc.computeVolts(adc.readADC_SingleEnded(CHAN_VOLTAGE))*3;
  return true;
}

void loop(void)
{
  char lcdLine[21];
  char lcdValue[5];

  if (wMode) { // winch test mode
    if (startTime == 0) {
      int value = analogRead(A0);
      if (value > 400) {
        // is started
        startTime = millis();
        endTime = startTime + 300/*msec*/;
        float a0Value = (value*15/*5V ref * 3rd division correction */)/1024.0; 
        Serial.print("V_arduino = ");
        Serial.println(a0Value);
        Serial.print("V");
      }
    }
    if (    (endTime <= millis())
        &&  (startTime != 0)
        &&  (I300 == 0)
       ) {

      // 300 msec passed
      endTime=millis();
      measureI300();

      // display end of test
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print(" F E R T I G ");
      delay(1000);

      // LCD display final result
      lcd.clear();
      lcd.setCursor(0,0);
      dtostrf(Ub,5,2,lcdValue);
      sprintf(lcdLine,"Ub=%sV/%dmsec",lcdValue,(endTime-startTime));
      lcd.print(lcdLine);
      lcd.setCursor(0,1);
      sprintf(lcdLine,"I300=%dA",I300);
      lcd.print(lcdLine);
      lcd.setCursor(0,2); 
      dtostrf(U300,5,2,lcdValue);
      sprintf(lcdLine,"U300=%sV",lcdValue);
      lcd.print(lcdLine);
      lcd.setCursor(0,3); 
      dtostrf(((1000*Ub)/I300),5,2,lcdValue);
      sprintf(lcdLine,"Rtot=%smOhm",lcdValue);
      lcd.print(lcdLine);
    }
  } else { // digital instrument mode
    float volt;
    int amp;

    // read AD data
    volt = adc.computeVolts(adc.readADC_SingleEnded(CHAN_VOLTAGE))*3;
    dtostrf(volt,5,2,lcdValue);
    amp = (int)round(adc.computeVolts(adc.readADC_SingleEnded(CHAN_CURRENT))*1000);

    // check boundaries
    if (volt < UminVal) {UminVal=volt; strcpy(Umin,lcdValue);}
    if (volt > UmaxVal) {UmaxVal=volt; strcpy(Umax,lcdValue);}
    if (amp > Imax) Imax = amp;

    // LCD display
    lcd.clear();
    lcd.setCursor(0,0);
    sprintf(lcdLine,"Ub=%sV",lcdValue);
    lcd.print(lcdLine);
    lcd.setCursor(0,1);
    sprintf(lcdLine,"min=%s/max=%s",Umin,Umax);
    lcd.print(lcdLine);
    lcd.setCursor(0,2); 
    sprintf(lcdLine,"I=%dA",amp);
    lcd.print(lcdLine);
    lcd.setCursor(0,3); 
    sprintf(lcdLine,"Imax=%dA",Imax);
    lcd.print(lcdLine);

    delay(1000);

  }

}

