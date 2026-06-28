// TLx493D address-assignment + read diagnostic.
//
// Replicates the EXACT init / I2C-address-reassignment sequence used by the
// real SensorController, reports whether each setIICAddress() call succeeds,
// re-scans the bus to show which addresses are actually present, then reads
// the magnetic field from each sensor.
//
// Results over USB-CDC serial (115200) and the LED ring (slot 0=MAG1,
// 1=MAG2, 2=MAG3):
//     GREEN  -> read returned a sane (non-zero) field magnitude
//     YELLOW -> read "ok" but field ~0 (suspicious)
//     RED    -> read failed
//
// A healthy sensor reads Earth's field (~0.025-0.065 mT) even with no
// reference magnet, so all-zero / failed reads point at the init sequence.
//
// Flash:   pio run -e diag_i2c -t upload
// Restore: pio run -e seeed_xiao_esp32s3 -t upload

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Wire.h>

#include "Config.h"
#include "TLx493D_inc.hpp"

using namespace ifx::tlx493d;

namespace {

Adafruit_NeoPixel ring(Config::LED_COUNT, Config::PIN_LED_DATA,
                       NEO_GRB + NEO_KHZ800);

TLx493D_A2B6 mag1(Wire, TLx493D_IIC_ADDR_A0_e);
TLx493D_A2B6 mag2(Wire, TLx493D_IIC_ADDR_A0_e);
TLx493D_A2B6 mag3(Wire, TLx493D_IIC_ADDR_A0_e);

// AP22817B load switches are active-low (EN low = on, high = off).
void powerOn(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(5);
}
void powerOff(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

void scanBus(const char* tag) {
  Serial.print("  bus scan ");
  Serial.print(tag);
  Serial.print(":");
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(" 0x");
      Serial.print(addr, HEX);
    }
  }
  Serial.println();
}

}  // namespace

void setup() {
  Serial.begin(115200);

  powerOn(Config::PIN_LED_LS);
  ring.begin();
  ring.setBrightness(Config::LED_BRIGHTNESS);
  for (int i = 0; i < Config::LED_COUNT; ++i)
    ring.setPixelColor(i, ring.Color(0, 0, 60));
  ring.show();

  powerOff(Config::PIN_MAG1_LS);
  powerOff(Config::PIN_MAG2_LS);
  powerOff(Config::PIN_MAG3_LS);
  delay(5);

  Wire.begin();
  Wire.setClock(400000);

  Serial.println("\n==== TLx493D init sequence (mirrors SensorController) ====");

  // --- MAG1: power, begin, move to A2 (0x78) ---
  powerOn(Config::PIN_MAG1_LS);
  bool b1 = mag1.begin(true, false, false, true);
  scanBus("after MAG1 begin");
  bool a1 = mag1.setIICAddress(TLx493D_IIC_ADDR_A2_e);
  mag1.setSensitivity(TLx493D_EXTRA_SHORT_RANGE_e);
  delay(10);
  scanBus("after MAG1 ->A2");
  Serial.print("  MAG1 begin="); Serial.print(b1);
  Serial.print(" setAddrA2(0x78)="); Serial.println(a1);

  // --- MAG2: power, begin, move to A1 (0x22) ---
  powerOn(Config::PIN_MAG2_LS);
  bool b2 = mag2.begin(true, false, false, true);
  scanBus("after MAG2 begin");
  bool a2 = mag2.setIICAddress(TLx493D_IIC_ADDR_A1_e);
  mag2.setSensitivity(TLx493D_EXTRA_SHORT_RANGE_e);
  delay(10);
  scanBus("after MAG2 ->A1");
  Serial.print("  MAG2 begin="); Serial.print(b2);
  Serial.print(" setAddrA1(0x22)="); Serial.println(a2);

  // --- MAG3: power, begin, stay at A0 (0x35) ---
  powerOn(Config::PIN_MAG3_LS);
  bool b3 = mag3.begin(true, false, false, true);
  mag3.setSensitivity(TLx493D_EXTRA_SHORT_RANGE_e);
  delay(10);
  scanBus("after MAG3 begin");
  Serial.print("  MAG3 begin="); Serial.println(b3);

  Serial.println("  expected final bus: 0x22 (MAG2) 0x35 (MAG3) 0x78 (MAG1)");
}

void loop() {
  TLx493D_A2B6* sensors[3] = {&mag1, &mag2, &mag3};
  const char* names[3] = {"MAG1(0x78)", "MAG2(0x22)", "MAG3(0x35)"};

  Serial.println("---- field read ----");
  for (int i = 0; i < 3; ++i) {
    double x = 0, y = 0, z = 0, t = 0;
    bool ok = sensors[i]->getMagneticFieldAndTemperature(&x, &y, &z, &t);
    double mag = sqrt(x * x + y * y + z * z);

    Serial.print("  ");
    Serial.print(names[i]);
    Serial.print(": ok="); Serial.print(ok);
    Serial.print(" x="); Serial.print(x, 4);
    Serial.print(" y="); Serial.print(y, 4);
    Serial.print(" z="); Serial.print(z, 4);
    Serial.print(" |B|="); Serial.print(mag, 4);
    Serial.print(" mT  T="); Serial.println(t, 1);

    uint32_t color;
    if (!ok) {
      color = ring.Color(60, 0, 0);    // red: failed
    } else if (mag < 0.005) {
      color = ring.Color(50, 35, 0);   // yellow: ~zero field (suspicious)
    } else {
      color = ring.Color(0, 60, 0);    // green: real reading
    }
    ring.setPixelColor(i, color);
  }
  for (int i = 3; i < Config::LED_COUNT; ++i) ring.setPixelColor(i, 0);
  ring.show();

  delay(1000);
}
