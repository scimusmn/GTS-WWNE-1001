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
const int relayPin = 10;
const int VdcMonitorPin = A1;
const int voltagePin = A2;
const int currentPin = A3;
// RTC sda and scl attached to A4 and A5

RTC_DS3231 rtc;
Adafruit_NeoPixel strip(144, neoPin, NEO_RGB + NEO_KHZ800);

Timer IncrementPress;
Timer DecrementPress;
Averager averager;

long voltage = 0;
long current;
long power;
const long wattSecondsGoal = 10000;
int numBulbs = 1;  // number of bulbs on per leg.
int lastMonth;     // stores the last read value for month to compare for change
int wattSecondsProduced;

// timing variables
unsigned long currentMillis, lastReadMillis = 0, lastUpdateMillis = 0;
unsigned long lastMonthCheckMillis = 0;

void setup() {
  // Start Serial for debuging purposes
  Serial.begin(115200);
  averager.setup(40);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // TODO ???
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  pinMode(relayPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(voltagePin, INPUT);
  pinMode(currentPin, INPUT);
  pinMode(VdcMonitorPin, INPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(incrementBtn, OUTPUT);
  pinMode(decrementBtn, OUTPUT);

  IncrementPress.setup(
      [](boolean running, boolean ended, unsigned long timeElapsed) {
        if (ended == true) {
          digitalWrite(incrementBtn, LOW);
        }
      },
      600);

  DecrementPress.setup(
      [](boolean running, boolean ended, unsigned long timeElapsed) {
        if (ended == true) {
          digitalWrite(decrementBtn, LOW);
        }
      },
      5005);

  lightLegs(1);
  Serial.println("Here we go...!");

  strip.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();   // Turn OFF all pixels
}

void loop() {
  IncrementPress.update();
  DecrementPress.update();

  voltage = analogRead(voltagePin);
  voltage = map(voltage, 0, 1023, 0, 412);  // store voltage as volts*10
  averager.insertNewSample(voltage);

  if (wattSecondsProduced > wattSecondsGoal) {
    // Serial.println("increment button!");
    digitalWrite(incrementBtn, HIGH);
    IncrementPress.start();
    wattSecondsProduced = 0;
    strip.clear();
    strip.show();
  }

  currentMillis = millis();

  if ((currentMillis - lastReadMillis) > 100)  // every 0.1 seconds
  {
    current = analogRead(currentPin);
    current = map(current, 0, 1023, 0, 682);  // store current as amps*10

    //  Serial.print(power);
    //  Serial.println(" watts");
    //  Serial.print(" I:");
    //  Serial.print(current/10);

    // power = (current * voltage) / 100; // power stored as watts
    // wattSecondsProduced = wattSecondsProduced + ; // TODO tally power
    // produced
    int pixels = map(wattSecondsProduced, 0, wattSecondsGoal, 0, 144);
    pixels = constrain(pixels, 0, 144);
    for (int i = 0; i < pixels; i++) {
      strip.setPixelColor(i, 0, 0, 20);
    }
    strip.show();

    lastReadMillis = currentMillis;
  }

  if ((currentMillis - lastUpdateMillis) > 50) {
    voltage = averager.calculateAverage();
    if (voltage > 200) {
      Serial.print("V:");
      Serial.println(voltage);
    }
    if (voltage > 260) error();
    // if (voltage < 130)
    //   numBulbs--;
    if (voltage < 145) numBulbs--;
    if (voltage > 175) numBulbs++;
    // if (voltage > 180)
    //   numBulbs++;

    numBulbs = constrain(numBulbs, 1, 28);

    lightLegs(numBulbs);

    lastUpdateMillis = currentMillis;
  }

  // TODO edit so it only checks every 5 min or so.
  if ((currentMillis - lastMonthCheckMillis) > 1000) {
    DateTime now = rtc.now();
    if (now.minute() != lastMonth)  // TODO change to month
    {
      // Serial.println("New Month!");
      digitalWrite(decrementBtn, HIGH);
      DecrementPress.start();
      wattSecondsProduced = 0;
      strip.clear();
      strip.show();
      lastMonth = now.minute();  // TODO change to month
    }
    lastMonthCheckMillis = currentMillis;
  }
}

void error(void) {
  Serial.print("Over Voltage Error");
  lightLegs(28);
  digitalWrite(relayPin, HIGH);
  delay(1000);
  while (voltage > 50) {
    voltage = analogRead(voltagePin);
    voltage = map(voltage, 0, 1023, 0, 412);  // store voltage as volts*10
  }
  digitalWrite(relayPin, LOW);
}

void lightLegs(int numberOfBulbs) {
  int oddSide = 0;
  int evenSide = 0;

  for (int j = 0; j < numberOfBulbs; j++) {
    // if j is even shift even light bar
    if ((j % 2) == 0) {
      evenSide = (evenSide << 1) + 1;
    } else {
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

void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
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
  for (i = 7; i >= 0; i--) {
    digitalWrite(myClockPin, 0);

    // if the value passed to myDataOut and a bitmask result
    // true then... so if we are at i=6 and our value is
    // %11010100 it would the code compares it to %01000000
    // and proceeds to set pinState to 1.
    if (myDataOut & (1 << i)) {
      pinState = 1;
    } else {
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
