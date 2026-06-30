#include "Settings.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>

namespace {
const char* kNamespace = "cadmouse";
const char* kKeyVid = "vid";
const char* kKeyPid = "pid";
const char* kKeyCal = "cal";
}  // namespace

void Settings::begin() {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);

  vid_ = prefs.getUShort(kKeyVid, kDefaultVid);
  pid_ = prefs.getUShort(kKeyPid, kDefaultPid);

  AxisCalibration cal;
  const size_t read = prefs.getBytes(kKeyCal, &cal, sizeof(cal));
  if (read == sizeof(cal)) {
    cal_ = cal;
  }

  prefs.end();
}

void Settings::save() {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);

  prefs.putUShort(kKeyVid, vid_);
  prefs.putUShort(kKeyPid, pid_);
  prefs.putBytes(kKeyCal, &cal_, sizeof(cal_));

  prefs.end();
}

void Settings::resetDefaults() {
  vid_ = kDefaultVid;
  pid_ = kDefaultPid;
  cal_ = AxisCalibration{};
  save();
}

#else  // Non-ESP32 (e.g. RP2040): no NVS backend, keep values in RAM only.

void Settings::begin() {}
void Settings::save() {}
void Settings::resetDefaults() {
  vid_ = kDefaultVid;
  pid_ = kDefaultPid;
  cal_ = AxisCalibration{};
}

#endif
