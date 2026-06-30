#include "controllers/TelemetryController.h"
#include <Arduino.h>
#include "Config.h"

namespace {
const int kPrintEvery = 5;
}

void TelemetryController::begin(Print& out) {
  out_ = &out;
  tick_ = 0;
}

bool TelemetryController::enabled() const { return Config::ENABLE_TELEMETRY; }

void TelemetryController::publish(const float motion[6], int buttonBits,
                                  bool hidReportSent) {
  if (!enabled() || out_ == nullptr) {
    return;
  }

  tick_++;
  if ((tick_ % kPrintEvery) != 0) {
    return;
  }

  out_->print(">X:");
  out_->println(motion[0]);
  out_->print(">Y:");
  out_->println(motion[1]);
  out_->print(">Z:");
  out_->println(motion[2]);
  out_->print(">Rx:");
  out_->println(motion[3]);
  out_->print(">Ry:");
  out_->println(motion[4]);
  out_->print(">Rz:");
  out_->println(motion[5]);
  out_->print(">btn:");
  out_->println(buttonBits & 0x0003);
  out_->print(">hid:");
  out_->println(hidReportSent ? 1 : 0);
}
