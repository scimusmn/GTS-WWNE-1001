//**************************************************************//
//  Component: Why We Need Energy 1001                          //
//  Project  : Gateway to Science                               //
//  Author   : Joe Meyer                                        //
//  Date     : 8/17/2020                                        //
//**************************************************************//

#define clockPin 2
#define dataPin 3
#define latchPin 4
#define voltagePin A1
#define currentPin A2
#define relayPin A5

// int dataPins[] = {0, 3, 5, 7, 9};
// int latchPins[] = {0, 10, 8, 6, 4};
long voltage = 0;
long current;
long power;
// int totalBulbs = 0;
int numBulbs = 1; // number of bulbs on per leg.
// int legNumBulbs[] = {0, 1, 0, 0, 0};
// int legIndex = 1;

unsigned long currentMillis, lastReadMillis = 0;

void setup()
{
  //Start Serial for debuging purposes
  //  Serial.begin(115200);
  //set pins to output because they are addressed in the main loop
  pinMode(relayPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(voltagePin, INPUT);
  pinMode(currentPin, INPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // for (int i = 1; i < 5; i++)
  // {
  //   pinMode(latchPins[i], OUTPUT);
  //   pinMode(dataPins[i], OUTPUT);

  lightLegs();
  // }
}

void loop()
{
  currentMillis = millis();
  voltage = analogRead(voltagePin);
  current = analogRead(currentPin);

  voltage = map(voltage, 0, 1023, 0, 206); // store voltage as volts*10
  current = map(current, 0, 1023, 0, 682); // store current as amps*10
  power = (current * voltage) / 100;       // power stored as watts

  if ((currentMillis - lastReadMillis) > 500)
  {
    lastReadMillis = currentMillis;
    //    Serial.print(power);
    //    Serial.println(" watts");
    //    Serial.print("V:");
    //    Serial.println(voltage/10);
    //    Serial.print(" I:");
    //    Serial.print(current/10);
    //    Serial.print("   L1:");
    //    Serial.print(legNumBulbs[1]);
    //    Serial.print(" L2:");
    //    Serial.print(legNumBulbs[2]);
    //    Serial.print(" L3:");
    //    Serial.print(legNumBulbs[3]);
    //    Serial.print(" L4:");
    //    Serial.println(legNumBulbs[4]);
  }

  if ((voltage > 170) && (numBulbs < 28))
  { //if voltage is greater than 17V
    numBulbs++;
    lightLegs();
  }

  if ((voltage < 135) && (numBulbs > 1))
  { //if voltage is less than 12V
    numBulbs--;
    lightLegs();
  }

  if (voltage > 300)
  {
    error();
  }
}

void error(void)
{
  //  Serial.print("Over Voltage Error");
  digitalWrite(relayPin, HIGH);
  while (voltage > 80)
  {
    voltage = analogRead(voltagePin);
    voltage = map(voltage, 0, 1023, 0, 206); // store voltage as volts*10
  }
  digitalWrite(relayPin, LOW);
}

void lightLegs()
{
  int oddSide = 0;
  int evenSide = 0;

  for (int j = 0; j < numBulbs; j++)
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
