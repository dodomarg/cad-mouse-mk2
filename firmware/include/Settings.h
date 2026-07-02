#pragma once

#include <Arduino.h>

// Persistent, NVS-backed device settings (USB identity, axis calibration and
// motion model parameters).
//
// Values are cached in RAM after begin() and only written back to flash on
// save(), so reads are cheap and writes are explicit. Defaults match the
// previous compile-time behaviour (3Dconnexion SpaceMouse Compact identity,
// same gains/geometry as the original Config.h constants) so a freshly
// flashed, never-configured device behaves as before.

struct AxisCalibration {
  bool valid = false;
  // Per-axis extremes captured while the user moves the device through its
  // full range of motion. Order: Tx, Ty, Tz, Rx, Ry, Rz.
  float min[6] = {0, 0, 0, 0, 0, 0};
  float max[6] = {0, 0, 0, 0, 0, 0};
};

// Runtime-tunable parameters of the magnet-plane motion model.
//
// Geometry: the three hall sensors (and the three magnets above them) sit on
// their own shared plane, spaced 120 degrees apart around a circle of radius
// `radiusMm`, with mag1 on the negative Y axis, mag2 in the second quadrant
// and mag3 in the first quadrant. The magnet plane is offset from the
// sensors' shared plane along Z by `zOffsetMm`. Only the radius and Z offset
// are needed; the 120 degree symmetry fixes every magnet's angle.
struct MotionParams {
  // Magnet-plane geometry (millimetres).
  float radiusMm = 16.5f;
  float zOffsetMm = 19.8637f;

  // Gains applied after the geometric mix, before dead-zone/smoothing.
  // Order: Tx, Ty, Tz / Rx, Ry, Rz.
  float gainT[3] = {28.0f, 28.0f, 24.0f};
  float gainR[3] = {18.0f, 18.0f, 20.0f};

  // Per-axis sign flips. Order: Tx, Ty, Tz, Rx, Ry, Rz.
  int8_t signAxis[6] = {-1, +1, -1, +1, +1, +1};

  // Dead zones (applied before smoothing, in output units).
  float deadT = 16.0f;
  float deadR = 20.0f;

  // Low-pass smoothing time constant, seconds.
  float smoothTauS = 0.08f;

  // Final clamp applied to every axis (output units, e.g. HID report range).
  float axisLimit = 350.0f;
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

  const MotionParams& motion() const { return motion_; }
  void setMotion(const MotionParams& m) { motion_ = m; }

  // Persists the current cache to NVS.
  void save();

  // Clears stored values back to compile-time defaults (and persists).
  void resetDefaults();

 private:
  uint16_t vid_ = kDefaultVid;
  uint16_t pid_ = kDefaultPid;
  AxisCalibration cal_;
  MotionParams motion_;
};
