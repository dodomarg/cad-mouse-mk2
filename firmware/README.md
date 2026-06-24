You can customize several variables to tune gains, smoothing, and deadzones for all six axes.
Most of these settings are defined in [`Config.h`](include/Config.h) and are the main place to adjust the overall feel of the device.

```cpp
// Gains and sign fixes
const float GAIN_T[3] = {28.0, 28.0, 24.0};
const float GAIN_R[3] = {18.0, 18.0, 20.0};
const int SIGN_AXIS[6] = {-1, +1, -1, +1, +1, +1};

// Dead zones
const float DEAD_T = 16.0;
const float DEAD_R = 20.0;

// Smoothing
const float SMOOTH_TAU_S = 0.08;
```

⚠️ Refer to the video at [6:23](https://youtu.be/62xlzGs8LXA?si=ld2shDCaTxOLIGB8&t=383) for a demo of driver support. Related settings can be found commented in[`platformio.ini`](../platformio.ini).

⚠️ As mentioned in the video, the motion processing still needs work and may eventually be replaced entirely. This is beyond me for now, so contributions and improvements are welcome.

There is bleed between the axis due to the way each of them is calculated independently. This implementation also assumes the readings from the sensors are linear, which is not true.

If you want to experiment with different motion processing approaches, you can modify the [`MotionController`](src/controllers/MotionController.cpp). The current implementation is as follows:

**Sensor layout:**
- `mag1` = bottom
- `mag2` = top left
- `mag3` = top right

```
Tx = (mag1x + mag2x + mag3x) / 3
Ty = (mag1y + mag2y + mag3y) / 3
Tz = (mag1z + mag2z + mag3z) / 3

Rx = sqrt(3) * (mag2z + mag3z - 2 * mag1z) / 3
Ry = mag3z - mag2z
Rz = sum_i (posXi * magYi - posYi * magXi)
```
- `Tx`, `Ty`, `Tz`: average of all three sensors
- `Ry`: left-right z difference across the top edge
- `Rx`: top pair versus bottom, scaled for the triangle geometry
- `Rz`: twist estimate from the x/y sensor positions

## Supported boards / building

The firmware targets the Seeed XIAO form factor and can be built for two boards
via [`platformio.ini`](../platformio.ini):

- `seeed_xiao_esp32s3` &mdash; Seeed XIAO ESP32-S3 (default). USB is routed
  through the TinyUSB USB-OTG stack (`ARDUINO_USB_MODE=0`) so the custom 6DoF
  HID descriptor is exposed, while the CDC serial port stays available for
  telemetry.
- `seeed_xiao_rp2040` &mdash; Seeed XIAO RP2040 (original board).

Because both XIAO boards share the same D-labelled pinout, the pin assignments
in [`Config.h`](include/Config.h) are identical for both targets.

```bash
# Build/upload the default ESP32-S3 target
pio run -t upload

# Build/upload the RP2040 target
pio run -e seeed_xiao_rp2040 -t upload
```
