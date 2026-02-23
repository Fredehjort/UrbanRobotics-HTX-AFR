/* --------------------------------------------------------------
   Arduino + ESP‑Audio + BMI088 bump detector
   -------------------------------------------------------------- */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "AudioGeneratorAAC.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioOutputI2S.h"
#include "AudioLogger.h"          // declares `extern AudioLogger *audioLogger;`

#include "sampleaac.h"

/* ==================== AUDIO ==================== */

AudioFileSourcePROGMEM *in   = nullptr;
AudioGeneratorAAC      *aac  = nullptr;
AudioOutputI2S         *out  = nullptr;

#define I2S_BCLK  16
#define I2S_LRCLK 17
#define I2S_DIN   21

/* ==================== BMI088 ==================== */

#define BMI088_ACC_ADDRESS 0x18

/* Register map (only the ones we need) */
#define ACC_PWR_CTRL   0x7D   // power control
#define ACC_PWR_CONF   0x7C   // power configuration
#define ACC_RANGE      0x41   // range setting

/* ==================== BUMP DETECTION ==================== */

#define SAMPLE_INTERVAL_MS  10
#define BUMP_THRESHOLD_G    0.5f
#define COOLDOWN_MS         2000   // 2 s between bumps

unsigned long lastTriggerTime = 0;
unsigned long lastSample      = 0;

float lastAx = 0.0f, lastAy = 0.0f, lastAz = 0.0f;
bool  playing = false;

/* --------------------------------------------------------------
   Helper: read a 16‑bit signed axis from the BMI088
   -------------------------------------------------------------- */
int16_t readAccelAxis(uint8_t regL) {
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(regL);
  Wire.endTransmission(false);          // repeated start
  Wire.requestFrom(BMI088_ACC_ADDRESS, (uint8_t)2);

  if (Wire.available() < 2) return 0;    // <-- semicolon added

  uint8_t l = Wire.read();               // low byte
  uint8_t h = Wire.read();               // high byte
  return (int16_t)((h << 8) | l);
}

/* --------------------------------------------------------------
   Playback control
   -------------------------------------------------------------- */
void startPlayback() {
  if (!out) return;                      // safety guard
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

/* --------------------------------------------------------------
   Arduino setup()
   -------------------------------------------------------------- */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();                          // default SDA/SCL pins

  /* ---- BMI088 initialisation ---- */

  // Power on the accelerometer
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_PWR_CTRL);
  Wire.write(0x04);                      // normal mode
  Wire.endTransmission();
  delay(50);                             // <-- semicolon added

  // Enable normal power configuration
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_PWR_CONF);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  // Set measurement range to ±6 g (0x01)
  Wire.beginTransmission(BMI088_ACC_ADDRESS);
  Wire.write(ACC_RANGE);
  Wire.write(0x01);
  Wire.endTransmission();
  delay(50);                             // <-- semicolon added

  /* ---- Audio initialisation ---- */

  audioLogger = &Serial;                 // now the logger is known

  out = new AudioOutputI2S(0, 1);         // I2S peripheral 0, channel 1
  out->SetPinout(I2S_BCLK, I2S_LRCLK, I2S_DIN);
  out->SetGain(0.3f);

  Serial.println("BMI088 bump detector ready :)");
}

/* --------------------------------------------------------------
   Main loop – read accelerometer, detect bump, play sound
   -------------------------------------------------------------- */
void loop() {
  unsigned long now = millis();

  /* ---- Sample at the requested interval (optional) ---- */
  if (now - lastSample < SAMPLE_INTERVAL_MS) {
    return;                              // skip this iteration
  }
  lastSample = now;

  /* ---- Read raw acceleration data ---- */
  int16_t ax_raw = readAccelAxis(0x12);
  int16_t ay_raw = readAccelAxis(0x14);
  int16_t az_raw = readAccelAxis(0x16);

  // Convert to g (±6 g → 0.000183 g per LSB)
  float ax = ax_raw * 0.000183f;
  float ay = ay_raw * 0.000183f;
  float az = az_raw * 0.000183f;

  /* ---- Simple bump detection ---- */
  float dx = ax - lastAx;
  float dy = ay - lastAy;
  float dz = az - lastAz;

  float shake   = sqrt(dx * dx + dy * dy + dz * dz);
  float totalG  = sqrt(ax * ax + ay * ay + az * az);

  lastAx = ax;
  lastAy = ay;
  lastAz = az;

  if (!playing &&
      totalG > BUMP_THRESHOLD_G &&
      (now - lastTriggerTime > COOLDOWN_MS)) {

    Serial.print("Shake: ");
    Serial.println(shake, 3);
    startPlayback();
    lastTriggerTime = now;
  }

  /* ---- Keep the AAC decoder running ---- */
  if (playing && aac) {
    if (aac->isRunning()) {
      aac->loop();
    } else {
      Serial.println("Playback finished");
      stopPlayback();
    }
  }

  // No extra delay – the sampling interval is already enforced above
}
