#pragma once

#include <Arduino.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_err.h>
#include <hal/adc_types.h>
#include <soc/soc_caps.h>

#define MAX_CHANNELS 8
#define SIGNAL_BUFFER_SIZE 128
#define MEDIAN_FILTER_WINDOW 5

// Режимы триггера
enum class TriggerMode {
    FREE,           // Без триггера - непрерывная запись
    AUTO_RISE,      // Автоматический триггер по нарастающему фронту
    AUTO_FALL,      // Автоматический триггер по спадающему фронту
    FIXED_RISE,     // Фиксированный триггер по нарастающему фронту
    FIXED_FALL      // Фиксированный триггер по спадающему фронту
};

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
    
    // Для медианного фильтра
    uint16_t median_buffer[MEDIAN_FILTER_WINDOW];
    size_t median_index;
    bool median_initialized;
    
    SignalStats() : min_value(UINT16_MAX), max_value(0), avg_value(0), 
                   frequency(0), sample_count(0), total_value(0), 
                   initialized(false), signal_was_high(false), 
                   last_crossing_time(0), total_crossing_delta(0), 
                   crossing_delta_count(0), median_index(0), median_initialized(false) {
        memset(median_buffer, 0, sizeof(median_buffer));
    }
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
    
    // Trigger configuration
    TriggerMode trigger_mode_;
    uint16_t trigger_level_;
    bool trigger_enabled_;
    bool trigger_armed_;
    bool trigger_fired_;
    size_t trigger_position_;
    
    // Auto trigger level calculation
    uint64_t auto_trigger_sum_;
    uint32_t auto_trigger_count_;
    uint16_t auto_trigger_level_;
    
    // Data storage
    SignalStats stats_[MAX_CHANNELS];
    uint16_t signal_buffers_[MAX_CHANNELS][SIGNAL_BUFFER_SIZE];
    size_t buffer_indices_[MAX_CHANNELS];
    
    // Private methods
    static void read_task_wrapper(void* parameter);
    void read_task();
    void process_sample(size_t channel_index, uint16_t sample);
    void calculate_frequency(SignalStats& stats, uint16_t sample, uint32_t absolute_sample_count);
    uint16_t apply_median_filter(SignalStats& stats, uint16_t sample);
    bool check_trigger(uint16_t sample, uint16_t prev_sample);
    void reset_trigger();
    void update_auto_trigger_level(uint16_t sample);
    
public:
    Signal(const adc_channel_t* channels, size_t channel_count);
    ~Signal();
    
    bool start(TriggerMode trigger_mode = TriggerMode::AUTO_RISE, uint16_t trigger_level = 2048);
    void stop();
    void reset_stats();
    
    SignalStats get_stats(size_t index) const;
    bool get_buffer(size_t index, size_t size, uint16_t* buffer) const;
    
    size_t get_channel_count() const { return channel_count_; }
    bool is_running() const { return running_; }
    bool is_trigger_fired() const { return trigger_fired_; }
    size_t get_trigger_position() const { return trigger_position_; }
    uint16_t get_auto_trigger_level() const { return auto_trigger_level_; }
    
    static constexpr size_t get_max_channels() { return MAX_CHANNELS; }
}; 