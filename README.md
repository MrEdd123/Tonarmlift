# Tonarmlift Standalone

Isolierte Klasse zur Steuerung eines **Tonarmlift-Schrittmotors** für Plattenspieler.
Extrahiert aus dem Projekt [Plattenspieler-Elektronik](https://github.com/MrEdd123/Plattenspieler-Elektronik).

## Funktionsprinzip

Die Klasse `Tonarmlift` steuert einen bipolaren Schrittmotor über ESP32-LEDC-PWM
mit **Sinus-/Cosinus-Mikroschritt-Ansteuerung**. Dadurch läuft der Motor besonders
ruhig und vibrationsarm – ideal für einen Tonarmlift.

- **Blockierende Bewegung**: `moveTo(target)` – kehrt erst zurück, wenn die Position erreicht ist
- **Nicht-blockierende Bewegung**: `startMoveTo(target)` + `update()` in `loop()` – für den Einsatz neben anderen zeitkritischen Aufgaben
- **Persistente Speicherung**: Position, Referenzpunkte und Geschwindigkeit werden in den ESP32-Preferences gespeichert (überlebt Reset/Stromausfall)
- **Mikroschritt-Adaption**: Bei Änderung der Mikroschritt-Einstellung werden gespeicherte Werte automatisch skaliert

## Hardware

- **ESP32** (getestet auf ESP32-S3-DevKitC-1)
- **Bipolares Schrittmotor-Treiberboard** (z. B. DRV8825, A4988, TMC2209)
- **Schrittmotor** (z. B. 28BYJ-48 umgebaut auf bipolar, NEMA-14 o. Ä.)

### Anschluss

| Tonarmlift-Pin | Funktion | Beispiel GPIO |
|----------------|----------|---------------|
| A+ (IA2)       | Spule A Phase+ | 2 |
| A- (IA1)       | Spule A Phase- | 1 |
| B+ (IB2)       | Spule B Phase+ | 42 |
| B- (IB1)       | Spule B Phase- | 41 |

Die Pins werden im Konstruktor frei gewählt – obige GPIOs sind nur ein Beispiel.

## Verwendung

### Dateien einbinden

Kopiere folgende Dateien in dein Projekt:

```
src/Tonarmlift.h
src/Tonarmlift.cpp
```

**Abhängigkeiten (alle in der ESP32-Arduino-Core enthalten):**
- `<Arduino.h>`
- `<Preferences.h>`
- `"driver/ledc.h"`
- `<math.h>`

### Minimalbeispiel

```cpp
#include <Arduino.h>
#include <Preferences.h>
#include "Tonarmlift.h"

Preferences prefs;
Tonarmlift lift(2, 1, 42, 41, 8, 20000, 10);

void setup() {
    Serial.begin(115200);
    prefs.begin("lift", false);
    lift.begin(prefs);

    lift.setRefTop(1024);      // Position für „oben"
    lift.setRefBottom(0);      // Position für „unten"
    lift.setStepDelay(5);      // 5 ms zwischen Schritten

    lift.moveToTop();          // Blockierende Fahrt nach oben
}

void loop() {
    lift.update();             // Für nicht-blockierende Bewegungen

    if (!lift.isMoving()) {
        // Alle 3 Sekunden zwischen oben/unten wechseln
        // ...
    }
}
```

### API-Übersicht

| Methode | Beschreibung |
|---------|-------------|
| `Tonarmlift(aPos, aNeg, bPos, bNeg, microsteps, freq, resolution)` | Konstruktor mit Pin-Definitionen |
| `begin(Preferences& prefs)` | Initialisiert PWM und lädt gespeicherte Werte |
| `moveTo(targetStep)` | Blockierende Fahrt zur Zielposition |
| `startMoveTo(targetStep)` | Startet nicht-blockierende Fahrt |
| `update()` | Setzt nicht-blockierende Bewegung fort (in loop()) |
| `getPosition()` | Aktuelle Schritt-Position |
| `setPosition(value)` | Position setzen (z. B. nach Kalibrierung) |
| `setRefTop(value)` / `setRefBottom(value)` | Referenzwerte für „oben"/„unten" setzen |
| `getRefTop()` / `getRefBottom()` | Referenzwerte abfragen |
| `moveToTop()` / `moveToBottom()` | Blockierend zur Referenz fahren |
| `setStepDelay(ms)` | Geschwindigkeit (1–30 ms Schritt-Intervall) |
| `getStepDelay()` | Aktuelle Verzögerung in ms |
| `disableMotor()` | PWM-Spulen stromlos schalten |
| `savePosition()` | Position in Preferences speichern |
| `isMoving()` | `true` während einer Bewegung |

## PlattformIO

Das Projekt verwendet PlatformIO. Einfach öffnen und bauen:

```bash
pio run
```

Zum Hochladen auf den ESP32:

```bash
pio run --target upload
pio device monitor
```

Das Beispiel-Sketch in `src/main.cpp` fährt nach dem Boot zur oberen Position
und pendelt dann alle 3 Sekunden zwischen oben und unten.

## Anpassung für andere ESP32-Modelle

In `platformio.ini` die `board`-Zeile ändern, z. B.:

- `board = esp32-devkitc` für ESP32
- `board = esp32-c3-devkitm-1` für ESP32-C3
- `board = esp32-s2-saola-1` für ESP32-S2

Gegebenenfalls die `build_flags` anpassen (PSRAM-Optionen nur für S3 nötig).