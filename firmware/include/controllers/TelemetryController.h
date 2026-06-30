#pragma once

#include <Arduino.h>

class TelemetryController {
 public:
  void begin(Print& out);
  void publish(const float motion[6], int buttonBits, bool hidReportSent);
  bool enabled() const;

 private:
  Print* out_ = nullptr;
  int tick_ = 0;
};
