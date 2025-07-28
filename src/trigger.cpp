#include "trigger.h"
#include <Arduino.h>

Trigger::Trigger(size_t buffer_size, size_t trigger_position) {
    mode_ = TriggerMode::FREE;
    threshold_ = 2048;
    hysteresis_ = 200; // Гистерезис по умолчанию
    armed_ = false;
    fired_ = false;
    ready_to_trigger_ = false;
    samples_after_trigger_ = 0;
    buffer_size_ = buffer_size;
    prev_sample_ = 2048;
    first_sample_ = true;
    trigger_position_ = trigger_position;
    
    // Автоматический уровень триггера
    auto_level_ = 2048;
}

void Trigger::start(TriggerMode mode, uint16_t threshold) {
    mode_ = mode;
    threshold_ = threshold;
    hysteresis_ = threshold / 40; // Гистерезис как 2.5% от порога
    armed_ = (mode_ != TriggerMode::FREE);
    fired_ = false;
    ready_to_trigger_ = false;
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

        // buffer not ready, continue work
        return {false, true};
    }
    
    // Обновляем автоматический уровень
    update_auto_level(sample);
    
    // Если триггер уже сработал, проверяем состояние записи
    if (fired_) {
        samples_after_trigger_++;

        // Если записали половину буфера после триггера
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
    
    // Если триггер еще не сработал, проверяем условия триггера
    bool trigger_condition = false;
    switch (mode_) {
        case TriggerMode::AUTO_RISE:
            // Для AUTO_RISE: сначала сигнал должен опуститься ниже threshold - hysteresis
            // потом подняться выше threshold + hysteresis
            if (!ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::AUTO_FALL:
            // Для AUTO_FALL: сначала сигнал должен подняться выше threshold + hysteresis
            // потом опуститься ниже threshold - hysteresis
            if (!ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::FIXED_RISE:
            // Для FIXED_RISE: сначала сигнал должен опуститься ниже threshold - hysteresis
            // потом подняться выше threshold + hysteresis
            if (!ready_to_trigger_ && prev_sample_ > threshold_ - hysteresis_ && sample <= threshold_ - hysteresis_) {
                ready_to_trigger_ = true;
            }
            if (ready_to_trigger_ && prev_sample_ < threshold_ + hysteresis_ && sample >= threshold_ + hysteresis_) {
                trigger_condition = true;
                ready_to_trigger_ = false;
            }
            break;
            
        case TriggerMode::FIXED_FALL:
            // Для FIXED_FALL: сначала сигнал должен подняться выше threshold + hysteresis
            // потом опуститься ниже threshold - hysteresis
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
    // Сбрасываем только уровень триггера
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
    // Обновляем среднее значение для автоматического уровня триггера
    auto_level_ = sample * 0.001 + auto_level_ * 0.999;
    
    // Для AUTO режимов используем вычисленный уровень
    if (mode_ == TriggerMode::AUTO_RISE || mode_ == TriggerMode::AUTO_FALL) {
        threshold_ = auto_level_;
    }
} 