/**
 * Tonarmlift Standalone mit Webinterface.
 *
 * Hardware:
 *   - ESP32 (z. B. ESP32-S3-DevKitC-1)
 *   - Bipolares Schrittmotor-Treiberboard (z. B. DRV8825, A4988)
 *   - Schrittmotor (z. B. 28BYJ-48 umgebaut auf bipolar)
 *
 * Anschluss (Beispiel-Pins, siehe LIFT_*-Defines unten):
 *   LIFT_IA2 (GPIO 2) → A+ (Motor-Spule A)
 *   LIFT_IA1 (GPIO 1) → A- (Motor-Spule A)
 *   LIFT_IB2 (GPIO 42) → B+ (Motor-Spule B)
 *   LIFT_IB1 (GPIO 41) → B- (Motor-Spule B)
 *
 * Funktion:
 *   - Der ESP32 startet einen WLAN-Access-Point ("Tonarmlift-AP").
 *   - Über einen Browser (IP 192.168.4.1) kann der Lift gesteuert werden.
 *   - Tasten: Hochfahren / Runterfahren
 *   - Endpunkte: Aktuelle Position als "oben" oder "unten" speichern
 */

#include <Arduino.h>
#include <Preferences.h>
#include "Tonarmlift.h"
#include "WebInterface.h"

// -------------------- Pin-Definitionen (anpassen!) --------------------
#define LIFT_IA2 2   // A+
#define LIFT_IA1 1   // A-
#define LIFT_IB2 42  // B+
#define LIFT_IB1 41  // B-

// -------------------- Parameter --------------------
const int MICROSTEPS = 8;       // Mikroschritte pro Vollschritt
const int PWM_FREQ   = 20000;   // PWM-Frequenz in Hz
const int PWM_RES    = 10;      // PWM-Auflösung in Bits (0-1023)
const int STEP_DELAY_MS = 5;    // Verzögerung zwischen Schritten (1-30 ms)

// -------------------- Objekte --------------------
Preferences prefs;
Tonarmlift lift(LIFT_IA2, LIFT_IA1, LIFT_IB2, LIFT_IB1,
                MICROSTEPS, PWM_FREQ, PWM_RES);
WebInterface web(lift);

// -------------------- Setup --------------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Tonarmlift Standalone mit Webinterface ===");

    // Preferences-Namespace "lift" öffnen
    prefs.begin("lift", false);

    // Lift initialisieren (lädt gespeicherte Position/Referenzen)
    lift.begin(prefs);

    // Geschwindigkeit setzen
    lift.setStepDelay(STEP_DELAY_MS);

    // Aktuelle Position und Referenzen ausgeben
    Serial.printf("Position: %d\n", lift.getPosition());
    Serial.printf("RefTop:   %d\n", lift.getRefTop());
    Serial.printf("RefBottom:%d\n", lift.getRefBottom());

    // Webinterface starten (WLAN-AP + Webserver)
    web.begin();
}

// -------------------- Loop --------------------
void loop() {
    // Nicht-blockierende Lift-Updates (wichtig, wenn startMoveTo verwendet wird)
    lift.update();

    // Webinterface: eingehende HTTP-Requests bearbeiten
    web.handleClient();
}
