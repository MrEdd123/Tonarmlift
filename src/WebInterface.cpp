#include <Arduino.h>
#include <WiFi.h>
#include "WebInterface.h"

// ============================================================
//  Konstruktor
// ============================================================
WebInterface::WebInterface(Tonarmlift& lift,
                           const char* ssid,
                           const char* password)
    : _lift(lift)
    , _server(80)
    , _ssid(ssid)
    , _password(password)
{}

// ============================================================
//  Start
// ============================================================
void WebInterface::begin() {
    Serial.println("\n=== WebInterface: Starte WLAN-AccessPoint ===");

    WiFi.mode(WIFI_AP);
    WiFi.softAP(_ssid, _password);
    Serial.printf("SSID:     %s\n", _ssid);
    Serial.printf("Passwort: %s\n", _password);
    Serial.printf("IP:       %s\n", WiFi.softAPIP().toString().c_str());

    // --- Routen registrieren ---
    _server.on("/",              HTTP_GET, [this](){ _handleRoot(); });
    _server.on("/status",        HTTP_GET, [this](){ _handleStatus(); });
    _server.on("/toggle",        HTTP_POST, [this](){ _handleToggle(); });
    _server.on("/jogStart",      HTTP_POST, [this](){ _handleJogStart(); });
    _server.on("/jogStop",       HTTP_POST, [this](){ _handleJogStop(); });
    _server.on("/moveTop",       HTTP_POST, [this](){ _handleMoveTop(); });
    _server.on("/moveBottom",    HTTP_POST, [this](){ _handleMoveBottom(); });
    _server.on("/setRefTop",     HTTP_POST, [this](){ _handleSetTop(); });
    _server.on("/setRefBottom",  HTTP_POST, [this](){ _handleSetBottom(); });
    _server.on("/moveToPos",     HTTP_POST, [this](){ _handleMove(); });
    _server.onNotFound([this](){ _handleNotFound(); });

    _server.begin();
    Serial.println("Webserver gestartet (Port 80).");
}

void WebInterface::handleClient() {
    _server.handleClient();
}

// ============================================================
//  JSON-Status
// ============================================================
String WebInterface::_buildStatusJson() {
    String json;
    json += "{\n";
    json += "  \"position\": "     + String(_lift.getPosition())     + ",\n";
    json += "  \"refTop\": "       + String(_lift.getRefTop())       + ",\n";
    json += "  \"refBottom\": "    + String(_lift.getRefBottom())    + ",\n";
    json += "  \"moving\": "       + String(_lift.isMoving() ? "true" : "false") + ",\n";
    json += "  \"stepDelay\": "    + String(_lift.getStepDelay())    + "\n";
    json += "}\n";
    return json;
}

// ============================================================
//  HTML-Seite (gebaut via String-Konkatenation,
//  kein SPIFFS nötig)
// ============================================================
String WebInterface::_buildHtmlPage() {
    String html = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tonarmlift Steuerung</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
         background: #1a1a2e; color: #eee; display: flex; justify-content: center;
         align-items: center; min-height: 100vh; padding: 16px; }
  .card { background: #16213e; border-radius: 16px; padding: 32px; max-width: 420px;
          width: 100%; box-shadow: 0 8px 32px rgba(0,0,0,0.4); }
  h1 { text-align: center; margin-bottom: 24px; font-size: 1.5rem; color: #0f3460; }
  .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 28px; }
  .status-item { background: #0f3460; border-radius: 10px; padding: 14px; text-align: center; }
  .status-item .label { font-size: 0.75rem; text-transform: uppercase; opacity: 0.7; }
  .status-item .value { font-size: 1.3rem; font-weight: 700; margin-top: 4px; }
  .status-item .value.moving { color: #e94560; }
  .btn { display: block; width: 100%; padding: 14px; border: none; border-radius: 10px;
         font-size: 1rem; font-weight: 600; cursor: pointer; transition: all 0.15s; }
  .btn:active { transform: scale(0.97); }
  .btn:disabled { opacity: 0.5; pointer-events: none; }
  .btn-toggle { background: #0f3460; color: #fff; margin-bottom: 20px;
                font-size: 1.1rem; padding: 16px; border: 2px solid #533483; }
  .btn-jog { display: block; width: 100%; padding: 20px; border: none; border-radius: 12px;
             font-size: 1.3rem; font-weight: 700; cursor: pointer; transition: all 0.1s;
             -webkit-touch-callout: none; -webkit-user-select: none; user-select: none; }
  .btn-jog:active { transform: scale(0.95); }
  .btn-jog:disabled { opacity: 0.5; pointer-events: none; }
  .btn-jog-up { background: #2ecc71; color: #fff; margin-bottom: 6px; }
  .btn-jog-down { background: #e94560; color: #fff; margin-bottom: 20px; }
  .btn-jog-up:active { background: #27ae60; }
  .btn-jog-down:active { background: #c0392b; }
  .section-title { font-size: 0.85rem; text-transform: uppercase; opacity: 0.6;
                   margin: 20px 0 10px; }
  .btn-set { background: #533483; color: #fff; margin-bottom: 8px; }
  .btn-set:last-of-type { margin-bottom: 0; }
  .toast { display: none; background: #2ecc71; color: #fff; text-align: center;
           padding: 10px; border-radius: 8px; margin-top: 16px;
           font-weight: 500; transition: opacity 0.3s; }
  .toast.error { background: #e94560; }
</style>
</head>
<body>
<div class="card">
  <h1>⚙ Tonarmlift</h1>

  <div class="status-grid">
    <div class="status-item">
      <div class="label">Position</div>
      <div class="value" id="pos">0</div>
    </div>
    <div class="status-item">
      <div class="label">Status</div>
      <div class="value moving" id="status">Bereit</div>
    </div>
    <div class="status-item">
      <div class="label">Oben (Ref)</div>
      <div class="value" id="refTop">0</div>
    </div>
    <div class="status-item">
      <div class="label">Unten (Ref)</div>
      <div class="value" id="refBottom">0</div>
    </div>
  </div>

  <!-- Press-and-Hold: Lift bewegt sich, solange gedrückt -->
  <div class="section-title">Manuelle Positionierung (gedrückt halten)</div>
  <button class="btn-jog btn-jog-up"    id="btnUp"
    onmousedown="jogStart(1)"  onmouseup="jogStop()"   onmouseleave="jogStop()"
    ontouchstart="jogStart(1)" ontouchend="jogStop()"  ontouchcancel="jogStop()">
    ▲  Hoch
  </button>
  <button class="btn-jog btn-jog-down"  id="btnDown"
    onmousedown="jogStart(-1)" onmouseup="jogStop()"   onmouseleave="jogStop()"
    ontouchstart="jogStart(-1)" ontouchend="jogStop()" ontouchcancel="jogStop()">
    ▼  Runter
  </button>

  <!-- Umschalten zwischen den Endpunkten -->
  <button class="btn btn-toggle" id="btnToggle" onclick="doToggle()">
    ⟳  Zum anderen Endpunkt
  </button>

  <div class="section-title">Endpunkte setzen (aktuelle Position)</div>
  <button class="btn btn-set" onclick="setRefTop()">   ⬆  Als oberen Endpunkt speichern</button>
  <button class="btn btn-set" onclick="setRefBottom()">⬇  Als unteren Endpunkt speichern</button>

  <div class="toast" id="toast"></div>
</div>

<script>
const API = '';
const btnJogUp   = document.getElementById('btnUp');
const btnJogDown = document.getElementById('btnDown');
const btnToggle  = document.getElementById('btnToggle');
let jogActive = false;
const toastEl = document.getElementById('toast');

function showToast(msg, error) {
  toastEl.textContent = msg;
  toastEl.className = 'toast' + (error ? ' error' : '');
  toastEl.style.display = 'block';
  setTimeout(() => { toastEl.style.display = 'none'; }, 2500);
}

function setMovingUI(moving, targetText) {
  btnToggle.disabled = moving;
  // Jog-Buttons nur deaktivieren, wenn eine Zielbewegung (nonBlocking) läuft, nicht beim Joggen selbst
  if (!jogActive) {
    btnJogUp.disabled   = moving;
    btnJogDown.disabled = moving;
  }
  document.getElementById('status').textContent = moving ? (targetText || 'Fährt...') : 'Bereit';
}

function fetchStatus() {
  fetch(API + '/status')
    .then(r => r.json())
    .then(d => {
      document.getElementById('pos').textContent       = d.position;
      document.getElementById('refTop').textContent    = d.refTop;
      document.getElementById('refBottom').textContent = d.refBottom;
      if (!d.moving) {
        document.getElementById('status').textContent = 'Bereit';
        btnToggle.disabled = false;
        if (!jogActive) {
          btnJogUp.disabled   = false;
          btnJogDown.disabled = false;
        }
      }
    })
    .catch(() => {});
}

function jogStart(dir) {
  if (jogActive) return;
  jogActive = true;
  btnJogUp.disabled   = true;
  btnJogDown.disabled = true;
  btnToggle.disabled  = true;
  document.getElementById('status').textContent = dir > 0 ? 'Hoch...' : 'Runter...';
  fetch(API + '/jogStart', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'direction=' + dir
  }).catch(() => {});
}

function jogStop() {
  if (!jogActive) return;
  jogActive = false;
  fetch(API + '/jogStop', { method:'POST' })
    .then(() => { fetchStatus(); })
    .catch(() => {});
}

function doToggle() {
  setMovingUI(true, 'Ermittle Ziel...');
  // Erst Status abfragen, dann entscheiden
  fetch(API + '/status')
    .then(r => r.json())
    .then(d => {
      if (d.moving) {
        showToast('Lift bereits in Bewegung', true);
        setMovingUI(false);
        return;
      }
      const distTop = Math.abs(d.refTop - d.position);
      const distBottom = Math.abs(d.position - d.refBottom);
      if (distTop <= distBottom) {
        // Wir sind oben oder näher an oben → nach unten
        setMovingUI(true, 'Fahre nach unten...');
        return fetch(API + '/moveBottom', { method:'POST' })
          .then(r => r.json())
          .then(d2 => {
            if (d2.success) showToast('Fahre nach unten');
            else { showToast(d2.error || 'Fehler', true); setMovingUI(false); }
          });
      } else {
        // Wir sind unten oder näher an unten → nach oben
        setMovingUI(true, 'Fahre nach oben...');
        return fetch(API + '/moveTop', { method:'POST' })
          .then(r => r.json())
          .then(d2 => {
            if (d2.success) showToast('Fahre nach oben');
            else { showToast(d2.error || 'Fehler', true); setMovingUI(false); }
          });
      }
    })
    .catch(() => { showToast('Verbindungsfehler', true); setMovingUI(false); });
}

function moveTop() {
  setMovingUI(true, 'Fahre nach oben...');
  fetch(API + '/moveTop', { method:'POST' })
    .then(r => r.json())
    .then(d => {
      if (d.success) showToast('Fahre nach oben');
      else { showToast(d.error || 'Fehler', true); setMovingUI(false); }
    })
    .catch(() => { showToast('Verbindungsfehler', true); setMovingUI(false); });
}

function moveBottom() {
  setMovingUI(true, 'Fahre nach unten...');
  fetch(API + '/moveBottom', { method:'POST' })
    .then(r => r.json())
    .then(d => {
      if (d.success) showToast('Fahre nach unten');
      else { showToast(d.error || 'Fehler', true); setMovingUI(false); }
    })
    .catch(() => { showToast('Verbindungsfehler', true); setMovingUI(false); });
}

function setRefTop() {
  fetch(API + '/setRefTop', { method:'POST' })
    .then(r => r.json())
    .then(d => {
      if (d.success) { showToast('Oberer Endpunkt gespeichert'); fetchStatus(); }
      else showToast(d.error || 'Fehler', true);
    })
    .catch(() => showToast('Verbindungsfehler', true));
}

function setRefBottom() {
  fetch(API + '/setRefBottom', { method:'POST' })
    .then(r => r.json())
    .then(d => {
      if (d.success) { showToast('Unterer Endpunkt gespeichert'); fetchStatus(); }
      else showToast(d.error || 'Fehler', true);
    })
    .catch(() => showToast('Verbindungsfehler', true));
}

// Status alle 1,5 s aktualisieren
setInterval(fetchStatus, 1500);
fetchStatus();
</script>
</body>
</html>
)HTML";
    return html;
}

// ============================================================
//  Route: Root → HTML-Seite ausliefern
// ============================================================
void WebInterface::_handleRoot() {
    _server.send(200, "text/html; charset=utf-8", _buildHtmlPage());
}

// ============================================================
//  Route: Status (JSON)
// ============================================================
void WebInterface::_handleStatus() {
    _server.send(200, "application/json", _buildStatusJson());
}

// ============================================================
//  Route: Jogging starten (direction=1 oder -1)
// ============================================================
void WebInterface::_handleJogStart() {
    String json;
    if (!_server.hasArg("direction")) {
        json = "{\"success\":false,\"error\":\"Parameter 'direction' fehlt\"}\n";
        _server.send(400, "application/json", json);
        return;
    }
    int dir = _server.arg("direction").toInt();
    if (dir == 0) {
        json = "{\"success\":false,\"error\":\"direction muss +1 oder -1 sein\"}\n";
        _server.send(400, "application/json", json);
        return;
    }
    // Bestehende Bewegung abbrechen, falls vorhanden
    if (_lift.isMoving()) {
        _lift.stopJog();
    }
    _lift.startJog(dir);
    json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Jogging stoppen
// ============================================================
void WebInterface::_handleJogStop() {
    _lift.stopJog();
    String json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Umschalten (Toggle – fährt zum jeweils anderen Endpunkt)
// ============================================================
void WebInterface::_handleToggle() {
    String json;
    if (_lift.isMoving()) {
        json = "{\"success\":false,\"error\":\"Lift ist bereits in Bewegung\"}\n";
        _server.send(200, "application/json", json);
        return;
    }

    int pos = _lift.getPosition();
    int distTop = abs(_lift.getRefTop() - pos);
    int distBottom = abs(pos - _lift.getRefBottom());

    if (distTop <= distBottom) {
        // Näher an oben → fahre nach unten
        _lift.startMoveTo(_lift.getRefBottom());
    } else {
        // Näher an unten → fahre nach oben
        _lift.startMoveTo(_lift.getRefTop());
    }
    json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Hochfahren (move to refTop)
// ============================================================
void WebInterface::_handleMoveTop() {
    String json;
    if (_lift.isMoving()) {
        json = "{\"success\":false,\"error\":\"Lift ist bereits in Bewegung\"}\n";
        _server.send(200, "application/json", json);
        return;
    }
    _lift.startMoveTo(_lift.getRefTop());
    json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Runterfahren (move to refBottom)
// ============================================================
void WebInterface::_handleMoveBottom() {
    String json;
    if (_lift.isMoving()) {
        json = "{\"success\":false,\"error\":\"Lift ist bereits in Bewegung\"}\n";
        _server.send(200, "application/json", json);
        return;
    }
    _lift.startMoveTo(_lift.getRefBottom());
    json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Aktuelle Position als oberen Endpunkt speichern
// ============================================================
void WebInterface::_handleSetTop() {
    _lift.setRefTop(_lift.getPosition());
    _lift.savePosition();
    String json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Aktuelle Position als unteren Endpunkt speichern
// ============================================================
void WebInterface::_handleSetBottom() {
    _lift.setPosition(0);
    _lift.setRefBottom(0);
    _lift.savePosition();
    String json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  Route: Freie Position anfahren (move?pos=1234)
// ============================================================
void WebInterface::_handleMove() {
    String json;
    if (_lift.isMoving()) {
        json = "{\"success\":false,\"error\":\"Lift ist bereits in Bewegung\"}\n";
        _server.send(200, "application/json", json);
        return;
    }
    if (!_server.hasArg("pos")) {
        json = "{\"success\":false,\"error\":\"Parameter 'pos' fehlt\"}\n";
        _server.send(400, "application/json", json);
        return;
    }
    int target = _server.arg("pos").toInt();
    _lift.startMoveTo(target);
    json = "{\"success\":true}\n";
    _server.send(200, "application/json", json);
}

// ============================================================
//  404
// ============================================================
void WebInterface::_handleNotFound() {
    String msg = "404 – Not Found\n";
    _server.send(404, "text/plain", msg);
}