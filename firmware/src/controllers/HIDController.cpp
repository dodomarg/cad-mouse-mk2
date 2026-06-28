#include "controllers/HIDController.h"

#include <math.h>

#include "Config.h"

namespace {
const uint8_t kHidReportDescriptor[] PROGMEM = {
    0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
    0x09, 0x08,        // USAGE (Multi-axis Controller)
    0xA1, 0x01,        // COLLECTION (Application)
    0xA1, 0x00,        // COLLECTION (Physical)
    0x85, 0x01,        // REPORT_ID (1)
    0x16, 0xA2, 0xFE,  // LOGICAL_MINIMUM  (-350)
    0x26, 0x5E, 0x01,  // LOGICAL_MAXIMUM  (350)
    0x09, 0x30,        // USAGE (X)     
    0x09, 0x31,        // USAGE (Y)
    0x09, 0x32,        // USAGE (Z)
    0x09, 0x33,        // USAGE (Rx)
    0x09, 0x34,        // USAGE (Ry)
    0x09, 0x35,        // USAGE (Rz)
    0x75, 0x10,        // REPORT_SIZE (16)
    0x95, 0x06,        // REPORT_COUNT (6)
    0x81, 0x02,        // INPUT (Data,Var,Abs)
    0xC0,              // END_COLLECTION
    0xA1, 0x00,        // COLLECTION (Physical)
    0x85, 0x03,        // REPORT_ID (3)
    0x05, 0x09,        // USAGE_PAGE (Button)
    0x19, 0x01,        // USAGE_MINIMUM (Button 1)
    0x29, 0x02,        // USAGE_MAXIMUM (Button 2)
    0x15, 0x00,        // LOGICAL_MINIMUM (0)
    0x25, 0x01,        // LOGICAL_MAXIMUM (1)
    0x75, 0x01,        // REPORT_SIZE (1)
    0x95, 0x02,        // REPORT_COUNT (2)
    0x81, 0x02,        // INPUT (Data,Var,Abs)
    0x95, 0x0E,        // REPORT_COUNT (14) padding
    0x81, 0x01,        // INPUT (Const,Array,Abs)
    0xC0,              // END_COLLECTION
    0xC0               // END_COLLECTION
};
}

#ifdef ARDUINO_ARCH_ESP32
HIDController::HIDController() {
  // The custom HID device must be registered before the USB stack assembles
  // its descriptors, so this has to happen at construction time rather than in
  // begin(); otherwise wDescriptorLength is reported as 0 and the host rejects
  // the HID interface (Linux: "can't add hid device: -22").
  static bool registered = false;
  if (!registered) {
    registered = true;
    usbHid_.addDevice(this, sizeof(kHidReportDescriptor));
  }
}

uint16_t HIDController::_onGetDescriptor(uint8_t* buffer) {
  memcpy(buffer, kHidReportDescriptor, sizeof(kHidReportDescriptor));
  return sizeof(kHidReportDescriptor);
}

void HIDController::begin() {
  // On the RP2040 the product string is provided via a build flag; the
  // arduino-esp32 core has no equivalent, so set it here for parity.
  USB.productName("CAD Mouse MK2");
  usbHid_.begin();
  USB.begin();
}

void HIDController::task() {}
#else
HIDController::HIDController() {}

void HIDController::begin() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  usbHid_.setReportDescriptor(kHidReportDescriptor, sizeof(kHidReportDescriptor));
  usbHid_.setPollInterval(1);
  usbHid_.begin();
}

void HIDController::task() { TinyUSBDevice.task(); }
#endif

HIDController::ReportAxes HIDController::makeAxesReport(const float motion[6]) {
  ReportAxes axes{};
  axes.x = static_cast<int16_t>(motion[0]);
  axes.y = static_cast<int16_t>(motion[1]);
  axes.z = static_cast<int16_t>(motion[2]);
  axes.rx = static_cast<int16_t>(motion[3]);
  axes.ry = static_cast<int16_t>(motion[4]);
  axes.rz = static_cast<int16_t>(motion[5]);
  return axes;
}

bool HIDController::axesReportChanged(const ReportAxes& axes) const {
  return axes.x != lastSentAxes_.x ||
         axes.y != lastSentAxes_.y ||
         axes.z != lastSentAxes_.z ||
         axes.rx != lastSentAxes_.rx ||
         axes.ry != lastSentAxes_.ry ||
         axes.rz != lastSentAxes_.rz;
}

bool HIDController::sendReports(const float motion[6], uint16_t buttonBits) {
  const ReportAxes axes = makeAxesReport(motion);
  const bool sendAxes = axesReportChanged(axes);
  const bool sendButtons = (buttonBits != buttonBitsSent_);

  if (!usbHid_.ready() || (!sendAxes && !sendButtons)) {
    return false;
  }

  if (sendAxes) {
#ifdef ARDUINO_ARCH_ESP32
    usbHid_.SendReport(0x01, &axes, sizeof(axes));
#else
    usbHid_.sendReport(0x01, &axes, sizeof(axes));
#endif
    lastSentAxes_ = axes;
  }

  if (sendButtons) {
    ReportButtons btn{};
    btn.bits = buttonBits & 0x0003;
#ifdef ARDUINO_ARCH_ESP32
    usbHid_.SendReport(0x03, &btn, sizeof(btn));
#else
    usbHid_.sendReport(0x03, &btn, sizeof(btn));
#endif
    buttonBitsSent_ = buttonBits;
  }

  return true;
}
