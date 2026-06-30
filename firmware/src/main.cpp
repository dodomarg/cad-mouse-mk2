#include <Arduino.h>

#include "Config.h"
#include "Controllers.h"
#include "Settings.h"
#include "StateMachine.h"
#include "controllers/ConfigPortal.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <USBCDC.h>
// With ARDUINO_USB_CDC_ON_BOOT=0 the core's CDC "Serial" is the USB-Serial-JTAG
// (HWCDC), which cannot enumerate while the USB PHY is routed to OTG/TinyUSB.
// Constructing this USBCDC registers a TinyUSB CDC interface, giving a working
// telemetry port composited alongside the HID interface.
USBCDC telemetrySerial(0);
#define TELEMETRY_SERIAL telemetrySerial
#else
#define TELEMETRY_SERIAL Serial
#endif

InputController inputController;
LEDController ledController;
SensorController sensorController;
MotionController motionController;
HIDController hidController;
TelemetryController telemetryController;
Settings settings;
ConfigPortal configPortal(settings, sensorController, motionController);

namespace {
bool configMode = false;

// Reads the two buttons directly (active-low with internal pull-ups) to decide
// whether to enter the configuration portal at power-up.
bool bothButtonsHeld() {
  pinMode(Config::PIN_LEFT_BTN, INPUT_PULLUP);
  pinMode(Config::PIN_RIGHT_BTN, INPUT_PULLUP);
  delay(10);
  return digitalRead(Config::PIN_LEFT_BTN) == LOW &&
         digitalRead(Config::PIN_RIGHT_BTN) == LOW;
}
}  // namespace

void setup() {
  settings.begin();

  if (bothButtonsHeld()) {
    // Configuration mode: skip the HID pipeline, show a solid yellow ring and
    // bring up the SoftAP + web UI.
    configMode = true;
    ledController.begin();
    ledController.setSolid(Config::LED_CONFIG_COLOR);
    configPortal.begin();
    return;
  }

  // Initialize USB HID first, applying the stored USB identity from NVS.
  hidController.begin(settings.vid(), settings.pid());

  if (Config::ENABLE_TELEMETRY) {
    TELEMETRY_SERIAL.begin(115200);
    delay(200);
  }

  inputController.begin();
  ledController.begin();
  sensorController.begin();
  motionController.reset();
  motionController.setCalibration(settings.calibration());
  telemetryController.begin(TELEMETRY_SERIAL);

  stateMachine.changeState(&StateMachine::calibratingState);
}

void loop() {
  if (configMode) {
    configPortal.update();
    return;
  }

  hidController.task();
  stateMachine.update();
}
