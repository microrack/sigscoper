#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h>
#include <cstring>
#include <algorithm>
#include "trigger.h"

#define MAX_CHANNELS 8
#define SIGNAL_BUFFER_SIZE 128
#define TRIGGER_POSITON 64
#define MEDIAN_FILTER_WINDOW 3
#define SAMPLE_RATE 20000

// Structure for signal statistics
struct SigscoperStats {
    uint16_t min_value;
    uint16_t max_value;
    float avg_value;
    float frequency;
    
    SigscoperStats() {
        min_value = UINT16_MAX;
        max_value = 0;
        avg_value = 0;
        frequency = 0;
    }
};

// Sigscoper configuration structure
struct SigscoperConfig {
    size_t channel_count;
    adc_channel_t channels[MAX_CHANNELS];
    TriggerMode trigger_mode;
    uint16_t trigger_level;
    uint32_t sampling_rate;
    float auto_speed;  // Controls coefficient of update_auto_level (0.0-1.0)
    
    SigscoperConfig() {
        channel_count = 0;
        trigger_mode = TriggerMode::FREE;
        trigger_level = 2048;
        sampling_rate = 20000;
        auto_speed = 0.002f;  // Default value (equivalent to previous 0.0002)
        memset(channels, 0, sizeof(channels));
    }
};

class Sigscoper {
private:
    // Configuration
    SigscoperConfig config_;
    
    // ADC
    adc_continuous_handle_t adc_handle_;
    
    // Task
    TaskHandle_t read_task_handle_;
    
    // Synchronization
    SemaphoreHandle_t mutex_;
    SemaphoreHandle_t start_semaphore_;
    
    // State
    bool running_;
    bool stop_requested_;
    bool is_ready_;
    uint32_t decimation_factor_;
    uint32_t sample_counter_;
    
    // Data
    uint16_t signal_buffers_[MAX_CHANNELS][SIGNAL_BUFFER_SIZE];
    size_t buffer_indices_[MAX_CHANNELS];
    
    // Trigger
    Trigger trigger_;
    
    // Median filter
    uint16_t median_buffer_[MEDIAN_FILTER_WINDOW];
    size_t median_index_;
    bool median_initialized_;
    
    // Constants
    static constexpr size_t CONV_FRAME_SIZE = 1024;
    
    // Private methods
    static void read_task_wrapper(void* param);
    void read_task();
    void process_sample(size_t channel_index, uint16_t sample);
    uint16_t apply_median_filter(uint16_t sample);
    float calculate_frequency_from_buffer_direct(size_t channel_index) const;

public:
    Sigscoper();
    Sigscoper(size_t buffer_size);
    ~Sigscoper();
    
    bool begin();
    bool start(const SigscoperConfig& config);
    void stop();
    void restart();
    
    // Getters
    bool is_running() const { return running_; }
    bool is_trigger_fired() const { return trigger_.is_fired(); }
    uint16_t get_trigger_threshold() const { return trigger_.get_threshold(); }
    size_t get_max_channels() const { return MAX_CHANNELS; }
    bool is_ready() const { return is_ready_; }
    
    // Data operations
    bool get_buffer(size_t index, size_t size, uint16_t* buffer, size_t* position) const;
    bool get_stats(size_t index, SigscoperStats* stats) const;
}; 