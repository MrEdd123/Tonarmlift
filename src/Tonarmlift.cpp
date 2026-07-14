#include <Arduino.h>
#include "driver/ledc.h"
#include "Tonarmlift.h"
#include <math.h>

// Debug-Makros für eigenständige Nutzung – einfach DEBUG auf 1 setzen, um
// Serial-Ausgaben zu aktivieren.
#define DEBUG 0
#if DEBUG
  #define DPRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DPRINTF(...)
#endif

static inline int scaleStepValue(int value, int fromMicrosteps, int toMicrosteps) {
    if (fromMicrosteps <= 0 || fromMicrosteps == toMicrosteps) return value;
    const int64_t scaled = (int64_t)value * (int64_t)toMicrosteps;
    return (int)((scaled + (fromMicrosteps / 2)) / fromMicrosteps);
}

static inline int wrapMicrostepIndex(int value, int microsteps) {
    if (microsteps <= 0) return 0;
    int wrapped = value % microsteps;
    if (wrapped < 0) wrapped += microsteps;
    return wrapped;
}

// Hilfsfunktion: Duty setzen (IDF benötigt update)
static inline void pwmWrite(ledc_channel_t ch, int duty) {
    if (duty < 0) duty = 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

Tonarmlift::Tonarmlift(int aPos, int aNeg, int bPos, int bNeg,
                       int microsteps, int freq, int resolution)
  : pinA_pos(aPos), pinA_neg(aNeg), pinB_pos(bPos), pinB_neg(bNeg),
    microsteps(microsteps), freq(freq), resolution(resolution) {}

void Tonarmlift::begin(Preferences& prefs) {
    prefsPtr = &prefs;

    // --- LEDC TIMER ---
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = (ledc_timer_bit_t)resolution,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = (uint32_t)freq,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    // --- LEDC CHANNELS ---
    ledc_channel_config_t chApos = {
        .gpio_num   = pinA_pos,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = (ledc_channel_t)chA_pos,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&chApos);

    ledc_channel_config_t chAneg = {
        .gpio_num   = pinA_neg,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = (ledc_channel_t)chA_neg,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&chAneg);

    ledc_channel_config_t chBpos = {
        .gpio_num   = pinB_pos,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = (ledc_channel_t)chB_pos,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&chBpos);

    ledc_channel_config_t chBneg = {
        .gpio_num   = pinB_neg,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = (ledc_channel_t)chB_neg,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&chBneg);

    // --- Preferences laden ---
    refTop    = prefs.getInt("refTop", 1024);
    refBottom = prefs.getInt("refBottom", 0);
    stepCount = prefs.getInt("liftPos", 0);
    const int storedMicrostepsRaw = prefs.getInt("liftMicrosteps", 16);
    const int storedMicrosteps = (storedMicrostepsRaw > 0) ? storedMicrostepsRaw : microsteps;
    stepDelayMs = prefs.getInt("liftDelayMs", 5);
    if (stepDelayMs < 1) stepDelayMs = 1;
    if (stepDelayMs > 30) stepDelayMs = 30;
    stepIntervalUs = (uint32_t)stepDelayMs * 1000U;
    if (storedMicrosteps != microsteps) {
        refTop = scaleStepValue(refTop, storedMicrosteps, microsteps);
        refBottom = scaleStepValue(refBottom, storedMicrosteps, microsteps);
        stepCount = scaleStepValue(stepCount, storedMicrosteps, microsteps);

        prefs.putInt("refTop", refTop);
        prefs.putInt("refBottom", refBottom);
        prefs.putInt("liftPos", stepCount);
    }
    currentStep = wrapMicrostepIndex(stepCount, microsteps);
    prefs.putInt("liftMicrosteps", microsteps);

    DPRINTF("Tonarmlift::begin → refTop=%d, refBottom=%d, stepCount=%d, stepDelayMs=%d\n",
            refTop, refBottom, stepCount, stepDelayMs);
}

void Tonarmlift::stepMotor(int direction, int pwmValue) {
    currentStep += direction;
    if (currentStep >= microsteps) currentStep = 0;
    if (currentStep < 0) currentStep = microsteps - 1;

    float angle = (float)currentStep / microsteps * 2 * PI;
    int aPWM = (int)(sin(angle) * pwmValue);
    int bPWM = (int)(cos(angle) * pwmValue);

    if (abs(aPWM) < pwmThreshold) aPWM = 0;
    if (abs(bPWM) < pwmThreshold) bPWM = 0;

    pwmWrite((ledc_channel_t)chA_pos, aPWM > 0 ? aPWM : 0);
    pwmWrite((ledc_channel_t)chA_neg, aPWM < 0 ? -aPWM : 0);
    pwmWrite((ledc_channel_t)chB_pos, bPWM > 0 ? bPWM : 0);
    pwmWrite((ledc_channel_t)chB_neg, bPWM < 0 ? -bPWM : 0);

    stepCount += direction;
}

void Tonarmlift::startMoveTo(int target)
{
    int delta = target - stepCount;
    if (delta == 0) {
        nonBlockingActive = false;
        _isMoving = false;
        disableMotor();
        savePosition();
        return;
    }

    targetStep = target;
    moveDirection = (delta > 0) ? +1 : -1;
    stepsRemaining = abs(delta);
    nextStepUs = micros();
    nonBlockingActive = true;
    _isMoving = true;

    DPRINTF("startMoveTo: target=%d, current=%d, steps=%d, dir=%d\n",
            targetStep, stepCount, stepsRemaining, moveDirection);
}

void Tonarmlift::startJog(int direction) {
    if (direction == 0) return;
    // Bestehende Bewegung abbrechen
    nonBlockingActive = false;
    joggingActive = true;
    jogDirection = (direction > 0) ? +1 : -1;
    nextStepUs = micros();
    _isMoving = true;
    DPRINTF("startJog: dir=%d\n", jogDirection);
}

void Tonarmlift::stopJog() {
    if (!joggingActive) return;
    joggingActive = false;
    _isMoving = false;
    disableMotor();
    savePosition();
    DPRINTF("stopJog at stepCount=%d\n", stepCount);
}

bool Tonarmlift::isJogging() {
    return joggingActive;
}

void Tonarmlift::update()
{
    // --- Jogging-Update: max. 1 Schritt pro Aufruf für feinfühlige Steuerung ---
    if (joggingActive) {
        uint32_t nowUs = micros();
        if ((int32_t)(nowUs - nextStepUs) >= 0) {
            stepMotor(jogDirection, computeStepPwm());
            nextStepUs = nowUs + computeStepIntervalUs();
        }
        return;
    }

    if (!nonBlockingActive) return;

    const uint8_t maxStepsPerUpdate = 10;
    uint8_t stepsThisUpdate = 0;
    uint32_t nowUs = micros();

    while (nonBlockingActive &&
           stepsRemaining > 0 &&
           (int32_t)(nowUs - nextStepUs) >= 0 &&
           stepsThisUpdate < maxStepsPerUpdate) {
        stepMotor(moveDirection, computeStepPwm());
        stepsRemaining--;
        if (stepsRemaining <= 0 || stepCount == targetStep) {
            nonBlockingActive = false;
            _isMoving = false;
            disableMotor();
            savePosition();
            DPRINTF("Lift move done at stepCount=%d\n", stepCount);
            return;
        }

        nextStepUs += computeStepIntervalUs();
        stepsThisUpdate++;
        nowUs = micros();
    }

    // Kein Burst-Verhalten nach einer längeren Blockade aufbauen.
    if ((int32_t)(nowUs - nextStepUs) >= 0) {
        nextStepUs = nowUs + computeStepIntervalUs();
    }
}

void Tonarmlift::moveTo(int target)
{
    startMoveTo(target);
    while (nonBlockingActive) {
        update();
        delay(0);
    }
}

void Tonarmlift::disableMotor() {
    DPRINTF("Motor disabled at stepCount=%d\n", stepCount);

    pwmWrite((ledc_channel_t)chA_pos, 0);
    pwmWrite((ledc_channel_t)chA_neg, 0);
    pwmWrite((ledc_channel_t)chB_pos, 0);
    pwmWrite((ledc_channel_t)chB_neg, 0);
}


int Tonarmlift::getPosition() { return stepCount; }
void Tonarmlift::setPosition(int value) { 
    stepCount = value;
    currentStep = wrapMicrostepIndex(stepCount, microsteps);
    savePosition();
}

void Tonarmlift::setRefTop(int value) {
    refTop = value;
    if (prefsPtr) prefsPtr->putInt("refTop", refTop);
}

void Tonarmlift::setRefBottom(int value) {
    refBottom = value;
    if (prefsPtr) prefsPtr->putInt("refBottom", refBottom);
}

void Tonarmlift::setStepDelay(int valueMs) {
    if (valueMs < 1) valueMs = 1;
    if (valueMs > 30) valueMs = 30;
    stepDelayMs = valueMs;
    stepIntervalUs = (uint32_t)stepDelayMs * 1000U;
    if (prefsPtr) prefsPtr->putInt("liftDelayMs", stepDelayMs);
}

int Tonarmlift::getStepDelay() { return stepDelayMs; }

int Tonarmlift::getRefTop() { return refTop; }
int Tonarmlift::getRefBottom() { return refBottom; }

void Tonarmlift::moveToTop() { moveTo(refTop); }
void Tonarmlift::moveToBottom() { moveTo(refBottom); }

void Tonarmlift::savePosition() {
    if (prefsPtr) prefsPtr->putInt("liftPos", stepCount);
}

bool Tonarmlift::isMoving() {
    return _isMoving;
}

uint32_t Tonarmlift::computeStepIntervalUs() const
{
    // Soft braking during lowering is disabled for reliable endpoint reach.
    return stepIntervalUs;
}

int Tonarmlift::computeStepPwm() const
{
    // Use full drive strength while moving to avoid stalling near the end stop.
    return maxPWM;
}