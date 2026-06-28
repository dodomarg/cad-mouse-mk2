#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <USB.h>
#include <USBHID.h>
#else
#include <Adafruit_TinyUSB.h>
#endif

class HIDController
#if defined(ARDUINO_ARCH_ESP32)
    : public USBHIDDevice
#endif
{
 public:
  HIDController();
  void begin();
  void task();
  bool sendReports(const float motion[6], uint16_t buttonBits);

#if defined(ARDUINO_ARCH_ESP32)
  // USBHIDDevice interface: supply the custom 6DoF report descriptor.
  uint16_t _onGetDescriptor(uint8_t* buffer) override;
#endif

 private:
  struct __attribute__((packed)) ReportAxes {
    int16_t x, y, z, rx, ry, rz;
  };

  struct __attribute__((packed)) ReportButtons {
    uint16_t bits;
  };

  static ReportAxes makeAxesReport(const float motion[6]);
  bool axesReportChanged(const ReportAxes& axes) const;

#if defined(ARDUINO_ARCH_ESP32)
  USBHID usbHid_;
#else
  Adafruit_USBD_HID usbHid_;
#endif
  uint16_t buttonBitsSent_ = 0;
  ReportAxes lastSentAxes_{};
};
