#include <Arduino.h>
#include <Wire.h>

#include "AudioGeneratorAAC.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioOutputI2S.h"

#include "sampleaac.h"

AudioFileSourcePROGMEM *in;
AudioGeneratorAAC *aac;
AudioOutputI2S *out;

#define I2S_BCLK  16
#define I2S_LRCLK 17
#define I2S_DIN   21

// BMI088 I2C stuffs
#define BMI088_ACC_ADDRESS 0x18

#define BUMP_THRESHOLD_G 1.2     // dynamisk acceleration (efter -1g)
#define COOLDOWN_MS 2000         // 2 sek mellem bump-lyde

unsigned long lastTriggerTime = 0;
bool playing = false;

// BMI088 læser
int16_t readAccelAxis(uint8_t regL) {
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(regL);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI088_ACC_ADDRESS, 2);

  int16_t l = Wire.read();
  int16_t h = Wire.read();
  return (h << 8) | l;
}

void startPlayback() {
  in = new AudioFileSourcePROGMEM(sampleaac, sizeof(sampleaac));
  aac = new AudioGeneratorAAC();
  aac->begin(in, out);
  playing = true;
}

void stopPlayback() {
  aac->stop();
  delete aac;
  delete in;
  aac = nullptr;
  in = nullptr;
  playing = false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();

  audioLogger = &Serial;

  out = new AudioOutputI2S(0, 1);
  out->SetPinout(I2S_BCLK, I2S_LRCLK, I2S_DIN);
  out->SetGain(0.3);

  Serial.println("BMI088 bump detector ready");
}

void loop() {
  unsigned long now = millis();

  // Læs acceleration (ny ting jeg har lavet, please sig hvis det er fucket op)
  int16_t ax_raw = readAccelAxis(0x12); // X_LSB
  int16_t ay_raw = readAccelAxis(0x14);
  int16_t az_raw = readAccelAxis(0x16);

  // Antager ±6g range
  float ax = ax_raw * 0.000183;
  float ay = ay_raw * 0.000183;
  float az = az_raw * 0.000183;

  float totalG = sqrt(ax*ax + ay*ay + az*az);

  // Fjern jordens tyngdekraft (≈1g)
  float dynamicAccel = abs(totalG - 1.0);

  // Start playback når bump registreres
  if (!playing &&
      dynamicAccel > BUMP_THRESHOLD_G &&
      (now - lastTriggerTime > COOLDOWN_MS)) {

    Serial.println("BUMP detected - Starting AAC playback");
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
}
