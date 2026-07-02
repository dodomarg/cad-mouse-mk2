#include "controllers/MotionController.h"

#include <Arduino.h>
#include <math.h>

namespace {
enum RawIndex {
  RAW_MAG1_X = 0,
  RAW_MAG1_Y,
  RAW_MAG1_Z,
  RAW_MAG2_X,
  RAW_MAG2_Y,
  RAW_MAG2_Z,
  RAW_MAG3_X,
  RAW_MAG3_Y,
  RAW_MAG3_Z
};

enum AxisIndex {
  AXIS_TX = 0,
  AXIS_TY,
  AXIS_TZ,
  AXIS_RX,
  AXIS_RY,
  AXIS_RZ
};
}  // namespace

void MotionController::reset() {
  for (int i = 0; i < 6; i++) {
    filt_[i] = 0.0;
  }
  motionActive_ = false;
}

float MotionController::clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float MotionController::hardZero(float v, float thr) {
  return (fabs(v) < thr) ? 0.0 : v;
}

float MotionController::lowpass(float prev, float x, float dt, float tau) {
  if (tau <= 0.0) return x;
  const float a = dt / (tau + dt);
  return prev + a * (x - prev);
}

float MotionController::axisBaseDead(int i) const {
  return (i < 3) ? params_.deadT : params_.deadR;
}

float MotionController::normalizeAxis(int i, float y) const {
  if (!cal_.valid) {
    return y;
  }
  // Map the captured full-range extreme onto +/-AXIS_LIMIT, handling the
  // positive and negative travel independently so asymmetric ranges still
  // reach the full limit on each side.
  const float kEps = 1e-3f;
  if (y >= 0.0f) {
    const float hi = cal_.max[i];
    return (hi > kEps) ? (y / hi) * params_.axisLimit : y;
  }
  const float lo = cal_.min[i];
  return (lo < -kEps) ? (y / (-lo)) * params_.axisLimit : y;
}

void MotionController::mixAxes(const float raw[9], const float* baseline,
                               float y[6]) const {
  // Baseline subtraction converts magnetic deltas around the calibrated rest pose.
  const float mag1x = raw[RAW_MAG1_X] - baseline[RAW_MAG1_X];
  const float mag1y = raw[RAW_MAG1_Y] - baseline[RAW_MAG1_Y];
  const float mag1z = raw[RAW_MAG1_Z] - baseline[RAW_MAG1_Z];
  const float mag2x = raw[RAW_MAG2_X] - baseline[RAW_MAG2_X];
  const float mag2y = raw[RAW_MAG2_Y] - baseline[RAW_MAG2_Y];
  const float mag2z = raw[RAW_MAG2_Z] - baseline[RAW_MAG2_Z];
  const float mag3x = raw[RAW_MAG3_X] - baseline[RAW_MAG3_X];
  const float mag3y = raw[RAW_MAG3_Y] - baseline[RAW_MAG3_Y];
  const float mag3z = raw[RAW_MAG3_Z] - baseline[RAW_MAG3_Z];

  // Magnet-plane geometry: three points 120 degrees apart around a circle of
  // radius R, mag1 on the negative Y axis, mag2 in the second quadrant, mag3
  // in the first quadrant. The magnet plane sits a distance Z0 above the
  // (parallel, non-rotated) sensor plane along the shared axis.
  //
  //   mag1 = (0, -R)
  //   mag2 = (-R*sqrt(3)/2, R/2)
  //   mag3 = (+R*sqrt(3)/2, R/2)
  //
  // Guard against a degenerate zero radius (would make every formula below
  // divide by zero) by clamping to a tiny positive value.
  const float r = fmax(params_.radiusMm, 1e-3f);
  const float z0 = params_.zOffsetMm;

  const float mag1PosX = 0.0f;
  const float mag1PosY = -r;
  const float mag2PosX = -r * sqrt(3.0f) / 2.0f;
  const float mag2PosY = r / 2.0f;
  const float mag3PosX = r * sqrt(3.0f) / 2.0f;
  const float mag3PosY = r / 2.0f;

  // Rigid-body model: for small translations T=(Tx,Ty,Tz) and rotations
  // R=(Rx,Ry,Rz) of the magnet plane about the shared axis' origin, the
  // in-plane displacement measured above magnet i at position (xi, yi, z0)
  // is approximately:
  //   dxi = Tx + Ry*z0 - Rz*yi
  //   dyi = Ty + Rz*xi - Rx*z0
  //   dzi = Tz + Rx*yi - Ry*xi
  //
  // With the symmetric 120 degree layout (sum xi = sum yi = 0, and every
  // point at distance R from the axis so xi^2+yi^2 = R^2), these invert to
  // closed form below without needing a general least-squares solve.

  // Tz, Rx, Ry solved from the three Z (out-of-plane) readings only.
  const float tz = (mag1z + mag2z + mag3z) / 3.0f;
  const float rx = (mag2z + mag3z - 2.0f * mag1z) / (3.0f * r);
  const float ry = (mag2z - mag3z) / (r * sqrt(3.0f));

  // Rz solved from the in-plane (X/Y) readings; the Tx/Ty and Ry*z0/Rx*z0
  // cross-terms cancel in this sum because sum(xi) = sum(yi) = 0.
  const float swirlNum =
      (mag1PosX * mag1y - mag1PosY * mag1x) +
      (mag2PosX * mag2y - mag2PosY * mag2x) +
      (mag3PosX * mag3y - mag3PosY * mag3x);
  const float rz = swirlNum / (3.0f * r * r);

  // Tx, Ty solved from the average in-plane reading, then decoupled from the
  // rotation-induced offset introduced by the Z0 plane separation.
  const float avgXMeas = (mag1x + mag2x + mag3x) / 3.0f;
  const float avgYMeas = (mag1y + mag2y + mag3y) / 3.0f;
  const float tx = avgXMeas - ry * z0;
  const float ty = avgYMeas + rx * z0;

  // Apply sign fixes and gains
  y[AXIS_TX] = params_.signAxis[AXIS_TX] * tx * params_.gainT[AXIS_TX];
  y[AXIS_TY] = params_.signAxis[AXIS_TY] * ty * params_.gainT[AXIS_TY];
  y[AXIS_TZ] = params_.signAxis[AXIS_TZ] * tz * params_.gainT[AXIS_TZ];
  y[AXIS_RX] = params_.signAxis[AXIS_RX] * rx * params_.gainR[AXIS_RX - 3];
  y[AXIS_RY] = params_.signAxis[AXIS_RY] * ry * params_.gainR[AXIS_RY - 3];
  y[AXIS_RZ] = params_.signAxis[AXIS_RZ] * rz * params_.gainR[AXIS_RZ - 3];
}

void MotionController::compute(const float raw[9], const float* baseline, float dt,
                               float out[6]) {
  float y[6];
  mixAxes(raw, baseline, y);

  // Normalize against the stored full-range calibration before the dead-zone
  // and smoothing stages so thresholds apply in the +/-AXIS_LIMIT space.
  for (int i = 0; i < 6; i++) {
    y[i] = normalizeAxis(i, y[i]);
  }

  // Filter, clamp to range and dead zones.
  motionActive_ = false;
  for (int i = 0; i < 6; i++) {
    const float dead = axisBaseDead(i);

    if (fabs(y[i]) < dead) {
      filt_[i] = 0.0;
    } else {
      filt_[i] = lowpass(filt_[i], y[i], dt, params_.smoothTauS);
    }

    const float limited =
        clampf(filt_[i], -params_.axisLimit, params_.axisLimit);
    out[i] = hardZero(limited, dead);
    if (out[i] != 0.0) {
      motionActive_ = true;
    }
  }
}

bool MotionController::hasMotionActivity() const { return motionActive_; }
