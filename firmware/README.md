All motion tuning parameters — magnet-plane geometry, gains, sign fixes, dead
zones, smoothing and the final axis limit — are runtime-configurable and
stored in NVS (`Settings::MotionParams`, `firmware/include/Settings.h`). On
ESP32-S3, hold both buttons at power-up to enter the configuration portal
(see below) and edit them from a browser; no reflash needed. The values in
`MotionParams`'s default member initializers are the fallback used on a
freshly flashed, never-configured device and match the original tuning:

```cpp
// firmware/include/Settings.h
float radiusMm = 16.5f;
float zOffsetMm = 19.8637f;

float gainT[3] = {28.0f, 28.0f, 24.0f};
float gainR[3] = {18.0f, 18.0f, 20.0f};
int8_t signAxis[6] = {-1, +1, -1, +1, +1, +1};

float deadT = 16.0f;
float deadR = 20.0f;

float smoothTauS = 0.08f;
float axisLimit = 350.0f;
```

⚠️ Refer to the video at [6:23](https://youtu.be/62xlzGs8LXA?si=ld2shDCaTxOLIGB8&t=383) for a demo of driver support. Related settings can be found commented in[`platformio.ini`](../platformio.ini).

⚠️ As mentioned in the video, the motion processing still needs work and may eventually be replaced entirely. This is beyond me for now, so contributions and improvements are welcome.

This implementation still assumes the readings from the sensors are linear, which is not true over the full travel range (the per-axis calibration in the config portal partially compensates for this).

If you want to experiment with different motion processing approaches, you can modify the [`MotionController`](src/controllers/MotionController.cpp). The current implementation is as follows:

**Sensor/magnet geometry:**

The three hall sensors (and the magnets above them) sit on their own shared,
parallel planes, 120&deg; apart around a circle of radius `radiusMm`:
- `mag1` = negative Y axis: `(0, -R)`
- `mag2` = second quadrant: `(-R*sqrt(3)/2, R/2)`
- `mag3` = first quadrant: `(+R*sqrt(3)/2, R/2)`

The magnet plane is offset from the (non-rotated, parallel) sensor plane
along the shared axis by `zOffsetMm`. Because the layout is exactly
120&deg;-symmetric, only the radius and Z offset matter — the per-magnet
angle is fixed by the symmetry, so there's no need to enter it separately.

**Rigid-body model:**

For small translations `T=(Tx,Ty,Tz)` and rotations `R=(Rx,Ry,Rz)` of the
magnet plane about the shared axis' origin, the displacement measured above
magnet `i` at position `(xi, yi, z0)` is approximately:

```
dxi = Tx + Ry*z0 - Rz*yi
dyi = Ty + Rz*xi - Rx*z0
dzi = Tz + Rx*yi - Ry*xi
```

With the symmetric 120&deg; layout (`sum(xi) = sum(yi) = 0`, and every point
at distance `R` from the axis so `xi^2+yi^2 = R^2`), these invert to closed
form without a general least-squares solve:

```
Tz = (mag1z + mag2z + mag3z) / 3
Rx = (mag2z + mag3z - 2*mag1z) / (3*R)
Ry = (mag2z - mag3z) / (R*sqrt(3))

Rz = sum_i (posXi * magYi - posYi * magXi) / (3*R^2)

Tx = avg(magXi) - Ry*Z0
Ty = avg(magYi) + Rx*Z0
```

- `Tz`, `Rx`, `Ry` come purely from the three Z (out-of-plane) readings.
- `Rz` (twist) comes from the in-plane X/Y readings; the translation and
  Z-offset cross-terms cancel out of the sum because the sensor positions
  are symmetric around the axis.
- `Tx`, `Ty` start from the average in-plane reading and are then explicitly
  decoupled from the rotation-induced offset introduced by the Z0 plane
  separation — this is what reduces axis bleed compared to the previous
  independent-per-axis formulas.
- Sign fixes and gains (`signAxis`/`gainT`/`gainR`) are applied afterwards,
  same as before.

## Supported boards / building

The firmware targets the Seeed XIAO form factor and can be built for two boards
via [`platformio.ini`](../platformio.ini):

- `seeed_xiao_esp32s3` &mdash; Seeed XIAO ESP32-S3 (default). USB is routed
  through the TinyUSB USB-OTG stack (`ARDUINO_USB_MODE=0`) so the custom 6DoF
  HID descriptor is exposed, while the CDC serial port stays available for
  telemetry. Also the only target with the configuration portal (SoftAP +
  web UI), since it needs WiFi.
- `seeed_xiao_rp2040` &mdash; Seeed XIAO RP2040 (original board). No config
  portal; motion parameters stay at their compile-time defaults.

Because both XIAO boards share the same D-labelled pinout, the pin assignments
in [`Config.h`](include/Config.h) are identical for both targets.

```bash
# Build/upload the default ESP32-S3 target
pio run -t upload

# Build/upload the RP2040 target
pio run -e seeed_xiao_rp2040 -t upload
```
