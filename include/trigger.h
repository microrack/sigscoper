#pragma once

#include <cstdint>
#include <cstddef>

// Trigger modes
enum class TriggerMode {
    FREE,
    AUTO_RISE,
    AUTO_FALL,
    FIXED_RISE,
    FIXED_FALL
};

// Structure for returning trigger state
typedef struct _TriggerState {
    bool buffer_ready;    // Whether buffer is ready for get_buffer and get_stats
    bool continue_work;   // Whether to continue work (if not, call stop)
} TriggerState;

class Trigger {
private:
    TriggerMode mode_;
    uint16_t threshold_;
    uint16_t hysteresis_;
    bool armed_;
    bool fired_;
    bool ready_to_trigger_;
    size_t samples_after_trigger_;
    size_t buffer_size_;
    size_t trigger_position_;
    uint16_t prev_sample_;
    bool first_sample_;
    
    // Automatic trigger level
    float auto_level_;
    float auto_speed_;  // Controls coefficient of update_auto_level (0.0-1.0)
    
    // Private methods
    void update_auto_level(uint16_t sample);
    
public:
    Trigger(size_t buffer_size, size_t trigger_position);
    
    void start(TriggerMode mode, uint16_t threshold, float auto_speed = 0.002f);
    TriggerState check_trigger(uint16_t sample);
    void reset_level();
    void reset();
    
    // Getters
    bool is_fired() const { return fired_; }
    uint16_t get_threshold() const { return threshold_; }
    bool is_armed() const { return armed_; }
    size_t get_buffer_size() const { return buffer_size_; }
};