#include <Arduino.h>

// Pin Configuration
const int ledcPin = 26;

// LEDC Configuration
const int ledcChannel = 0;
const int ledcResolution = 8;
const int ledcFreq = 5000;

// Sine wave configuration
const uint32_t sampling_frequency = 1000; // 1 kHz
const float sine_frequency = 1.0; // 1 Hz

// Globals for sine generation
uint32_t phase_accumulator = 0;
// (sine_frequency / sampling_frequency) * 2^32
const uint32_t phase_increment = (uint32_t)((sine_frequency / (float)sampling_frequency) * 4294967296.0);
uint8_t sin_lut[256];

unsigned long last_update_time = 0;
const unsigned long sampling_period_us = 1000000 / sampling_frequency;

void setup() {
    // Generate sine lookup table
    for (int i = 0; i < 256; i++) {
        float angle = 2.0 * PI * (float)i / 256.0;
        sin_lut[i] = (uint8_t)((sin(angle) + 1.0) * 0.5 * 255.0);
    }

    // Configure LEDC
    ledcSetup(ledcChannel, ledcFreq, ledcResolution);
    ledcAttachPin(ledcPin, ledcChannel);
}

void loop() {
    unsigned long current_time = micros();
    if (current_time - last_update_time >= sampling_period_us) {
        last_update_time = current_time;

        phase_accumulator += phase_increment;
        uint8_t index = phase_accumulator >> 24;
        ledcWrite(ledcChannel, sin_lut[index]);
    }
}