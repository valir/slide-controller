
#pragma once
#include <esp32-hal-ledc.h>

class Buzzer {
  uint8_t _pin = 0;

  public:
  Buzzer() { }
  void init();
  void tone(note_t, uint8_t octave, uint16_t dur);
  void doorbell();
  void startAlert();
  void stopAlert();
  void ackTone();
  void playStartup();
  void playOtaStart();
  void playOtaFailed();
  void playOtaDownloading();
  void swipeDetectedTone();
};

extern Buzzer buzzer;
