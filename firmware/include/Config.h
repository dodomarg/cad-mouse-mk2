#pragma once

#include <Arduino.h>

namespace Config {

const bool ENABLE_TELEMETRY = true;

// Hardware pins (Seeed XIAO form factor: D-labelled pads are identical on the
// XIAO RP2040 and XIAO ESP32-S3, so the same constants work for both boards).
const int PIN_RIGHT_BTN = D0;
const int PIN_LEFT_BTN = D2;
const int PIN_LED_DATA = D3;
const int PIN_LED_LS = D1;
const int PIN_MAG1_LS = D10;
const int PIN_MAG2_LS = D9;
const int PIN_MAG3_LS = D8;

// Samples for calibration offset
const int ZERO_SAMPLES = 200;

// Motion model gains, geometry, dead zones, smoothing and axis limit are all
// runtime-configurable now (see Settings::MotionParams / the config portal's
// /api/motion endpoint). Their defaults live in MotionParams, not here.

// RGB LEDs
const int LED_COUNT = 8;
const int LED_BRIGHTNESS = 40;
const unsigned long LED_IDLE_COLOR = 0x00FF00;
const unsigned long LED_CALIBRATING_COLOR = 0x0000FF;
const unsigned long LED_CONFIG_COLOR = 0xFFFF00;

// Config portal (entered by holding both buttons at power-up). Open SoftAP
// serving a web UI for setting USB identity and running calibration.
const char* const CONFIG_AP_SSID = "CAD-Mouse-Config";

// FSM timing
const long IDLE_SLEEP_TIMEOUT_MS = 2 * 60 * 1000;

}  // namespace Config
