
#pragma once

class BackLight {
  static BackLight _instance;
  int _ledPin = -1;
  bool _on = false;
  bool _timerActive = false;

  public:
  BackLight();
  static void setPin(int ledPin);
  static bool isOn();
  static void turnOn();
  static void activateTimer();
  static void turnOff();

  private:
  static void triggerTimer();
};
