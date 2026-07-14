#pragma once
#include <Preferences.h>

/// @brief Steuert einen bipolaren Schrittmotor (z. B. 28BYJ-48 mit DRV8825 / A4988)
///        über ESP32-LEDC-PWM mit Sinus-/Cosinus-Mikroschritt-Ansteuerung.
///        Verwendet Preferences zur persistenten Speicherung der Lift-Position.
///
/// Der Lift ist ein „Tonarmlift“ für Plattenspieler – der Motor fährt den
/// Tonarm nach oben/unten. Die Klasse unterstützt sowohl blockierende
/// (moveTo) als auch nicht-blockierende Bewegungen (startMoveTo + update).
class Tonarmlift {
public:
    /// @brief Konstruktor
    /// @param aPos  GPIO für A+
    /// @param aNeg  GPIO für A-
    /// @param bPos  GPIO für B+
    /// @param bNeg  GPIO für B-
    /// @param microsteps  Mikroschritte pro Vollschritt (default 16)
    /// @param freq        PWM-Frequenz in Hz (default 10000)
    /// @param resolution  PWM-Auflösung in Bits (default 10, → 0-1023)
    Tonarmlift(int aPos, int aNeg, int bPos, int bNeg,
               int microsteps = 16, int freq = 10000, int resolution = 10);

    /// @brief Initialisiert LEDC-Timer/-Kanäle und lädt gespeicherte Parameter
    ///        aus Preferences. Muss nach Konstruktor einmal aufgerufen werden.
    void begin(Preferences& prefs);

    /// @brief Blockierende Bewegung zur Zielposition (wartet bis fertig)
    void moveTo(int targetStep);

    /// @brief Startet eine nicht-blockierende Bewegung zur Zielposition.
    ///        Danach muss update() regelmäßig aufgerufen werden.
    void startMoveTo(int targetStep);

    /// @brief Muss in loop() aufgerufen werden, solange eine nicht-blockierende
    ///        Bewegung aktiv ist. Erledigt bis zu 10 Schritte pro Aufruf.
    void update();

    /// @brief Gibt die aktuelle absolute Schrittposition zurück
    int getPosition();

    /// @brief Setzt die Position (z. B. nach Kalibrierung) und speichert
    void setPosition(int value);

    /// @brief Schaltet die PWM-Spulen stromlos
    void disableMotor();

    /// @brief Setzt den Referenzwert für „oben“ und speichert
    void setRefTop(int value);

    /// @brief Setzt den Referenzwert für „unten“ und speichert
    void setRefBottom(int value);

    /// @brief Gibt den Referenzwert für „oben“
    int getRefTop();

    /// @brief Gibt den Referenzwert für „unten“
    int getRefBottom();

    /// @brief Setzt die Verzögerung zwischen Schritten (1–30 ms)
    void setStepDelay(int valueMs);

    /// @brief Gibt die Schrittverzögerung in ms zurück
    int getStepDelay();

    /// @brief Blockierende Fahrt zur Referenzposition „oben“
    void moveToTop();

    /// @brief Blockierende Fahrt zur Referenzposition „unten“
    void moveToBottom();

    /// @brief Speichert die aktuelle Position in Preferences
    void savePosition();

    /// @brief Startet Dauerbewegung (Jogging) in eine Richtung.
    ///        Läuft so lange, bis stopJog() aufgerufen wird.
    ///        @param direction +1 = vorwärts, -1 = rückwärts
    void startJog(int direction);

    /// @brief Stoppt eine Jogging-Bewegung (schaltet Motor ab).
    void stopJog();

    /// @brief Gibt true zurück, wenn gerade gejoggt wird
    bool isJogging();

    /// @brief Gibt true zurück, wenn eine Bewegung läuft
    bool isMoving();

private:
    void stepMotor(int direction, int pwmValue);
    uint32_t computeStepIntervalUs() const;
    int computeStepPwm() const;

    int pinA_pos, pinA_neg, pinB_pos, pinB_neg;
    int chA_pos = 0;
    int chA_neg = 1;
    int chB_pos = 2;
    int chB_neg = 3;

    int microsteps;
    int freq;
    int resolution;
    int currentStep = 0;
    int stepCount = 0;

    bool nonBlockingActive = false;
    int targetStep = 0;
    int moveDirection = 0;
    int stepsRemaining = 0;
    uint32_t nextStepUs = 0;
    uint32_t stepIntervalUs = 5000;

    bool joggingActive = false;
    int jogDirection = 0;

    int refTop = 1024;
    int refBottom = 0;
    int stepDelayMs = 5;

    const int maxPWM = 1023;
    const int pwmThreshold = 8;

    Preferences* prefsPtr = nullptr;
    bool _isMoving = false;
};