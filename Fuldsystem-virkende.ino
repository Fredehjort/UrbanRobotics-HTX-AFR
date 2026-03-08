#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <LiquidCrystal.h>

#include "AudioGeneratorAAC.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioOutputI2S.h"
#include "AudioLogger.h"
#include "sampleaac.h"

const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int buttonPin = 6;
volatile bool state = true;

#define BMI088_ACC_ADDRESS 0x19
#define ACC_PWR_CTRL   0x7D
#define ACC_PWR_CONF   0x7C
#define ACC_RANGE      0x41

#define SAMPLE_INTERVAL_MS  10
#define BUMP_THRESHOLD_G    0.5f
#define COOLDOWN_MS         2000

unsigned long lastTriggerTime = 0;
unsigned long lastSample      = 0;

float lastAx = 0.0f, lastAy = 0.0f, lastAz = 0.0f;
bool  playing = false;

AudioFileSourcePROGMEM *in   = nullptr;
AudioGeneratorAAC      *aac  = nullptr;
AudioOutputI2S         *out  = nullptr;

#define I2S_BCLK  16
#define I2S_LRCLK 17
#define I2S_DIN   21

byte openEye[8] = {
  B00000,
  B00000,
  B01110,
  B10001,
  B10001,
  B10001,
  B10001,
  B01110
};
byte halfClosedEye[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B10001,
  B10001,
  B01110
};
byte closedEye[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B10001,
  B10001,
  B10001,
  B01110
};
byte leftSquint[8] = {
  B00000,
  B00000,
  B00000,
  B11000,
  B00110,
  B00001,
  B00110,
  B11000
};
byte rightSquint[8] = {
  B00000,
  B00000,
  B00000,
  B00011,
  B01100,
  B10000,
  B01100,
  B00011
};
byte mouth[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B10001,
  B10101,
  B01010
};
byte mouthSquint[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00100,
  B01010,
  B01010
};

enum chars {
  OPENEYE = 0,
  CLOSEDEYE,
  HALFCLOSEDEYE,
  LEFTSQUINT,
  RIGHTSQUINT,
  MOUTH,
  MOUTHSQUINT
};

int faceCycle = 0;

int16_t readAccelAxis(uint8_t regL) {
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(regL);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI088_ACC_ADDRESS, (uint8_t)2);
  if (Wire.available() < 2) return 0;
  uint8_t l = Wire.read();
  uint8_t h = Wire.read();
  return (int16_t)((h << 8) | l);
}

void startPlayback() {
  if (!out) return;
  in  = new AudioFileSourcePROGMEM(sampleaac, sizeof(sampleaac));
  aac = new AudioGeneratorAAC();
  aac->begin(in, out);
  playing = true;
}

void stopPlayback() {
  if (aac) {
    aac->stop();
    delete aac;
    aac = nullptr;
  }
  if (in) {
    delete in;
    in = nullptr;
  }
  playing = false;
}

void showNormalFace() {
  lcd.setCursor(0, 0);
  if (faceCycle % 2 == 0) {
    lcd.write(HALFCLOSEDEYE);
    lcd.write(MOUTH);
    lcd.write(HALFCLOSEDEYE);
  } else {
    lcd.write(OPENEYE);
    lcd.write(MOUTH);
    lcd.write(OPENEYE);
  }
  faceCycle++;
}

void showSquintFace() {
  lcd.setCursor(0, 0);
  lcd.write(LEFTSQUINT);
  lcd.write(MOUTHSQUINT);
  lcd.write(RIGHTSQUINT);
}

void IRAM_ATTR statechange() {
  state = !state;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  lcd.begin(16, 2);
  lcd.createChar(OPENEYE, openEye);
  lcd.createChar(CLOSEDEYE, closedEye);
  lcd.createChar(HALFCLOSEDEYE, halfClosedEye);
  lcd.createChar(LEFTSQUINT, leftSquint);
  lcd.createChar(RIGHTSQUINT, rightSquint);
  lcd.createChar(MOUTH, mouth);
  lcd.createChar(MOUTHSQUINT, mouthSquint);
  lcd.setCursor(0, 0);
  showNormalFace();

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), statechange, FALLING);

  Wire.begin();

  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_PWR_CTRL);
  Wire.write(0x04);
  Wire.endTransmission();
  delay(50);

  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_PWR_CONF);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_RANGE);
  Wire.write(0x01);
  Wire.endTransmission();
  delay(50);

  audioLogger = &Serial;
  out = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S);
  out->SetPinout(I2S_BCLK, I2S_LRCLK, I2S_DIN);
  out->SetGain(0.3f);

  Serial.println("BMI088 bump detector ready :)");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSample < SAMPLE_INTERVAL_MS) {
    if (playing && aac) {
      if (aac->isRunning()) aac->loop();
      else stopPlayback();
    }
    return;
  }
  lastSample = now;

  int16_t ax_raw = readAccelAxis(0x12);
  int16_t ay_raw = readAccelAxis(0x14);
  int16_t az_raw = readAccelAxis(0x16);

  float ax = ax_raw * 0.000183f;
  float ay = ay_raw * 0.000183f;
  float az = az_raw * 0.000183f;

  float dx = ax - lastAx;
  float dy = ay - lastAy;
  float dz = az - lastAz;

  float shake   = sqrt(dx * dx + dy * dy + dz * dz);
  float totalG  = sqrt(ax * ax + ay * ay + az * az);

  lastAx = ax;
  lastAy = ay;
  lastAz = az;

  bool bumpDetected = false;

  if (!playing &&
      totalG > BUMP_THRESHOLD_G &&
      (now - lastTriggerTime > COOLDOWN_MS)) {
    Serial.print("Shake: ");
    Serial.println(shake, 3);
    startPlayback();
    lastTriggerTime = now;
    bumpDetected = true;
  }

  if (playing && aac) {
    if (aac->isRunning()) aac->loop();
    else {
      Serial.println("Playback finished");
      stopPlayback();
    }
  }

  if (state) {
    if (bumpDetected) showNormalFace();
  } else {
    showSquintFace();
  }
}
