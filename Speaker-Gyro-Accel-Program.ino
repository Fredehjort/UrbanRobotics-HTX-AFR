#include <Arduino.h>

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

const unsigned long PLAY_INTERVAL = 15000; // 15 seconds
unsigned long lastPlayTime = 0;
bool playing = false;

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

  audioLogger = &Serial;

  out = new AudioOutputI2S(0, 1);
  out->SetPinout(I2S_BCLK, I2S_LRCLK, I2S_DIN);
  out->SetGain(0.3);

  lastPlayTime = millis() - PLAY_INTERVAL; // play immediately
}

void loop() {
  unsigned long now = millis();

  // Start playback every 15 seconds
  if (!playing && (now - lastPlayTime >= PLAY_INTERVAL)) {
    Serial.println("Starting AAC playback");
    lastPlayTime = now;
    startPlayback();
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
