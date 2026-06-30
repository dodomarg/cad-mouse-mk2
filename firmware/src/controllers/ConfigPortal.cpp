#include "controllers/ConfigPortal.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>

#include "Config.h"
#include "Settings.h"
#include "controllers/MotionController.h"
#include "controllers/SensorController.h"

namespace {

// Self-contained config UI. Kept deliberately small and dependency-free.
const char kIndexHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CAD Mouse MK2 Config</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 0; padding: 1.5rem;
         background: #111; color: #eee; }
  h1 { font-size: 1.3rem; }
  section { background: #1c1c1c; border-radius: 10px; padding: 1rem 1.2rem;
            margin: 1rem 0; max-width: 30rem; }
  label { display: block; margin: 0.6rem 0 0.2rem; font-size: 0.9rem; color: #bbb; }
  input { width: 100%; box-sizing: border-box; padding: 0.5rem; font-size: 1rem;
          border-radius: 6px; border: 1px solid #444; background: #222; color: #eee; }
  button { margin-top: 1rem; padding: 0.6rem 1rem; font-size: 1rem; border: 0;
           border-radius: 6px; background: #f5c518; color: #111; font-weight: 600;
           cursor: pointer; }
  button.secondary { background: #333; color: #eee; }
  .row { display: flex; gap: 0.8rem; }
  .row > div { flex: 1; }
  .status { margin-top: 0.8rem; font-size: 0.9rem; min-height: 1.2em; }
  .muted { color: #888; font-size: 0.85rem; }
  table { width: 100%; margin-top: 0.8rem; border-collapse: collapse;
          font-variant-numeric: tabular-nums; font-size: 0.9rem; }
  th, td { text-align: right; padding: 0.25rem 0.4rem; border-bottom: 1px solid #2a2a2a; }
  th:first-child, td:first-child { text-align: left; color: #bbb; }
</style>
</head>
<body>
<h1>CAD Mouse MK2 &mdash; Configuration</h1>

<section>
  <h2 style="font-size:1.05rem">USB Identity</h2>
  <p class="muted">Hex values, e.g. 256f. Applied after reboot.</p>
  <div class="row">
    <div>
      <label for="vid">Vendor ID (VID)</label>
      <input id="vid" inputmode="latin" autocomplete="off" placeholder="256f">
    </div>
    <div>
      <label for="pid">Product ID (PID)</label>
      <input id="pid" inputmode="latin" autocomplete="off" placeholder="c635">
    </div>
  </div>
  <button onclick="saveSettings()">Save</button>
  <button class="secondary" onclick="reboot()">Save &amp; Reboot</button>
  <div class="status" id="status"></div>
</section>

<section>
  <h2 style="font-size:1.05rem">Calibration</h2>
  <p class="muted" id="calState">Loading&hellip;</p>
  <p class="muted">1. Rest the device, press <b>Set Zero</b>. 2. Press <b>Start</b>
     and move the cap through its full travel on every axis. 3. Press
     <b>Stop &amp; Save</b>.</p>
  <div class="row">
    <button class="secondary" onclick="calZero()">Set Zero</button>
    <button id="calBtn" onclick="calToggle()">Start</button>
    <button class="secondary" onclick="calClear()">Clear</button>
  </div>
  <table id="live"></table>
  <div class="status" id="calStatus"></div>
</section>

<script>
const AX = ['Tx','Ty','Tz','Rx','Ry','Rz'];
let capturing = false;
let timer = null;

async function load() {
  const r = await fetch('/api/settings');
  const s = await r.json();
  document.getElementById('vid').value = s.vid;
  document.getElementById('pid').value = s.pid;
  document.getElementById('calState').textContent =
    s.calValid ? 'Calibration data present.' : 'Not calibrated yet.';
}
function clean(v) { return v.trim().replace(/^0x/i, ''); }
async function postSettings() {
  const vid = clean(document.getElementById('vid').value);
  const pid = clean(document.getElementById('pid').value);
  const body = new URLSearchParams({ vid, pid });
  const r = await fetch('/api/settings', { method: 'POST', body });
  return r.ok;
}
async function saveSettings() {
  const ok = await postSettings();
  document.getElementById('status').textContent =
    ok ? 'Saved.' : 'Save failed - check VID/PID are valid hex.';
}
async function reboot() {
  if (!await postSettings()) {
    document.getElementById('status').textContent = 'Save failed - not rebooting.';
    return;
  }
  document.getElementById('status').textContent = 'Saved. Rebooting...';
  fetch('/api/reboot', { method: 'POST' });
}
function renderLive(d) {
  let h = '<tr><th>Axis</th><th>Now</th><th>Min</th><th>Max</th></tr>';
  for (let i = 0; i < 6; i++) {
    h += '<tr><td>' + AX[i] + '</td><td>' + d.now[i].toFixed(1) +
         '</td><td>' + d.min[i].toFixed(1) + '</td><td>' +
         d.max[i].toFixed(1) + '</td></tr>';
  }
  document.getElementById('live').innerHTML = h;
}
async function poll() {
  try {
    const r = await fetch('/api/live');
    renderLive(await r.json());
  } catch (e) {}
}
function startPolling() { if (!timer) timer = setInterval(poll, 150); }
async function calZero() {
  document.getElementById('calStatus').textContent = 'Capturing zero...';
  await fetch('/api/cal/zero', { method: 'POST' });
  document.getElementById('calStatus').textContent = 'Zero set.';
  startPolling();
}
async function calToggle() {
  const btn = document.getElementById('calBtn');
  if (!capturing) {
    await fetch('/api/cal/start', { method: 'POST' });
    capturing = true;
    btn.textContent = 'Stop & Save';
    document.getElementById('calStatus').textContent =
      'Move through full range...';
    startPolling();
  } else {
    const r = await fetch('/api/cal/stop', { method: 'POST' });
    capturing = false;
    btn.textContent = 'Start';
    document.getElementById('calStatus').textContent =
      r.ok ? 'Calibration saved.' : 'Save failed.';
    load();
  }
}
async function calClear() {
  await fetch('/api/cal/clear', { method: 'POST' });
  document.getElementById('calStatus').textContent = 'Calibration cleared.';
  load();
}
load();
</script>
</body>
</html>)HTML";

bool parseHex16(const String& s, uint16_t& out) {
  if (s.length() == 0 || s.length() > 4) {
    return false;
  }
  uint32_t value = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    value <<= 4;
    if (c >= '0' && c <= '9') {
      value |= (c - '0');
    } else if (c >= 'a' && c <= 'f') {
      value |= (c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      value |= (c - 'A' + 10);
    } else {
      return false;
    }
  }
  out = static_cast<uint16_t>(value);
  return true;
}

}  // namespace

ConfigPortal::ConfigPortal(Settings& settings, SensorController& sensors,
                           MotionController& motion)
    : server_(80), settings_(settings), sensors_(sensors), motion_(motion) {}

void ConfigPortal::begin() {
  // Bring the sensors up so live readings and calibration work in config mode,
  // then capture an initial rest baseline.
  sensors_.begin();
  captureBaseline();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(Config::CONFIG_AP_SSID);

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/settings", HTTP_GET, [this]() { handleGetSettings(); });
  server_.on("/api/settings", HTTP_POST, [this]() { handlePostSettings(); });
  server_.on("/api/live", HTTP_GET, [this]() { handleLive(); });
  server_.on("/api/cal/zero", HTTP_POST, [this]() { handleCalZero(); });
  server_.on("/api/cal/start", HTTP_POST, [this]() { handleCalStart(); });
  server_.on("/api/cal/stop", HTTP_POST, [this]() { handleCalStop(); });
  server_.on("/api/cal/clear", HTTP_POST, [this]() { handleCalClear(); });
  server_.on("/api/reboot", HTTP_POST, [this]() {
    server_.send(200, "text/plain", "rebooting");
    delay(200);
    ESP.restart();
  });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
}

void ConfigPortal::update() {
  server_.handleClient();

  if (capturing_) {
    const unsigned long now = millis();
    if (now - lastSampleMs_ >= 5) {
      lastSampleMs_ = now;
      float axes[6];
      readAxes(axes);
      for (int i = 0; i < 6; i++) {
        if (axes[i] < capMin_[i]) capMin_[i] = axes[i];
        if (axes[i] > capMax_[i]) capMax_[i] = axes[i];
      }
    }
  }
}

// Blocks briefly to average the rest pose into the sensor baseline.
void ConfigPortal::captureBaseline() {
  sensors_.beginCalibration();
  while (!sensors_.calibrationDone()) {
    sensors_.updateCalibration();
    delay(2);
  }
}

void ConfigPortal::readAxes(float axes[6]) {
  float raw[9] = {};
  sensors_.readRaw(raw);
  motion_.mixAxes(raw, sensors_.baseline(), axes);
}

void ConfigPortal::handleRoot() {
  server_.send_P(200, "text/html", kIndexHtml);
}

void ConfigPortal::handleGetSettings() {
  char vid[8];
  char pid[8];
  snprintf(vid, sizeof(vid), "%04x", settings_.vid());
  snprintf(pid, sizeof(pid), "%04x", settings_.pid());

  String json = "{\"vid\":\"";
  json += vid;
  json += "\",\"pid\":\"";
  json += pid;
  json += "\",\"calValid\":";
  json += settings_.calibration().valid ? "true" : "false";
  json += "}";

  server_.send(200, "application/json", json);
}

void ConfigPortal::handlePostSettings() {
  uint16_t vid = 0;
  uint16_t pid = 0;
  if (!server_.hasArg("vid") || !server_.hasArg("pid") ||
      !parseHex16(server_.arg("vid"), vid) ||
      !parseHex16(server_.arg("pid"), pid)) {
    server_.send(400, "text/plain", "invalid vid/pid");
    return;
  }

  settings_.setVid(vid);
  settings_.setPid(pid);
  settings_.save();

  server_.send(200, "text/plain", "saved");
}

void ConfigPortal::handleLive() {
  float axes[6];
  readAxes(axes);

  String json = "{\"now\":[";
  for (int i = 0; i < 6; i++) {
    if (i) json += ',';
    json += String(axes[i], 2);
  }
  json += "],\"min\":[";
  for (int i = 0; i < 6; i++) {
    if (i) json += ',';
    json += String(capMin_[i], 2);
  }
  json += "],\"max\":[";
  for (int i = 0; i < 6; i++) {
    if (i) json += ',';
    json += String(capMax_[i], 2);
  }
  json += "]}";

  server_.send(200, "application/json", json);
}

void ConfigPortal::handleCalZero() {
  captureBaseline();
  server_.send(200, "text/plain", "zeroed");
}

void ConfigPortal::handleCalStart() {
  for (int i = 0; i < 6; i++) {
    capMin_[i] = 0.0f;
    capMax_[i] = 0.0f;
  }
  lastSampleMs_ = millis();
  capturing_ = true;
  server_.send(200, "text/plain", "started");
}

void ConfigPortal::handleCalStop() {
  capturing_ = false;

  AxisCalibration cal;
  cal.valid = true;
  for (int i = 0; i < 6; i++) {
    cal.min[i] = capMin_[i];
    cal.max[i] = capMax_[i];
  }
  settings_.setCalibration(cal);
  settings_.save();
  motion_.setCalibration(cal);

  server_.send(200, "text/plain", "saved");
}

void ConfigPortal::handleCalClear() {
  capturing_ = false;
  for (int i = 0; i < 6; i++) {
    capMin_[i] = 0.0f;
    capMax_[i] = 0.0f;
  }
  AxisCalibration cal;  // valid = false by default
  settings_.setCalibration(cal);
  settings_.save();
  motion_.setCalibration(cal);

  server_.send(200, "text/plain", "cleared");
}

void ConfigPortal::handleNotFound() {
  // Redirect everything else to the root page (simple captive-portal feel).
  server_.sendHeader("Location", "/");
  server_.send(302, "text/plain", "");
}

#else  // Non-ESP32: configuration portal is unavailable.

ConfigPortal::ConfigPortal(Settings& settings, SensorController& sensors,
                           MotionController& motion)
    : settings_(settings), sensors_(sensors), motion_(motion) {}
void ConfigPortal::begin() {}
void ConfigPortal::update() {}

#endif

