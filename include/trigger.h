#pragma once

#include <cstdint>
#include <cstddef>

// Режимы триггера
enum class TriggerMode {
    FREE,
    AUTO_RISE,
    AUTO_FALL,
    FIXED_RISE,
    FIXED_FALL
};

// Структура для возврата состояния триггера
typedef struct _TriggerState {
    bool buffer_ready;    // Готов ли буфер к работе через get_buffer и get_stats
    bool continue_work;   // Нужно ли продолжать работу (если нет, вызывать stop)
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
    
    // Автоматический уровень триггера
    float auto_level_;
    
    // Приватные методы
    void update_auto_level(uint16_t sample);
    
public:
    Trigger(size_t buffer_size, size_t trigger_position);
    
    void start(TriggerMode mode, uint16_t threshold);
    TriggerState check_trigger(uint16_t sample);
    void reset_level();
    void reset();
    
    // Геттеры
    bool is_fired() const { return fired_; }
    uint16_t get_threshold() const { return threshold_; }
    bool is_armed() const { return armed_; }
    size_t get_buffer_size() const { return buffer_size_; }
};