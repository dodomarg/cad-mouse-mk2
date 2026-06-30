#pragma once

#include <Arduino.h>

// Persistent, NVS-backed device settings (USB identity + axis calibration).
//
// Values are cached in RAM after begin() and only written back to flash on
// save(), so reads are cheap and writes are explicit. Defaults match the
// previous compile-time behaviour (3Dconnexion SpaceMouse Compact identity)
// so a freshly flashed, never-configured device behaves as before.

struct AxisCalibration {
  bool valid = false;
  // Per-axis extremes captured while the user moves the device through its
  // full range of motion. Order: Tx, Ty, Tz, Rx, Ry, Rz.
  float min[6] = {0, 0, 0, 0, 0, 0};
  float max[6] = {0, 0, 0, 0, 0, 0};
};

class Settings {
 public:
  // Default USB identity: 3Dconnexion SpaceMouse Compact (256f:c635).
  static constexpr uint16_t kDefaultVid = 0x256f;
  static constexpr uint16_t kDefaultPid = 0xc635;

  // Loads cached values from NVS (applying defaults for missing keys).
  void begin();

  uint16_t vid() const { return vid_; }
  uint16_t pid() const { return pid_; }
  void setVid(uint16_t v) { vid_ = v; }
  void setPid(uint16_t v) { pid_ = v; }

  const AxisCalibration& calibration() const { return cal_; }
  void setCalibration(const AxisCalibration& c) { cal_ = c; }

  // Persists the current cache to NVS.
  void save();

  // Clears stored values back to compile-time defaults (and persists).
  void resetDefaults();

 private:
  uint16_t vid_ = kDefaultVid;
  uint16_t pid_ = kDefaultPid;
  AxisCalibration cal_;
};
