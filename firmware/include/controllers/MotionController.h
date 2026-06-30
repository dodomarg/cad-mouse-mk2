#pragma once

#include "Settings.h"

class MotionController {
 public:
  void reset();
  void compute(const float raw[9], const float* baseline, float dt, float out[6]);
  bool hasMotionActivity() const;

  // Computes the six signed, gain-scaled axis values from a raw sensor frame,
  // without dead-zone, filtering, clamping or calibration normalization. Used
  // both by compute() and by the config portal's full-range calibration.
  void mixAxes(const float raw[9], const float* baseline, float y[6]) const;

  // Installs the per-axis min/max calibration used to normalize output to the
  // full +/-AXIS_LIMIT range. When invalid, the raw gain-scaled values pass
  // through unchanged.
  void setCalibration(const AxisCalibration& cal) { cal_ = cal; }

 private:
  static float clampf(float v, float lo, float hi);
  static float hardZero(float v, float thr);
  static float lowpass(float prev, float x, float dt, float tau);
  static float axisBaseDead(int i);
  float normalizeAxis(int i, float y) const;
  float filt_[6] = {};
  bool motionActive_ = false;
  AxisCalibration cal_;
};
