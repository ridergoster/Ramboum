#include <LiquidCrystal.h>
#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
// --------------------- DEFINE FOR MPU650 -----------------------------------
#define YAW      0
#define PITCH    1
#define ROLL     2
// --------------------- DEFINE FOR POSITION ---------------------------------
#define LEFT 0
#define MIDDLE 1
#define RIGHT 2
// --------------------- DEFINE FOR MODE -------------------------------------
#define MENU 0
#define SINGLE 1
#define MULTIPLE 2
// --------------------- MPU650 variables ------------------------------------
MPU6050 mpu;
// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
// Orientation/motion vars
Quaternion q;        // [w, x, y, z]         quaternion container
VectorFloat gravity; // [x, y, z]            gravity vector
float ypr[3];        // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
// --------------------- Sensor variables ------------------------------------
const int rs = 5, en = 4, d4 = A0, d5 = A1, d6 = 1, d7 = 0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const int roadLed[3] = {8, 7, 6};
const int playerLed[3] = {9, 10, 11};
const int gearDownBtn = 12;
const int gearUpBtn = 13;
const int knockSensor[3] = {A3, A4, A5};
const int soundSensor = A2;
// --------------------- Sensor Values ---------------------------------------
int roadLedState[3] = {LOW, LOW, LOW};
int playerLedState[3] = {LOW, HIGH, LOW};
int knockValue = 0;
int soundValue = 0;
int soundMin = 1024;
int soundMax = 0;
int gearUpValue = LOW;
int gearDownValue = LOW;
// --------------------- Sensor Threshold ------------------------------------
const int knockThreshold = 200;
const int soundThreshold = 500;
// --------------------- Timer Values ----------------------------------------
unsigned long randomCarMillis = 0;
unsigned long frameMillis = 0;
unsigned long speedCooldownMillis = 0;
unsigned long soundMillis = 0;
unsigned long wheelMillis = 0;
unsigned long knockMillis = 0;
unsigned long currentMillis = 0;

int randomCarInterval = 0;
const int speedCooldownInterval = 1000;
const int soundInterval = 50;
const int wheelInterval = 250;
const int knockInterval = 500;
// --------------------- Games parameters ------------------------------------
const int roadSize = 20;
const int vMax[8] = {4000, 3000, 2000, 1000, 500, 300, 200, 100};
bool road[3][roadSize];
int playerPosition = MIDDLE;
int playerMode = MENU;
int gearSpeed = 0;
int startSpeed = 5000;
int randomCarDelay = 1000;
int playerSpeed = startSpeed;
int score = 0;
int life = 3;
char numStr[3];
const int wheelAngle = 35;
// ---------------------------------------------------------------------------

// Setup the parameters
void setup() {
  // Init components
  lcd.begin(16, 2);
  Serial.begin(9600);
  Wire.begin();
  mpu.initialize();

  // Verify connection
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  // Load and configure the DMP
  devStatus = mpu.dmpInitialize();

  // MPU calibration: set YOUR offsets here.
  mpu.setXAccelOffset(-4413);
  mpu.setYAccelOffset(-62);
  mpu.setZAccelOffset(1556);
  mpu.setXGyroOffset(11);
  mpu.setYGyroOffset(-50);
  mpu.setZGyroOffset(33);

  // Returns 0 if it worked
  if (devStatus == 0) {
    // Turn on the DMP, now that it's ready
    mpu.setDMPEnabled(true);

    // Set our DMP Ready flag so the main loop() function knows it's okay to use it
    dmpReady = true;
    // Get expected DMP packet size for later comparison
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    // ERROR!
    Serial.print(F("DMP Initialization failed (code "));
    Serial.print(devStatus);
    Serial.println(F(")"));
  }

  // init road array
  memset(road, 0, sizeof(bool[0][0]) * 3 * roadSize);

  // init pinMode
  pinMode(roadLed[0], OUTPUT);
  pinMode(roadLed[1], OUTPUT);
  pinMode(roadLed[2], OUTPUT);

  pinMode(playerLed[0], OUTPUT);
  pinMode(playerLed[1], OUTPUT);
  pinMode(playerLed[2], OUTPUT);

  pinMode(gearDownBtn, INPUT);
  pinMode(gearUpBtn, INPUT);

  // trigger random seed
  randomSeed(analogRead(0));
}

// FIFO update for gyroscope
void updateFIFO() {
  while ((fifoCount = mpu.getFIFOCount()) < packetSize) {
    Serial.print("updating FIFO");
  }

  if (fifoCount == 1024) {
    mpu.resetFIFO();
    Serial.println(F("FIFO overflow!"));
    updateFIFO();
  }
}

// game over screen
void gameOver() {
  lcd.setCursor(0, 0);
  lcd.print("GAME OVER...    ");
  lcd.setCursor(0, 1);
  lcd.print("score:");
  sprintf(numStr,"%3d", score);
  lcd.setCursor(6, 1);
  lcd.print(numStr);
  lcd.setCursor(9, 1);
  lcd.print("       ");

  // press 2 buttons to restart the game
  if (gearDownValue == HIGH && gearUpValue == HIGH) {
    playerPosition = MIDDLE;
    playerSpeed = startSpeed;
    gearSpeed = 0;
    score = 0;
    life = 3;
    for (int i = 0 ; i < 3 ; i++)
      for(int j = 0 ; j < roadSize ; j++)
        road[i][j] = false;
  }
}

// get menu screen
void getMenu() {
  lcd.setCursor(0, 0);
  lcd.print("LEFT:      MULTI");
  lcd.setCursor(0, 1);
  lcd.print("RIGHT:     SOLO");
  if (gearDownValue == HIGH) {
    playerMode = MULTIPLE;
  } else if (gearUpValue == HIGH) {
    playerMode = SINGLE;
  }
}

// trigger random car
void getRandomCar() {
  randomCarInterval = random(playerSpeed, playerSpeed+randomCarDelay);

  if (currentMillis - randomCarMillis < randomCarInterval) {
    return;
  }

  randomCarMillis = currentMillis;
  int roadNb = random(0, 5);
  if (roadNb < 3) {
    road[roadNb][roadSize-1] = true;
  }
}

// trigger car from knock sensor
void getKnockingCar() {
  if (currentMillis - knockMillis < knockInterval) {
    return;
  }

  for(int i = 0; i < 3 ; i++) {
    knockValue = analogRead(knockSensor[i]);
    if (knockValue >= knockThreshold) {
      road[i][roadSize-1] = true;
      knockMillis = currentMillis;
      return;
    }
  }
}

// update player position
void updatePlayer() {
  if (currentMillis - wheelMillis < wheelInterval) {
    return;
  }

  wheelMillis = currentMillis;

  updateFIFO();

  mpu.getFIFOBytes(fifoBuffer, packetSize);
  fifoCount -= packetSize;

  mpu.dmpGetQuaternion(&q, fifoBuffer);
  mpu.dmpGetGravity(&gravity, &q);
  mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

  float iZAxis = ypr[ROLL] * (180 / M_PI);

  if (iZAxis < -wheelAngle) {
    if (playerPosition == LEFT) {
      playerPosition = MIDDLE;
    } else if (playerPosition == MIDDLE) {
      playerPosition = RIGHT;
    }
  } else if (iZAxis > wheelAngle) {
    if (playerPosition == RIGHT) {
      playerPosition = MIDDLE;
    } else if (playerPosition == MIDDLE) {
      playerPosition = LEFT;
    }
  }
}

// update speed and gear
void updateSpeed() {

  // update gear with buttons
  if (currentMillis - speedCooldownMillis >= speedCooldownInterval) {
    if (
      gearDownValue == HIGH &&
      gearSpeed > 0 &&
      playerSpeed >= ((vMax[gearSpeed] + vMax[gearSpeed-1])/2)
    ) {
      gearSpeed = gearSpeed - 1;
      playerSpeed = vMax[gearSpeed];
      speedCooldownMillis = currentMillis;
    } else if (
      gearUpValue == HIGH &&
      gearSpeed < 8 &&
      playerSpeed <= vMax[gearSpeed]+10
    ) {
      gearSpeed = gearSpeed + 1;
      speedCooldownMillis = currentMillis;
    }
  }

  // update speed with microphone
  if (currentMillis - soundMillis >= soundInterval) {
    soundMillis = currentMillis;
    soundValue = soundMax - soundMin;
    soundMax = 0;
    soundMin = 1024;

    if (soundValue < soundThreshold && playerSpeed < startSpeed) {
      playerSpeed = playerSpeed + 5;
      if (gearSpeed > 0 && playerSpeed > (vMax[gearSpeed-1]+20)) {
        gearSpeed = gearSpeed - 1;
      }
    } else if (soundValue >= soundThreshold && playerSpeed > vMax[gearSpeed]) {
      int newSpeed = playerSpeed - 50;
      playerSpeed = newSpeed < vMax[gearSpeed] ? vMax[gearSpeed] : newSpeed;
    }
  } else {
    int sample = analogRead(soundSensor);

    if (sample < 1024) {
       if (sample > soundMax) {
          soundMax = sample;
       } else if (sample < soundMin) {
          soundMin = sample;
       }
    }
  }
}

// update road to next frame
void updateRoad() {
  for (int i = 0; i < 3; i++) {
    for (int j = 0 ; j < roadSize-1; j++) {
       road[i][j] = road [i][j+1];
    }
    road[i][roadSize-1] = false;
  }
}

// update score to next frame
void updateScore() {
  for (int i = 0 ; i < 3 ; i++) {
    if (road[i][0] == true) {
      if (i == playerPosition) {
        life = life - 1;
      } else {
        score = score + 1;
      }
    }
  }
}

// print road to Serial
void printGame() {
  Serial.println();

  for (int i = 0; i < 3; i++) {
    for (int j = 0 ; j < roadSize; j++) {
      Serial.print(road[i][j] ? "0" : "X");
    }
    Serial.println();
  }
}

// change light status to next frame
void updateLight() {
  for (int i = 0; i < 3; i++) {
    bool haveCar = false;
    for (int j = 1 ; j < 3; j++) {
      if (road[i][j] == true) {
        roadLedState[i] = HIGH;
        haveCar = true;
      }
    }
    if (haveCar == false) {
      roadLedState[i] = LOW;
    }

    playerLedState[i] = i == playerPosition ? HIGH : LOW;

    digitalWrite(roadLed[i], roadLedState[i]);
    digitalWrite(playerLed[i], playerLedState[i]);
  }
}

// print data to lcd
void printData() {
  lcd.setCursor(0, 0);
  lcd.print("life:");
  lcd.setCursor(5, 0);
  lcd.print(life);
  lcd.setCursor(8, 0);
  lcd.print("gear:");
  lcd.setCursor(13, 0);
  sprintf(numStr,"%3d", gearSpeed);
  lcd.print(numStr);

  lcd.setCursor(0, 1);
  lcd.print("sc:");
  lcd.setCursor(3, 1);
  sprintf(numStr,"%3d", score);
  lcd.print(numStr);
  lcd.setCursor(6, 1);
  lcd.print(" speed:");
  lcd.setCursor(13, 1);
  sprintf(numStr,"%3d", ((startSpeed - playerSpeed)/20));
  lcd.print(numStr);
}

void loop() {
  Serial.println(playerMode);
  // getting new value for the loop
  currentMillis = millis();
  gearDownValue = digitalRead(gearDownBtn);
  gearUpValue = digitalRead(gearUpBtn);

  // stop state
  if (!dmpReady) {
    return;
  } else if (life <= 0) {
    gameOver();
    return;
  }

  // mode state
  if (playerMode == MENU) {
    getMenu();
    return;
  }
  else if (playerMode == SINGLE) {
    getRandomCar();
  } else {
    getKnockingCar();
  }

  // update value
  updatePlayer();
  updateSpeed();

  // update frame
  if (currentMillis - frameMillis >= playerSpeed) {
    frameMillis = currentMillis;
    updateRoad();
    updateScore();
    // render frame
    printGame();
  }

  updateLight();
  printData();
}
