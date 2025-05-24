// pattern_player.h
#pragma once

class PatternPlayer {
public:
  PatternPlayer(int pin);  // Constructor with fixed pin
  void start(const char* p, int unit, bool restore = false);
  void update();
  void stop();
  bool isActive() const { return active; }

private:
  const char* pattern = nullptr;
  int unitTime = 0;
  int pin = -1;
  bool active = false;
  unsigned long lastChange = 0;
  int index = 0;
  bool state = false;
  bool restoreStateAfter = false;
  bool previousState = LOW;
};

// pattern_player.cpp
#include "pattern_player.h"
#include <Arduino.h>

PatternPlayer::PatternPlayer(int controlPin) {
  pin = controlPin;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void PatternPlayer::start(const char* p, int unit, bool restore) {
  pattern = p;
  unitTime = unit;
  restoreStateAfter = restore;
  active = true;
  lastChange = millis();
  index = 0;
  state = false;
  previousState = digitalRead(pin);
  digitalWrite(pin, LOW);
}

void PatternPlayer::stop() {
  active = false;
  digitalWrite(pin, restoreStateAfter ? previousState : LOW);
}

void PatternPlayer::update() {
  if (!active || !pattern[index]) return;

  unsigned long now = millis();

  if (!state) {
    if (pattern[index] == '.' || pattern[index] == '_') {
      digitalWrite(pin, HIGH);
    } else {
      digitalWrite(pin, LOW);
    }
    state = true;
    lastChange = now;
  } else {
    int duration = (pattern[index] == '.') ? unitTime :
                   (pattern[index] == '_') ? unitTime * 3 :
                   unitTime * 3;

    if (now - lastChange >= duration) {
      digitalWrite(pin, LOW);
      lastChange = now;
      index++;
      state = false;
    }
  }

  if (!pattern[index]) stop();
}
