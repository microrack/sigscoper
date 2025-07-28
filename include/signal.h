#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h>
#include <cstring>
#include <algorithm>

#define MAX_CHANNELS 8
#define SIGNAL_BUFFER_SIZE 128
#define MEDIAN_FILTER_WINDOW 5
#define SAMPLE_RATE 20000

// Структура для статистики сигнала
struct SignalStats {
    uint16_t min_value;
    uint16_t max_value;
    float avg_value;
    float frequency;
    
    SignalStats() {
        min_value = UINT16_MAX;
        max_value = 0;
        avg_value = 0;
        frequency = 0;
    }
};

// Режимы триггера
enum class TriggerMode {
    FREE,
    AUTO_RISE,
    AUTO_FALL,
    FIXED_RISE,
    FIXED_FALL
};

// Структура конфигурации сигнала
struct SignalConfig {
    size_t channel_count;
    adc_channel_t channels[MAX_CHANNELS];
    TriggerMode trigger_mode;
    uint16_t trigger_level;
    
    SignalConfig() {
        channel_count = 0;
        trigger_mode = TriggerMode::FREE;
        trigger_level = 2048;
        memset(channels, 0, sizeof(channels));
    }
};

class Signal {
private:
    // Конфигурация
    SignalConfig config_;
    
    // ADC
    adc_continuous_handle_t adc_handle_;
    
    // Задача
    TaskHandle_t read_task_handle_;
    
    // Синхронизация
    SemaphoreHandle_t mutex_;
    SemaphoreHandle_t start_semaphore_;
    
    // Состояние
    bool running_;
    bool stop_requested_;
    
    // Данные
    uint16_t signal_buffers_[MAX_CHANNELS][SIGNAL_BUFFER_SIZE];
    size_t buffer_indices_[MAX_CHANNELS];
    
    // Триггер
    TriggerMode trigger_mode_;
    uint16_t trigger_level_;
    bool trigger_enabled_;
    bool trigger_armed_;
    bool trigger_fired_;
    size_t trigger_position_;
    
    // Автоматический уровень триггера
    uint64_t auto_trigger_sum_;
    uint32_t auto_trigger_count_;
    uint16_t auto_trigger_level_;
    
    // Медианный фильтр
    uint16_t median_buffer_[MEDIAN_FILTER_WINDOW];
    size_t median_index_;
    bool median_initialized_;
    
    // Константы
    static constexpr size_t CONV_FRAME_SIZE = 1024;
    
    // Приватные методы
    static void read_task_wrapper(void* param);
    void read_task();
    void process_sample(size_t channel_index, uint16_t sample);
    uint16_t apply_median_filter(uint16_t sample);
    bool check_trigger(uint16_t sample, uint16_t prev_sample);
    void reset_trigger();
    void update_auto_trigger_level(uint16_t sample);
    float calculate_frequency_from_buffer_direct(size_t channel_index) const;

public:
    Signal();
    ~Signal();
    
    bool start(const SignalConfig& config);
    void stop();
    
    // Геттеры
    bool is_running() const { return running_; }
    bool is_trigger_fired() const { return trigger_fired_; }
    size_t get_trigger_position() const { return trigger_position_; }
    uint16_t get_auto_trigger_level() const { return auto_trigger_level_; }
    size_t get_max_channels() const { return MAX_CHANNELS; }
    
    // Работа с данными
    bool get_buffer(size_t index, size_t size, uint16_t* buffer) const;
    SignalStats get_stats(size_t index) const;
}; 