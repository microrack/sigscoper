#pragma once

#include <Arduino.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_err.h>
#include <hal/adc_types.h>
#include <soc/soc_caps.h>

#define MAX_CHANNELS 8
#define SIGNAL_BUFFER_SIZE 128

// Структура статистики для одного пина
struct SignalStats {
    uint16_t min_value;
    uint16_t max_value;
    float avg_value;
    float frequency;
    uint32_t sample_count;
    uint64_t total_value;
    bool initialized;
    
    // Для вычисления частоты
    bool signal_was_high;
    uint32_t last_crossing_time;
    uint64_t total_crossing_delta;
    uint32_t crossing_delta_count;
    
    SignalStats() : min_value(UINT16_MAX), max_value(0), avg_value(0), 
                   frequency(0), sample_count(0), total_value(0), 
                   initialized(false), signal_was_high(false), 
                   last_crossing_time(0), total_crossing_delta(0), 
                   crossing_delta_count(0) {}
};

class Signal {
private:
    static const uint32_t SAMPLE_RATE = 20000;
    static const size_t BUFFER_SIZE = 256;
    static const size_t CONV_FRAME_SIZE = BUFFER_SIZE * 2;
    
    // ADC configuration
    adc_channel_t channels_[MAX_CHANNELS];
    size_t channel_count_;
    adc_continuous_handle_t adc_handle_;
    
    // Threading (using void* to avoid header dependencies)
    void* read_task_handle_;
    void* mutex_;
    void* start_semaphore_;
    bool running_;
    bool stop_requested_;
    
    // Data storage
    SignalStats stats_[MAX_CHANNELS];
    uint16_t signal_buffers_[MAX_CHANNELS][SIGNAL_BUFFER_SIZE];
    size_t buffer_indices_[MAX_CHANNELS];
    
    // Private methods
    static void read_task_wrapper(void* parameter);
    void read_task();
    void process_sample(size_t channel_index, uint16_t sample);
    void calculate_frequency(SignalStats& stats, uint16_t sample, uint32_t absolute_sample_count);
    
public:
    Signal(const adc_channel_t* channels, size_t channel_count);
    ~Signal();
    
    bool start();
    void stop();
    void reset_stats();
    
    SignalStats get_stats(size_t index) const;
    bool get_buffer(size_t index, size_t size, uint16_t* buffer) const;
    
    size_t get_channel_count() const { return channel_count_; }
    bool is_running() const { return running_; }
    
    static constexpr size_t get_max_channels() { return MAX_CHANNELS; }
}; 