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
struct TriggerState {
    bool buffer_ready;    // Готов ли буфер к работе через get_buffer и get_stats
    bool continue_work;   // Нужно ли продолжать работу (если нет, вызывать stop)
    
    TriggerState() : buffer_ready(false), continue_work(true) {}
};

class Trigger {
private:
    TriggerMode mode_;
    uint16_t threshold_;
    bool armed_;
    bool fired_;
    size_t position_;
    size_t samples_after_trigger_;
    size_t buffer_size_;
    size_t half_buffer_;
    uint16_t prev_sample_;
    
    // Автоматический уровень триггера
    uint64_t auto_sum_;
    uint32_t auto_count_;
    uint16_t auto_level_;
    
    // Приватные методы
    void update_auto_level(uint16_t sample);
    
public:
    Trigger(size_t buffer_size = 128);
    
    void start(TriggerMode mode, uint16_t threshold);
    TriggerState check_trigger(uint16_t sample);
    void reset();
    
    // Геттеры
    bool is_fired() const { return fired_; }
    size_t get_position() const { return position_; }
    uint16_t get_auto_level() const { return auto_level_; }
    bool is_armed() const { return armed_; }
    size_t get_buffer_size() const { return buffer_size_; }
    size_t get_half_buffer() const { return half_buffer_; }
};