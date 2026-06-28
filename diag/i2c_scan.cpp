// I2C bus scanner / TLx493D presence diagnostic.
//
// Standalone firmware (built by the `diag_i2c` PlatformIO environment) used to
// confirm whether the three TLx493D magnetometers actually ACK on the I2C bus.
// Results are shown on the LED ring so no serial connection is required:
//
//   * Brief BLUE flash      -> a scan pass is starting.
//   * N GREEN LEDs lit      -> N devices ACKed on the bus this pass.
//   * All RED               -> nothing ACKed (bus/power/pull-up/solder fault).
//
// The same information is also printed over USB-CDC serial (115200) if a host
// terminal is attached with DTR asserted.
//
// Flash with:  pio run -e diag_i2c -t upload
// Restore the real firmware with:  pio run -e seeed_xiao_esp32s3 -t upload

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Wire.h>

#include "Config.h"

namespace {

Adafruit_NeoPixel ring(Config::LED_COUNT, Config::PIN_LED_DATA,
                       NEO_GRB + NEO_KHZ800);

// AP22817B load switches are active-low (EN low = powered on, high = off).
void powerOn(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void powerOff(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

// Returns true if any device ACKs anywhere on the bus.
bool busHasDevice() {
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) return true;
  }
  return false;
}

void showColor(uint32_t color) {
  for (int i = 0; i < Config::LED_COUNT; ++i) {
    ring.setPixelColor(i, color);
  }
  ring.show();
}

}  // namespace

void setup() {
  Serial.begin(115200);

  // Power the LED ring (its load switch is also active-low).
  powerOn(Config::PIN_LED_LS);
  ring.begin();
  ring.setBrightness(Config::LED_BRIGHTNESS);
  showColor(ring.Color(0, 0, 60));  // blue = booting

  // Start with all three magnetometer rails OFF so we can test them one at a
  // time. They all share the same default address (0x35), so powering them
  // together would collide on the bus and look like a single device.
  powerOff(Config::PIN_MAG1_LS);
  powerOff(Config::PIN_MAG2_LS);
  powerOff(Config::PIN_MAG3_LS);
  delay(50);

  // Use the XIAO default I2C pins (SDA=D4/GPIO5, SCL=D5/GPIO6). Scan slowly at
  // 100 kHz for maximum reliability.
  Wire.begin();
  Wire.setClock(100000);

  delay(500);
}

void loop() {
  // Signal the start of a test pass.
  showColor(ring.Color(0, 0, 60));
  delay(300);

  const int pins[3] = {Config::PIN_MAG1_LS, Config::PIN_MAG2_LS,
                       Config::PIN_MAG3_LS};
  const char* names[3] = {"MAG1 (D10)", "MAG2 (D9)", "MAG3 (D8)"};

  Serial.println("---- per-rail magnetometer test ----");
  int found = 0;
  bool present[3] = {false, false, false};
  for (int i = 0; i < 3; ++i) {
    // Power ONLY this sensor's rail.
    powerOff(pins[0]);
    powerOff(pins[1]);
    powerOff(pins[2]);
    delay(20);
    powerOn(pins[i]);
    delay(60);  // let the sensor boot

    present[i] = busHasDevice();
    if (present[i]) ++found;

    Serial.print("  ");
    Serial.print(names[i]);
    Serial.println(present[i] ? "  -> ACK (alive)" : "  -> no response");
  }
  Serial.print("Sensors responding: ");
  Serial.print(found);
  Serial.println(" / 3");

  // Show one LED per responding sensor: green = alive, red = dead, in fixed
  // slots so you can tell WHICH sensor is missing (LED0=MAG1, 1=MAG2, 2=MAG3).
  for (int i = 0; i < Config::LED_COUNT; ++i) {
    if (i < 3) {
      ring.setPixelColor(i, present[i] ? ring.Color(0, 60, 0)
                                       : ring.Color(60, 0, 0));
    } else {
      ring.setPixelColor(i, 0);
    }
  }
  ring.show();

  delay(2000);
}
