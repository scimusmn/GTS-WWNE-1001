//**************************************************************//
//  Component: Why We Need Energy 1001                          //
//  Project  : Gateway to Science                               //
//  Author   : Joe Meyer                                        //
//  Date     : 2/12/2021                                        //
//**************************************************************//
#include <Adafruit_NeoPixel.h>

#include "RTClib.h"
#include "arduino-base/Libraries/Averager.h"
#include "arduino-base/Libraries/Timer.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PIN ASSIGNMENTS
const int dataPin = 2;
const int clockPin = 3;
const int latchPin = 4;
const int neoPin = 5;
const int incrementBtn = 6;
const int decrementBtn = 7;
const int bellPin = 9;
const int relayPin = 10;
const int relayPin2 = 11;
const int VdcMonitorPin = A1;
const int voltagePin = A2;
const int currentPin = A3;
const int num_pixels = 98;
// RTC sda and scl attached to A4 and A5

// Configuration
const int voltageCalibration = 412;
const int currentCalibration = 682;
const long wattSecondsPerBag = 3000; //126000;

RTC_DS3231 rtc;
Adafruit_NeoPixel bagOMeter(num_pixels, neoPin, NEO_RGB + NEO_KHZ800);
Timer CounterIncrement;
Timer CounterReset;
Averager voltageAverager;
Averager currentAverager;

// Variables
long voltage = 0;
long current;
long power;
bool newMonthReset = false;
int numBulbs = 1; // number of bulbs on per leg.
int wattSecondsProduced;
unsigned long currentMillis, lastReadMillis = 0, lastUpdateMillis = 0, lastSampleMillis = 0;
unsigned long lastMonthCheckMillis = 0;

void setup()
{
  // Start Serial for debuging purposes
  Serial.begin(115200);
  voltageAverager.setup(20);
  currentAverager.setup(20);

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

  pinMode(relayPin, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(voltagePin, INPUT);
  pinMode(currentPin, INPUT);
  pinMode(VdcMonitorPin, INPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(incrementBtn, OUTPUT);
  pinMode(bellPin, OUTPUT);
  pinMode(decrementBtn, OUTPUT);

  digitalWrite(relayPin, LOW);
  digitalWrite(relayPin2, LOW);

  CounterIncrement.setup(
      [](boolean running, boolean ended, unsigned long timeElapsed) {
        if (ended == true)
        {
          digitalWrite(incrementBtn, LOW);
          digitalWrite(bellPin, LOW);
        }
      },
      600);

  CounterReset.setup(
      [](boolean running, boolean ended, unsigned long timeElapsed) {
        if (ended == true)
        {
          digitalWrite(decrementBtn, LOW);
          Serial.println("Reset Complete (new month)!");
        }
      },
      5005);

  lightLegs(1);
  Serial.println("Here we go...!");

  bagOMeter.begin(); // INITIALIZE NeoPixel bagOMeter object
  bagOMeter.show();  // Turn OFF all pixels
}

void loop()
{

  currentMillis = millis();
  CounterIncrement.update();
  CounterReset.update();

  if ((currentMillis - lastSampleMillis) > 10) // every 0.1 seconds
  {
    //running average for voltage
    int v = analogRead(voltagePin);
    v = map(v, 0, 1023, 0, voltageCalibration); // store voltage as volts*10
    voltageAverager.insertNewSample(v);

    //running average for current
    int c = analogRead(currentPin);
    c = map(c, 0, 1023, 0, currentCalibration); // store current as amps*10
    currentAverager.insertNewSample(c);

    lastSampleMillis = currentMillis;
  }

  if (wattSecondsProduced > wattSecondsPerBag)
  {
    digitalWrite(incrementBtn, HIGH);
    CounterIncrement.start();
    wattSecondsProduced = 0;
    bagOMeter.clear();
    bagOMeter.show();
    digitalWrite(bellPin, HIGH);
  }

  if ((currentMillis - lastReadMillis) > 100) // every 0.1 seconds
  {

    // Serial.print(power);
    // Serial.println(" watts");
    // Serial.print(" I:");
    // Serial.print(current / 10);
    voltage = voltageAverager.calculateAverage();
    current = currentAverager.calculateAverage();
    power = ((current / 10) * (voltage / 10));              // power stored as watts
    wattSecondsProduced = wattSecondsProduced + power / 10; //wattSecondsProduced updated every 0.1 seconds
    int pixels = map(wattSecondsProduced, 0, wattSecondsPerBag, 0, num_pixels);
    pixels = constrain(pixels, 0, num_pixels);
    for (int i = 0; i <= pixels; i++)
    {
      bagOMeter.setPixelColor(num_pixels - i, 20, 20, 0);
    }
    bagOMeter.show();

    lastReadMillis = currentMillis;
  }

  if ((currentMillis - lastUpdateMillis) > 50)
  {
    
    voltage = voltageAverager.calculateAverage();
    current = currentAverager.calculateAverage();
    Serial.print("V:");
    Serial.println(voltage / 10);
    Serial.print(", I:");
    Serial.println(current / 10);

    if (voltage > 260)
      overVoltage();

    int bulbs = map(voltage, 170, 240, 1, 28);
    if (bulbs > numBulbs)
      numBulbs = bulbs;
    if (voltage < 140)
      numBulbs--;

    numBulbs = constrain(numBulbs, 1, 28);

    lightLegs(numBulbs);

    lastUpdateMillis = currentMillis;
  }

  // Every 5 min check if it's a
  if ((currentMillis - lastMonthCheckMillis) > 3000)
  {
    DateTime now = rtc.now();
    //if it's the 1st of the month and has not been reset.
    if ((now.day() == 1) && (!newMonthReset))
    {
      Serial.println("New Month");
      digitalWrite(decrementBtn, HIGH);
      CounterReset.start();
      wattSecondsProduced = 0;
      bagOMeter.clear();
      bagOMeter.show();
      newMonthReset = true;
    }
    if (now.day() != 1)
      newMonthReset = false;
    lastMonthCheckMillis = currentMillis;

    // Serial.print(now.month());
    // Serial.print("/");
    // Serial.print(now.day());
    // Serial.print("/");
    // Serial.print(now.year());
    // Serial.print("  ");
    // Serial.print(now.hour());
    // Serial.print(":");
    // Serial.print(now.minute());
    // Serial.print("  wattMin:");
    // Serial.println(wattSecondsProduced / 60);
  }
}

void overVoltage(void)
{
  Serial.print("Over Voltage Error");
  lightLegs(28);
  digitalWrite(relayPin, HIGH);
  digitalWrite(relayPin2, HIGH);
  delay(1000);
  while (voltageAverager.calculateAverage() > 50)
  {
    Serial.print("V:");
    Serial.print(voltage / 10);
    Serial.print(", I:");
    Serial.println(current / 10);
    voltage = analogRead(voltagePin);
    voltage = map(voltage, 0, 1023, 0, voltageCalibration); // store voltage as volts*10
    voltageAverager.insertNewSample(voltage);
  }
  digitalWrite(relayPin, LOW);
  digitalWrite(relayPin2, LOW);
}

void lightLegs(int numberOfBulbs)
{
  int oddSide = 0;
  int evenSide = 0;

  for (int j = 0; j < numberOfBulbs; j++)
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
  // ground latchPin and hold low for as long as you are transmitting
  digitalWrite(latchPin, 0);
  // count up on GREEN LEDs
  shiftOut(dataPin, clockPin, MSBFIRST, byte(oddSide));
  shiftOut(dataPin, clockPin, MSBFIRST, byte(oddSide >> 8));
  shiftOut(dataPin, clockPin, MSBFIRST, byte(evenSide));
  shiftOut(dataPin, clockPin, MSBFIRST, byte(evenSide >> 8));
  // return the latch pin high to signal chip that it
  // no longer needs to listen for information
  digitalWrite(latchPin, 1);
}

void shiftOut(int myDataPin, int myClockPin, byte myDataOut)
{
  // This shifts 8 bits out MSB first,
  // on the rising edge of the clock,
  // clock idles low

  // internal function setup
  int i = 0;
  int pinState;

  // clear everything out just in case to
  // prepare shift register for bit shifting
  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  // for each bit in the byte myDataOut�
  // NOTICE THAT WE ARE COUNTING DOWN in our for loop
  // This means that %00000001 or "1" will go through such
  // that it will be pin Q0 that lights.
  for (i = 7; i >= 0; i--)
  {
    digitalWrite(myClockPin, 0);

    // if the value passed to myDataOut and a bitmask result
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

    // Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(myDataPin, pinState);
    // register shifts bits on upstroke of clock pin
    digitalWrite(myClockPin, 1);
    // zero the data pin after shift to prevent bleed through
    digitalWrite(myDataPin, 0);
  }

  // stop shifting
  digitalWrite(myClockPin, 0);
}
