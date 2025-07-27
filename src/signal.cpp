#include "signal.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstring>

Signal::Signal(const adc_channel_t* channels, size_t channel_count) 
    : channel_count_(0), adc_handle_(nullptr), read_task_handle_(nullptr),
      running_(false), stop_requested_(false) {
    
    // Ограничиваем количество каналов
    if (channel_count > MAX_CHANNELS) {
        channel_count = MAX_CHANNELS;
    }
    
    // Создаем семафоры
    mutex_ = xSemaphoreCreateMutex();
    start_semaphore_ = xSemaphoreCreateBinary();
    
    if (!mutex_ || !start_semaphore_) {
        Serial.println("Failed to create semaphores");
        return;
    }
    
    // Копируем каналы ADC
    for (size_t i = 0; i < channel_count; i++) {
        channels_[channel_count_] = channels[i];
        channel_count_++;
    }
    
    // Инициализируем структуры данных
    for (size_t i = 0; i < channel_count_; i++) {
        stats_[i] = SignalStats();
        buffer_indices_[i] = 0;
        memset(signal_buffers_[i], 0, sizeof(signal_buffers_[i]));
    }
    
    // Создаем задачу, которая будет ожидать start()
    TaskHandle_t task_handle;
    BaseType_t result = xTaskCreate(
        read_task_wrapper,
        "SignalReader",
        4096,
        this,
        5,
        &task_handle
    );
    
    read_task_handle_ = task_handle;
    
    if (result != pdPASS) {
        Serial.println("Failed to create read task");
    }
}

Signal::~Signal() {
    // Останавливаем мониторинг
    stop();
    
    // Устанавливаем флаг для завершения задачи
    stop_requested_ = true;
    
    // Даем сигнал задаче, чтобы она вышла из ожидания
    if (start_semaphore_) {
        xSemaphoreGive((SemaphoreHandle_t)start_semaphore_);
    }
    
    // Ждем немного, чтобы задача успела завершиться
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Удаляем задачу
    if (read_task_handle_) {
        vTaskDelete((TaskHandle_t)read_task_handle_);
        read_task_handle_ = nullptr;
    }
    
    // Удаляем семафоры
    if (mutex_) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex_);
        mutex_ = nullptr;
    }
    
    if (start_semaphore_) {
        vSemaphoreDelete((SemaphoreHandle_t)start_semaphore_);
        start_semaphore_ = nullptr;
    }
}

bool Signal::start() {
    if (running_ || channel_count_ == 0) {
        return false;
    }
    
    // Конфигурация ADC continuous
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = CONV_FRAME_SIZE * 4,
        .conv_frame_size = CONV_FRAME_SIZE,
    };
    
    // Создаем handle ADC
    esp_err_t err = adc_continuous_new_handle(&adc_config, &adc_handle_);
    if (err != ESP_OK) {
        Serial.printf("ADC continuous handle creation failed: %s\n", esp_err_to_name(err));
        return false;
    }
    
    // Настраиваем паттерны для всех каналов
    adc_digi_pattern_config_t adc_pattern[MAX_CHANNELS];
    for (size_t i = 0; i < channel_count_; i++) {
        adc_pattern[i] = {
            .atten = ADC_ATTEN_DB_12,
            .channel = channels_[i],
            .unit = ADC_UNIT_1,
            .bit_width = ADC_BITWIDTH_12,
        };
    }
    
    adc_continuous_config_t dig_cfg = {
        .pattern_num = static_cast<uint32_t>(channel_count_),
        .adc_pattern = adc_pattern,
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    
    // Конфигурируем ADC
    err = adc_continuous_config(adc_handle_, &dig_cfg);
    if (err != ESP_OK) {
        Serial.printf("ADC continuous config failed: %s\n", esp_err_to_name(err));
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
        return false;
    }
    
    // Запускаем ADC
    err = adc_continuous_start(adc_handle_);
    if (err != ESP_OK) {
        Serial.printf("ADC continuous start failed: %s\n", esp_err_to_name(err));
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
        return false;
    }
    
    running_ = true;
    stop_requested_ = false;
    
    // Разблокируем задачу чтения
    xSemaphoreGive((SemaphoreHandle_t)start_semaphore_);
    
    Serial.println("Signal monitoring started");
    return true;
}

void Signal::stop() {
    if (!running_) {
        return;
    }
    
    stop_requested_ = true;
    
    if (adc_handle_) {
        adc_continuous_stop(adc_handle_);
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
    }
    
    running_ = false;
    Serial.println("Signal monitoring paused - task will wait for next start signal");
}

void Signal::reset_stats() {
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < channel_count_; i++) {
            stats_[i] = SignalStats(); // Сброс в конструктор по умолчанию
        }
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
    }
}

SignalStats Signal::get_stats(size_t index) const {
    SignalStats result;
    
    if (index >= channel_count_) {
        return result;
    }
    
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        result = stats_[index];
        
        // Вычисляем среднее значение
        if (result.sample_count > 0) {
            result.avg_value = static_cast<float>(result.total_value) / result.sample_count;
        }
        
        // Вычисляем частоту
        if (result.crossing_delta_count > 0) {
            float avg_delta = static_cast<float>(result.total_crossing_delta) / result.crossing_delta_count;
            if (avg_delta > 0) {
                result.frequency = static_cast<float>(SAMPLE_RATE) / avg_delta;
            }
        }
        
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
    }
    
    return result;
}

bool Signal::get_buffer(size_t index, size_t size, uint16_t* buffer) const {
    if (!buffer || index >= channel_count_ || size == 0) {
        return false;
    }
    
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        size_t copy_size = (size < SIGNAL_BUFFER_SIZE) ? size : SIGNAL_BUFFER_SIZE;
        size_t start_idx = buffer_indices_[index];
        
        // Копируем данные из кольцевого буфера
        for (size_t i = 0; i < copy_size; i++) {
            size_t buf_idx = (start_idx + i) % SIGNAL_BUFFER_SIZE;
            buffer[i] = signal_buffers_[index][buf_idx];
        }
        
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
        return true;
    }
    
    return false;
}

void Signal::read_task_wrapper(void* parameter) {
    Signal* signal = static_cast<Signal*>(parameter);
    signal->read_task();
}

void Signal::read_task() {
    uint8_t adc_read_buffer[CONV_FRAME_SIZE];
    uint32_t total_sample_count = 0;
    
    while (true) {
        // Ждем сигнала для начала работы
        xSemaphoreTake((SemaphoreHandle_t)start_semaphore_, portMAX_DELAY);
        
        // Сбрасываем счетчик при каждом запуске
        total_sample_count = 0;
        
        while (!stop_requested_) {
            uint32_t current_bytes_read;
            
            esp_err_t ret = adc_continuous_read(adc_handle_, adc_read_buffer, 
                                              CONV_FRAME_SIZE, &current_bytes_read, 100);
            
            if (ret == ESP_OK && current_bytes_read > 0) {
                int samples_read = current_bytes_read / SOC_ADC_DIGI_RESULT_BYTES;
                
                for (int i = 0; i < samples_read; i++) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)
                        &adc_read_buffer[i * SOC_ADC_DIGI_RESULT_BYTES];
                    
                    // Находим индекс канала
                    size_t channel_index = channel_count_;
                    for (size_t j = 0; j < channel_count_; j++) {
                        if (channels_[j] == p->type1.channel) {
                            channel_index = j;
                            break;
                        }
                    }
                    
                    if (channel_index < channel_count_) {
                        process_sample(channel_index, p->type1.data);
                        total_sample_count++;
                    }
                }
            } else if (ret == ESP_ERR_TIMEOUT) {
                // Нормальный timeout, продолжаем
                continue;
            } else {
                // Ошибка чтения
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        
        Serial.println("Read task paused, waiting for next start signal");
    }
}

void Signal::process_sample(size_t channel_index, uint16_t sample) {
    if (channel_index >= channel_count_) {
        return;
    }
    
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        SignalStats& stat = stats_[channel_index];
        
        // Обновляем статистику
        if (sample < stat.min_value) stat.min_value = sample;
        if (sample > stat.max_value) stat.max_value = sample;
        stat.total_value += sample;
        stat.sample_count++;
        
        // Сохраняем в буфер
        signal_buffers_[channel_index][buffer_indices_[channel_index]] = sample;
        buffer_indices_[channel_index] = (buffer_indices_[channel_index] + 1) % SIGNAL_BUFFER_SIZE;
        
        // Вычисляем частоту
        calculate_frequency(stat, sample, stat.sample_count);
        
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
    }
}

void Signal::calculate_frequency(SignalStats& stats, uint16_t sample, uint32_t absolute_sample_count) {
    if (!stats.initialized) {
        stats.initialized = true;
        return;
    }
    
    // Простой алгоритм детекции частоты через zero crossing
    float avg_value = stats.sample_count > 0 ? 
        static_cast<float>(stats.total_value) / stats.sample_count : 2048;
    
    uint16_t signal_range = (stats.max_value > stats.min_value) ? 
        (stats.max_value - stats.min_value) : 0;
    uint16_t hysteresis = signal_range / 5;
    float upper_threshold = avg_value + hysteresis / 2.0;
    float lower_threshold = avg_value - hysteresis / 2.0;
    
    if (!stats.signal_was_high && sample > upper_threshold) {
        stats.signal_was_high = true;
        
        if (stats.last_crossing_time > 0) {
            uint32_t delta = absolute_sample_count - stats.last_crossing_time;
            if (delta >= 4) { // Минимум 200 μs между переходами при 20kHz
                stats.total_crossing_delta += delta;
                stats.crossing_delta_count++;
            }
        }
        stats.last_crossing_time = absolute_sample_count;
    } else if (stats.signal_was_high && sample < lower_threshold) {
        stats.signal_was_high = false;
    }
}
