#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "AudioGeneratorAAC.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioOutputI2S.h"

#include "sampleaac.h"


/* ========== AUDIO ========== */

AudioFileSourcePROGMEM *in = nullptr;
AudioGeneratorAAC *aac = nullptr;
AudioOutputI2S *out = nullptr;

#define I2S_BCLK  16
#define I2S_LRCLK 17
#define I2S_DIN   21


/* ========== BMI088 ========== */

#define BMI088_ACC_ADDRESS 0x18

#define AAC_PWR_CTRL  0x7D
#define ACC_PWR_Conf  0x7C
#define ACC_RANGE     0x41


/* ========== BMI088 ========== */

#define SAMPLE_INTERVAL_MS  10
#define BUMP_THRESHOLD_G    0.5
#define COOLDOWN_MS         2000 // 2 sek mellem bump-lyde

unsigned long lastTriggerTime = 0;
unsigned long lastSample = 0;

float lastAx = 0;
float lastAy = 0;
float lastAz = 0;

bool playing = false;


/* ========== BMI088 læser ========== */

int16_t readAccelAxis(uint8_t regL) {
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(regL);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI088_ACC_ADDRESS, 2);

  if (Wire.available() < 2) return 0

  uint8_t l = Wire.read();
  uint8_t h = Wire.read();
  return (int16_t) ((h << 8) | l);
}


/* ========== Definer playback ========== */

void startPlayback() {
  in = new AudioFileSourcePROGMEM(sampleaac, sizeof(sampleaac));
  aac = new AudioGeneratorAAC();
  aac->begin(in, out);
  playing = true;
}


/* ========== Definer stop player ========== */

void stopPlayback() {
  aac->stop();
  delete aac;
  delete in;
  aac = nullptr;
  in = nullptr;
  playing = false;
}


/* ========== Setup ========== */

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();

  // -------- BMI088 init --------

  // Tænd for den
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_PWR_CTRL);
  Wire.write(0x04);
  Wire.endTransmission();
  delay(50)

  // Tændt ting
  Write.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_PWR_CONF);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  // Giv range af +- 6g
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_RANGE);
  Wire.write(0x01);   // +- 6g
  Wire.endTransmission();
  delay(50)

  // -------- Audio init --------

  audioLogger = &Serial;

  out = new AudioOutputI2S(0, 1);
  out->SetPinout(I2S_BCLK, I2S_LRCLK, I2S_DIN);
  out->SetGain(0.3);

  Serial.println("BMI088 bump ting ready:]");
}

void loop() {
  unsigned long now = millis();

  // Læs acceleration (ny ting jeg har lavet, please sig hvis det er fucket op)
  int16_t ax_raw = readAccelAxis(0x12); 
  int16_t ay_raw = readAccelAxis(0x14);
  int16_t az_raw = readAccelAxis(0x16);

  // Antager ±6g range
  float ax = ax_raw * 0.000183;
  float ay = ay_raw * 0.000183;
  float az = az_raw * 0.000183;

  float dx = ax - lastAx;
  float dy = ay - lastAy;
  float dz = az - lastAz;

  float shake = sqrt(dx*dx + dy*dy + dz*dz);

  float totalG = sqrt(ax*ax + ay*ay + az*az);

  // Start playback når bump registreres
  if (!playing &&
      totalG > BUMP_THRESHOLD_G &&
      (now - lastTriggerTime > COOLDOWN_MS)) {

    Serial.print("Shake: ");
    Serial.println(shake);

    startPlayback();
    lastTriggerTime = now;
  }

  // Run decoder while playing
  if (playing) {
    if (aac->isRunning()) {
      aac->loop();
    } else {
      Serial.println("Playback finished");
      stopPlayback();
    }
  }

  delay(10);
}
