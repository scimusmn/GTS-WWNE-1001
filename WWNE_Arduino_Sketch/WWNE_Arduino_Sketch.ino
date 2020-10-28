//**************************************************************//
//  Component: Why We Need Energy 1001                          //
//  Project  : Gateway to Science                               //
//  Author   : Joe Meyer                                        //
//  Date     : 8/17/2020                                        //
//**************************************************************//
#include "RTClib.h"

#include <AutoPID.h>

//pid settings and gains
#define OUTPUT_MIN 1
#define OUTPUT_MAX 28
#define KP .3
#define KI 0.5
#define KD 2000

//Pin assignments
const int clockPin = 3;
const int dataPin = 2;
const int latchPin = 4;
const int voltagePin = A2;
const int currentPin = A3;
const int relayPin = 13;
const int powerOutageWatch = A1;
const int incrementBtn = 6;
const int resetBtn = 7;

const long wattSecondsGoal = 120000;

//long voltage = 0;
long current;
long power;
double voltage, setPoint = 140, numBulbs = 1;
// int numBulbs = 1; // number of bulbs on per leg.
int lastMonth; //stores the last read value for month to compare for change
int wattSecondsProduced;
unsigned long currentMillis, lastReadMillis = 0, lastMonthCheckMillis = 0;

//input/output variables passed by reference, so they are updated automatically
AutoPID myPID(&voltage, &setPoint, &numBulbs, OUTPUT_MIN, OUTPUT_MAX, KP, KI, KD);
// RTC sda and scl attached to A4 and A5
RTC_DS3231 rtc;

void setup()
{
  //Start Serial for debuging purposes
  Serial.begin(115200);
  Serial.println("WWNE running");

  pinMode(relayPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(voltagePin, INPUT);
  pinMode(currentPin, INPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(incrementBtn, OUTPUT);
  pinMode(resetBtn, OUTPUT);

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // TODO ???
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  //set PID update interval to 50ms
  myPID.setTimeStep(100);

  //if voltage is more than 4V below or above setpoint, OUTPUT will be set to min or max respectively
  myPID.setBangBang(0, 50);

  lightLegs(28);
}

void loop()
{
  currentMillis = millis();

  if (wattSecondsProduced > wattSecondsGoal)
  {
    Serial.println("increment button!");
    // digitalWrite(incrementBtn, HIGH); //TODO
    wattSecondsProduced = 0;
  }

  voltage = analogRead(voltagePin);
  current = analogRead(currentPin);
  voltage = map(voltage, 0, 1023, 0, 412); // store voltage as volts*10
  current = map(current, 0, 1023, 0, 682); // store current as amps*10

  if (voltage > 100)
  {
    Serial.print("V:");
    Serial.println(voltage / 10);
  }
  if ((currentMillis - lastReadMillis) > 100) // every 0.1 seconds
  {
    power = (current * voltage) / 100; // power stored as watts
    power = 50;
    wattSecondsProduced = wattSecondsProduced + (power / 10);
    lastReadMillis = currentMillis;
    //    Serial.print(power);
    //    Serial.println(" watts");
    //    Serial.print(" I:");
    //    Serial.println(current/10);
  }

  if ((currentMillis - lastMonthCheckMillis) > 1000) //TODO edit so it only checks every 5 min or so.
  {
    DateTime now = rtc.now();
    if (now.minute() != lastMonth) //TODO change to month
    {
      Serial.println("New Month!");
      // digitalWrite(resetBtn, HIGH); //TODO
      lastMonth = now.minute(); //TODO change to month
    }
    //    Serial.println(wattSecondsProduced);
    lastMonthCheckMillis = currentMillis;
  }

  myPID.run(); //call every loop, updates automatically at certain time interval
  lightLegs(numBulbs);

  if (voltage > 230)
    error();
}

void error(void)
{
  Serial.print("Over Voltage Error");
  digitalWrite(relayPin, HIGH);
  lightLegs(1);
  while (voltage > 80)
  {
    voltage = analogRead(voltagePin);
    voltage = map(voltage, 0, 1023, 0, 412); // store voltage as volts*10
  }
  digitalWrite(relayPin, LOW);
}

void lightLegs(int nB)
{
  nB = 29 - nB;

  int oddSide = 0;
  int evenSide = 0;

  for (int j = 0; j < nB; j++)
  {
    // if j is even shift even light bar
    if ((j % 2) == 0)
    {
      evenSide = (evenSide << 1) + 1;
    }
    else
    {
      // j is odd shift odd light bar
      oddSide = (oddSide << 1) + 1;
    }
  }
  //ground latchPin and hold low for as long as you are transmitting
  digitalWrite(latchPin, 0);
  //count up on GREEN LEDs
  shiftOut(dataPin, clockPin, MSBFIRST, byte(evenSide));
  shiftOut(dataPin, clockPin, MSBFIRST, byte(evenSide >> 8));
  shiftOut(dataPin, clockPin, MSBFIRST, byte(oddSide));
  shiftOut(dataPin, clockPin, MSBFIRST, byte(oddSide >> 8));
  //return the latch pin high to signal chip that it
  //no longer needs to listen for information
  digitalWrite(latchPin, 1);
}

void shiftOut(int myDataPin, int myClockPin, byte myDataOut)
{
  // This shifts 8 bits out MSB first,
  //on the rising edge of the clock,
  //clock idles low

  //internal function setup
  int i = 0;
  int pinState;

  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  //for each bit in the byte myDataOut�
  //NOTICE THAT WE ARE COUNTING DOWN in our for loop
  //This means that %00000001 or "1" will go through such
  //that it will be pin Q0 that lights.
  for (i = 7; i >= 0; i--)
  {
    digitalWrite(myClockPin, 0);

    //if the value passed to myDataOut and a bitmask result
    // true then... so if we are at i=6 and our value is
    // %11010100 it would the code compares it to %01000000
    // and proceeds to set pinState to 1.
    if (myDataOut & (1 << i))
    {
      pinState = 1;
    }
    else
    {
      pinState = 0;
    }

    //Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(myDataPin, pinState);
    //register shifts bits on upstroke of clock pin
    digitalWrite(myClockPin, 1);
    //zero the data pin after shift to prevent bleed through
    digitalWrite(myDataPin, 0);
  }

  //stop shifting
  digitalWrite(myClockPin, 0);
}
