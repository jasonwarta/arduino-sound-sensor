/**
Jason Warta
CS 241 Final Project
Arduino Noise level detector

Class: CS241 - Computer Hardware Concepts - Spring 2021
Instructor: Dr Lawlor


Hardware:
Arduino Uno
K851264 LED RGB from lab kit
SEN-15892 Zio Qwiic Loudness Sensor
4x4 Keypad


This software allows a user to configure noise gates,
and show a different colored indicator (green, yellow, or red)
to indicate if the noise level in a room is too high.

The intended purpose was for a grade-school teacher to use
to indicate that students need to quiet down.


The software holds 4 thresholds in memory, to set the color of the light.

300 - 500: green
500 - 700: yellow
700 - 900: red

Configuration steps to change the thresholds:
1. Press '#' on the keypad. this puts the device in listening mode.
2. Enter a multidigit number by pressing the desired keys.
3. When you have finished entering the number, press '*'.
4. Repeat steps 2-3 for 2 more numbers.
5. Repeat step 2 again for a fourth number, but press '#' to finish the sequence.


 */

#include <SoftwareSerial.h>
#include <Keypad.h>
#include <Wire.h>



/*
    QWIIC Configuration
 */

#define COMMAND_LED_OFF     0x00
#define COMMAND_LED_ON      0x01
#define COMMAND_GET_VALUE   0x05
#define COMMAND_NOTHING_NEW 0x99

const byte QWIIC_ADDRESS = 0x38;  //Default Address



/*
    Keypad Configuration
 */

const uint8_t NUM_ROWS = 4;
uint8_t KEYPAD_ROWS[NUM_ROWS] = {9,8,7,6};

const uint8_t NUM_COLS = 4;
uint8_t KEYPAD_COLS[NUM_COLS] = {5,4,3,2};

char KEYPAD_LAYOUT[NUM_ROWS][NUM_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

Keypad keypad = Keypad(makeKeymap(KEYPAD_LAYOUT), KEYPAD_ROWS, KEYPAD_COLS, NUM_ROWS, NUM_COLS);



/*
    Color Configuration
 */

const uint8_t NUM_RGB_PINS = 3;
uint8_t RGB_PINS[NUM_RGB_PINS] = {12, 10, 11};

enum Color {
  red,
  yellow,
  green
};
int currentColor = red;

const uint8_t NUM_COLORS = 3;
uint8_t colors[NUM_COLORS][3] = {
  {255, 0, 0},    // red
  {255, 230, 15}, // yellow
  {0, 255, 0}     // green
};




/*
    Threshold Configuration
 */

const uint8_t NUM_THRESHOLDS = 4;
int thresholds[NUM_THRESHOLDS] = {300, 500, 700, 900};

enum ThresholdProgrammingState {
  listening,
  firstNum,
  secondNum,
  thirdNum,
  fourthNum,
};
ThresholdProgrammingState state = listening;



/*
    Variables for sound detection
 */

uint16_t adcValue=0;
uint8_t adcValueLow;
uint8_t adcValueHigh;



/*
  Setup Fucntion
 */

void setup() {
  Serial.begin(9600);

  // Set correct mode for pins

  for(uint8_t i = 0; i < NUM_RGB_PINS; i++) {
    pinMode(RGB_PINS[i], OUTPUT);
  }

  for (uint8_t i = 0; i < NUM_ROWS; i++)
  {
    pinMode(KEYPAD_ROWS[i], INPUT);
  }

  for (uint8_t i = 0; i < NUM_COLS; i++)
  {
    pinMode(KEYPAD_COLS[i], INPUT);
  }
  
  // set up microphone
  Wire.begin();
  testForConnectivity();
  ledOn();
  delay(1000);

  Serial.println("Sensors and inputs are ready");
}

/*
  Main Loop
 */

char currentKey;
void loop() {
  currentKey = keypad.getKey();

  if (currentKey != '\0') {
    handleKeypress(currentKey);
  }

  checkAudioLevel();

  delay(100);
}



/*
  Display pressed key and dispatch an action if required
 */
void handleKeypress(char key) {
  Serial.print("current key: '");
  Serial.print(key);
  Serial.println("'");

  if (key == '#' && state == listening) {
    changeThresholds();
  }
}


/*
  Change noise thresholds.
  Uses a state machine to gather input from the user for each of the 4 thresholds.
  When the last threshold is reached, state machine is set back to initial state and the function exits
 */
void changeThresholds() {
  Serial.println("Starting threshold reprogramming procecss");

  // update state
  state = firstNum;

  char triggerKey = '*';
  char key;
  String current = "";

  // state is managed in the loop.
  // loop should exit when we've finished cycling through the states
  while(state != listening) {
    key = keypad.waitForKey();

    while(key != triggerKey) {
      Serial.println(key);

      switch(key) {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case '#':
        case '*':
          break;
        
        default:
          current += key;
          break;
      }

      key = keypad.waitForKey();
    }

    Serial.println(current);
    
    switch (state) {
      case firstNum:
        thresholds[0] = current.toInt();
        current = "";
        state = secondNum;
        break;

      case secondNum:
        thresholds[1] = current.toInt();
        current = "";
        state = thirdNum;
        break;

      case thirdNum:
        thresholds[2] = current.toInt();
        triggerKey = '#'; // update exit key for next loop through
        current = "";
        state = fourthNum;
        break;

      case fourthNum:
        thresholds[3] = current.toInt();
        if (thresholds[3] > 1023) thresholds[3] = 1023; // the sensor shouldn't ever return a number greater than 1023
        state = listening;
        rotateThroughColors(5);
        break;
    }
  }

  for(uint8_t i = 0; i < NUM_THRESHOLDS; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(thresholds[i]);
  }
}


/*
  Cycle through the colors a number of times.
  Used for user feedback to indicate *something* happened
 */
void rotateThroughColors(uint8_t cycles) {
  for(uint8_t i = 0; i < cycles; i++) {
    displayColor(red);
    delay(200);
    displayColor(yellow);
    delay(200);
    displayColor(green);
    delay(200);
    displayColor(yellow);
    delay(200);
  }
}


/*
  Get Audio level from Loudness Sensor
 */
void checkAudioLevel() {
  Wire.beginTransmission(QWIIC_ADDRESS);
  Wire.write(COMMAND_GET_VALUE);
  Wire.endTransmission();

  // request 1 bytes from slave device qwiicAddress
  Wire.requestFrom(QWIIC_ADDRESS, 2);

  while (Wire.available()) {
    adcValueLow = Wire.read();
    adcValueHigh = Wire.read();

    adcValue = adcValueHigh;
    adcValue <<= 8;
    adcValue |= adcValueLow;
  }

  Serial.print("ADC: ");
  Serial.print(adcValue);

  setColorFromInput(adcValue);
}


/*
  update color of LED based on configured thresholds
 */
void setColorFromInput(uint16_t value) {
  Serial.print("\tcolor: ");
  if (value >= thresholds[0] && value < thresholds[1]) {
    displayColor(green);
    Serial.println("GREEN");
  }
  else if (value >= thresholds[1] && value < thresholds[2]) {
    displayColor(yellow);
    Serial.println("YELLOW");
  }
  else if (value >= thresholds[2] && value < thresholds[3]) {
    displayColor(red);
    Serial.println("RED");
  }
}


/*
  display a configured color on the LED
 */
void displayColor(Color color) {
  analogWrite(RGB_PINS[0], colors[color][0]);
  analogWrite(RGB_PINS[1], colors[color][1]);
  analogWrite(RGB_PINS[2], colors[color][2]);
}


/*
  turn on diagnostic LED on QWIIC device
 */
void ledOn() {
  Wire.beginTransmission(QWIIC_ADDRESS);
  Wire.write(COMMAND_LED_ON);
  Wire.endTransmission();
}


/*
  turn off diagnostic LED on QWIIC device
 */
void ledOff() {
  Wire.beginTransmission(QWIIC_ADDRESS);
  Wire.write(COMMAND_LED_OFF);
  Wire.endTransmission();
}


/*
  check for an ACK from QWIIC device.
  If no ACK, program freezes and notifies user
 */
void testForConnectivity() {
  Wire.beginTransmission(QWIIC_ADDRESS);
  //check here for an ACK from the slave, if no ACK don't allow change?
  if (Wire.endTransmission() != 0) {
    Serial.println("Check connections. No device attached.");
    while (1);
  }
}
