#include "trigger.h"
#include <Arduino.h>

Trigger::Trigger(size_t buffer_size) {
    mode_ = TriggerMode::FREE;
    threshold_ = 2048;
    armed_ = false;
    fired_ = false;
    samples_after_trigger_ = 0;
    buffer_size_ = buffer_size;
    half_buffer_ = buffer_size_ / 2;
    prev_sample_ = 2048;
    first_sample_ = true;
    
    // Автоматический уровень триггера
    auto_level_ = 2048;
}

void Trigger::start(TriggerMode mode, uint16_t threshold) {
    mode_ = mode;
    threshold_ = threshold;
    armed_ = (mode_ != TriggerMode::FREE);
    fired_ = false;
    samples_after_trigger_ = 0;
    prev_sample_ = threshold;
    first_sample_ = true;
    
    // Сбрасываем счетчики для автоматического уровня триггера
    auto_level_ = threshold;
}

TriggerState Trigger::check_trigger(uint16_t sample) {
    // Если это первый сэмпл, возвращаем специальное состояние
    if (first_sample_) {
        first_sample_ = false;
        prev_sample_ = sample;
        TriggerState state;
        state.buffer_ready = false;
        state.continue_work = true;
        return state;
    }
    
    // Обновляем автоматический уровень
    update_auto_level(sample);
    
    TriggerState state;
    state.buffer_ready = false;
    state.continue_work = true;
    
    if(mode_ == TriggerMode::FREE) {
        if(samples_after_trigger_ < buffer_size_) {
            samples_after_trigger_++;
            return TriggerState{false, true};
        }

        return TriggerState{true, true};
    }
    
    // Если триггер уже сработал, проверяем состояние записи
    if (fired_) {
        samples_after_trigger_++;
        
        // Если записали половину буфера после триггера
        if (samples_after_trigger_ >= half_buffer_) {
            state.buffer_ready = true;
            state.continue_work = false; // Останавливаем работу
            prev_sample_ = sample; // Обновляем предыдущий сэмпл
            return state;
        }
        
        // Если еще не дописали половину буфера
        state.buffer_ready = false;
        state.continue_work = true; // Продолжаем записывать
        prev_sample_ = sample; // Обновляем предыдущий сэмпл
        return state;
    }
    
    // Если триггер еще не сработал, проверяем условия триггера
    bool trigger_condition = false;
    switch (mode_) {
        case TriggerMode::AUTO_RISE:
            trigger_condition = (prev_sample_ < threshold_) && (sample >= threshold_);
            break;
            
        case TriggerMode::AUTO_FALL:
            trigger_condition = (prev_sample_ > threshold_) && (sample <= threshold_);
            break;
            
        case TriggerMode::FIXED_RISE:
            trigger_condition = (prev_sample_ < threshold_) && (sample >= threshold_);
            break;
            
        case TriggerMode::FIXED_FALL:
            trigger_condition = (prev_sample_ > threshold_) && (sample <= threshold_);
            break;
            
        case TriggerMode::FREE:
        default:
            trigger_condition = false;
            break;
    }
    
    if (trigger_condition) {
        fired_ = true;
        samples_after_trigger_ = 0;
        state.buffer_ready = false;
        state.continue_work = true; // Продолжаем записывать после триггера
    } else {
        state.buffer_ready = false;
        state.continue_work = true;
    }
    
    prev_sample_ = sample; // Обновляем предыдущий сэмпл
    return state;
}

void Trigger::reset_level() {
    // Сбрасываем только уровень триггера
    first_sample_ = true;
    auto_level_ = threshold_;

    reset();
}

void Trigger::reset() {
    fired_ = false;
    position_ = 0;
    armed_ = (mode_ != TriggerMode::FREE);
    samples_after_trigger_ = 0;
    prev_sample_ = threshold_;
}

void Trigger::update_auto_level(uint16_t sample) {
    // Обновляем среднее значение для автоматического уровня триггера
    auto_level_ = sample * 0.001 + auto_level_ * 0.999;
    
    // Для AUTO режимов используем вычисленный уровень
    if (mode_ == TriggerMode::AUTO_RISE || mode_ == TriggerMode::AUTO_FALL) {
        threshold_ = auto_level_;
    }
} 