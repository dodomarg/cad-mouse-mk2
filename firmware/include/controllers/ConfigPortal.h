#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#endif

class Settings;
class SensorController;
class MotionController;

// On-device configuration portal. When the user holds both buttons at power-up
// the firmware starts an open SoftAP and this HTTP server instead of the HID
// pipeline, letting a phone or PC browser set the USB identity, tune the
// motion model (magnet-plane geometry, gains, dead zones, smoothing) and run
// full-range calibration. No host-side software or drivers required.
//
// ESP32-only feature; on other targets the methods compile to no-ops.
class ConfigPortal {
 public:
  ConfigPortal(Settings& settings, SensorController& sensors,
               MotionController& motion);

  void begin();   // Start sensors, SoftAP + HTTP server.
  void update();  // Service HTTP requests + sample calibration (call from loop()).

 private:
#if defined(ARDUINO_ARCH_ESP32)
  void handleRoot();
  void handleGetSettings();
  void handlePostSettings();
  void handleGetMotion();
  void handlePostMotion();
  void handleMotionReset();
  void handleLive();
  void handleCalZero();
  void handleCalStart();
  void handleCalStop();
  void handleCalClear();
  void handleNotFound();

  void captureBaseline();
  void readAxes(float axes[6]);

  WebServer server_;
  bool capturing_ = false;
  float capMin_[6] = {0, 0, 0, 0, 0, 0};
  float capMax_[6] = {0, 0, 0, 0, 0, 0};
  unsigned long lastSampleMs_ = 0;
#endif
  Settings& settings_;
  SensorController& sensors_;
  MotionController& motion_;
};
