#include "trigger.h"
#include <Arduino.h>

Trigger::Trigger() {
    mode_ = TriggerMode::FREE;
    threshold_ = 2048;
    hysteresis_ = 200; // Default hysteresis
    armed_ = false;
    fired_ = false;
    ready_to_trigger_ = false;
    samples_after_trigger_ = 0;
    buffer_size_ = 128; // Default buffer size
    prev_sample_ = 2048;
    first_sample_ = true;
    trigger_position_ = 64; // Default trigger position
    
    // Automatic trigger level
    auto_level_ = 2048;
    auto_speed_ = 0.002f;  // Default value
}

void Trigger::start(TriggerMode mode, uint16_t threshold, float auto_speed, size_t buffer_size, size_t trigger_position) {
    mode_ = mode;
    threshold_ = threshold;
    hysteresis_ = threshold / 40; // Hysteresis as 2.5% of threshold
    armed_ = (mode_ != TriggerMode::FREE);
    fired_ = false;
    ready_to_trigger_ = false;
    samples_after_trigger_ = 0;
    prev_sample_ = threshold;
    first_sample_ = true;
    
    // Set buffer parameters
    buffer_size_ = buffer_size;
    trigger_position_ = trigger_position;
    
    // Set automatic trigger level parameters
    auto_level_ = threshold;
    auto_speed_ = auto_speed;
}

TriggerState Trigger::check_trigger(uint16_t sample) {
    // If this is the first sample, return special state
    if (first_sample_) {
        first_sample_ = false;
        prev_sample_ = sample;

        // buffer not ready, continue work
        return {false, true};
    }
    
    // Update automatic level
    update_auto_level(sample);
    
    // If trigger has already fired, check recording state
    if (fired_) {
        samples_after_trigger_++;

        // If we've written half the buffer after trigger
        if (samples_after_trigger_ >= buffer_size_) {
            // buffer ready, stop work
            return TriggerState{true, false};
        }

        // buffer not ready, continue work
        return TriggerState{false, true};
    }

    // get at least half of the buffer
    if(samples_after_trigger_ < trigger_position_) {
        samples_after_trigger_++;
        return TriggerState{false, true};
    }
    
    // If trigger hasn't fired yet, check trigger conditions
    bool trigger_condition = false;
    switch (mode_) {
        case TriggerMode::AUTO_RISE:
            // For AUTO_RISE: first signal must drop below threshold - hysteresis
            // then rise above threshold + hysteresis
            if (!ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::AUTO_FALL:
            // For AUTO_FALL: first signal must rise above threshold + hysteresis
            // then drop below threshold - hysteresis
            if (!ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::FIXED_RISE:
            // For FIXED_RISE: first signal must drop below threshold - hysteresis
            // then rise above threshold + hysteresis
            if (!ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::FIXED_FALL:
            // For FIXED_FALL: first signal must rise above threshold + hysteresis
            // then drop below threshold - hysteresis
            if (!ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::FREE:
        default:
            trigger_condition = true;
            break;
    }

    prev_sample_ = sample;
    
    if (trigger_condition) {
        fired_ = true;
    }

    // buffer not ready, continue work
    return TriggerState{false, true};
}

void Trigger::reset_level() {
    // Reset only trigger level
    first_sample_ = true;
    auto_level_ = threshold_;

    reset();
}

void Trigger::reset() {
    fired_ = false;
    armed_ = (mode_ != TriggerMode::FREE);
    ready_to_trigger_ = false;
    samples_after_trigger_ = 0;
    prev_sample_ = threshold_;
}

void Trigger::update_auto_level(uint16_t sample) {
    // Update average value for automatic trigger level
    // auto_speed controls the coefficient: 0.0 = no change, 1.0 = immediate change
    float auto_speed = max(0.0f, min(1.0f, auto_speed_));
    auto_level_ = sample * auto_speed + auto_level_ * (1.0f - auto_speed);
    
    // For AUTO modes use calculated level
    if (mode_ == TriggerMode::AUTO_RISE
        || mode_ == TriggerMode::AUTO_FALL
        || mode_ == TriggerMode::FREE) {
        threshold_ = auto_level_;
    }
} 