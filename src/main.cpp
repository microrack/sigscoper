#include <Arduino.h>

// Pin Configuration
const int ledcPin = 26;

// LEDC Configuration
const int ledcChannel = 0;
const int ledcResolution = 8;
const int ledcFreq = 100000; // 100 kHz

// Sine wave configuration
const uint32_t sampling_frequency = 20000; // 20 kHz
const float sine_frequency = 1.0; // 1 Hz

// Globals for ISR
hw_timer_t *timer = NULL;
volatile uint32_t phase_accumulator = 0;
// (sine_frequency / sampling_frequency) * 2^32
const uint32_t phase_increment = (uint32_t)((sine_frequency / (float)sampling_frequency) * 4294967296.0);
uint8_t sin_lut[256];

void IRAM_ATTR onTimer() {
    phase_accumulator += phase_increment;
    uint8_t index = phase_accumulator >> 24;
    ledcWrite(ledcChannel, sin_lut[index]);
}

void setup() {
    // Generate sine lookup table
    for (int i = 0; i < 256; i++) {
        float angle = 2.0 * PI * (float)i / 256.0;
        sin_lut[i] = (uint8_t)((sin(angle) + 1.0) * 0.5 * 255.0);
    }

    // Configure LEDC
    ledcSetup(ledcChannel, ledcFreq, ledcResolution);
    ledcAttachPin(ledcPin, ledcChannel);

    // Configure Timer
    // APB_CLK is 80MHz, prescaler of 80 gives 1MHz timer clock
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    // Set timer to fire at the configured sampling frequency
    timerAlarmWrite(timer, 1000000 / sampling_frequency, true);
    timerAlarmEnable(timer);
}

void loop() {
    // Everything is handled by the timer ISR
}