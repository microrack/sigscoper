#include "sigscoper.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstring>
#include <algorithm>

Sigscoper::Sigscoper() : Sigscoper(SIGNAL_BUFFER_SIZE) {
}

Sigscoper::Sigscoper(size_t buffer_size) : trigger_(buffer_size, TRIGGER_POSITON) {
    // Configuration initialization
    config_.channel_count = 0;
    config_.trigger_mode = TriggerMode::FREE;
    config_.trigger_level = 2048;
    memset(config_.channels, 0, sizeof(config_.channels));
    
    // ADC initialization
    adc_handle_ = nullptr;
    
    // Task initialization
    read_task_handle_ = nullptr;
    
    // Synchronization initialization
    mutex_ = nullptr;
    start_semaphore_ = nullptr;
    
    // State initialization
    running_ = false;
    stop_requested_ = false;
    is_ready_ = false;
    decimation_factor_ = 1;
    sample_counter_ = 0;
    
    // Data initialization
    memset(signal_buffers_, 0, sizeof(signal_buffers_));
    memset(buffer_indices_, 0, sizeof(buffer_indices_));
    
    // Median filter initialization
    memset(median_buffer_, 0, sizeof(median_buffer_));
    median_index_ = 0;
    median_initialized_ = false;
}

Sigscoper::~Sigscoper() {
    stop();
    stop_requested_ = true;
    if (start_semaphore_) {
        xSemaphoreGive((SemaphoreHandle_t)start_semaphore_);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Give task time to exit
    
    if (read_task_handle_) {
        vTaskDelete((TaskHandle_t)read_task_handle_);
        read_task_handle_ = nullptr;
    }
    
    if (mutex_) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex_);
        mutex_ = nullptr;
    }
    
    if (start_semaphore_) {
        vSemaphoreDelete((SemaphoreHandle_t)start_semaphore_);
        start_semaphore_ = nullptr;
    }
    
    if (adc_handle_) {
        adc_continuous_stop(adc_handle_);
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
    }
}

bool Sigscoper::begin() {
    // Create semaphores
    mutex_ = xSemaphoreCreateMutex();
    start_semaphore_ = xSemaphoreCreateBinary();
    
    if (!mutex_ || !start_semaphore_) {
        return false;
    }
    
    // Configure ADC
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = CONV_FRAME_SIZE * 4,
        .conv_frame_size = CONV_FRAME_SIZE,
    };
    
    esp_err_t err = adc_continuous_new_handle(&adc_config, &adc_handle_);
    if (err != ESP_OK) {
        return false;
    }
    
    // Create task
    BaseType_t task_created = xTaskCreate(
        read_task_wrapper,
        "signal_read_task",
        4096,
        this,
        5,
        &read_task_handle_
    );
    
    if (task_created != pdPASS) {
        return false;
    }
    
    return true;
}

bool Sigscoper::start(const SigscoperConfig& config) {
    if (running_) {
        Serial.println("::start: Sigscoper already run");
        return false;
    }
    
    // Check configuration
    if (config.channel_count == 0 || config.channel_count > MAX_CHANNELS) {
        return false;
    }
    
    // Save configuration
    config_ = config;
    
    // Configure patterns for all channels
    adc_digi_pattern_config_t adc_pattern[MAX_CHANNELS];
    for (size_t i = 0; i < config_.channel_count; i++) {
        adc_pattern[i] = {
            .atten = ADC_ATTEN_DB_12,
            .channel = config_.channels[i],
            .unit = ADC_UNIT_1,
            .bit_width = ADC_BITWIDTH_12,
        };
    }
    
    adc_continuous_config_t dig_cfg = {
        .pattern_num = static_cast<uint32_t>(config_.channel_count),
        .adc_pattern = adc_pattern,
        .sample_freq_hz = (config_.sampling_rate < 20000) ? 
            ((20000 + config_.sampling_rate - 1) / config_.sampling_rate) * config_.sampling_rate : 
            config_.sampling_rate,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    
    // Check if ADC is already running
    if (adc_handle_) {
        // Try to stop if it's running
        // adc_continuous_stop(adc_handle_);
    }
    
    esp_err_t err = adc_continuous_config(adc_handle_, &dig_cfg);
    if (err != ESP_OK) {
        // If configuration failed, deinit handle
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
        return false;
    }
    
    // Start ADC
    err = adc_continuous_start(adc_handle_);
    if (err != ESP_OK) {
        adc_continuous_deinit(adc_handle_);
        adc_handle_ = nullptr;
        return false;
    }
    
    // Configure trigger
    trigger_.start(config_.trigger_mode, config_.trigger_level);
    
    // Reset buffers
    memset(signal_buffers_, 0, sizeof(signal_buffers_));
    memset(buffer_indices_, 0, sizeof(buffer_indices_));
    
    // Reset trigger
    trigger_.reset_level();
    
    // Reset ready state
    is_ready_ = false;
    
    // Calculate decimation factor
    if (config_.sampling_rate < 20000) {
        decimation_factor_ = ((20000 + config_.sampling_rate - 1) / config_.sampling_rate);
    } else {
        decimation_factor_ = 1;
    }
    sample_counter_ = 0;
    
    // Reset median filter
    memset(median_buffer_, 0, sizeof(median_buffer_));
    median_index_ = 0;
    median_initialized_ = false;

    running_ = true;
    stop_requested_ = false;
    
    // Start the task
    xSemaphoreGive((SemaphoreHandle_t)start_semaphore_);
    
    return true;
}

void Sigscoper::restart() {
    running_ = true;
    stop_requested_ = false;
    is_ready_ = false;
    trigger_.reset();
    xSemaphoreGive((SemaphoreHandle_t)start_semaphore_);
}

void Sigscoper::stop() {
    if (!running_) {
        Serial.println("::stop: Sigscoper is not running");
        return;
    }
    
    stop_requested_ = true;
    
    if (adc_handle_) {
        adc_continuous_stop(adc_handle_);
    }
    
    running_ = false;
    Serial.println("Sigscoper monitoring paused - task will wait for next start signal");
}



bool Sigscoper::get_stats(size_t index, SigscoperStats* stats) const {
    if (!stats || index >= config_.channel_count) {
        return false;
    }

    stats->min_value = UINT16_MAX;
    stats->max_value = 0;
    stats->avg_value = 0;
    stats->frequency = 0;
    
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        // Calculate statistics directly from ring buffer
        uint64_t sum = 0;
        uint32_t valid_samples = 0;
        size_t start_idx = buffer_indices_[index];
        
        for (size_t i = 0; i < SIGNAL_BUFFER_SIZE; i++) {
            size_t buf_idx = (start_idx + i) % SIGNAL_BUFFER_SIZE;
            uint16_t sample = signal_buffers_[index][buf_idx];
            if (sample > 0) { // Count only valid samples
                if (sample < stats->min_value) stats->min_value = sample;
                if (sample > stats->max_value) stats->max_value = sample;
                sum += sample;
                valid_samples++;
            }
        }
        
        // Calculate average value
        if (valid_samples > 0) {
            stats->avg_value = static_cast<float>(sum) / valid_samples;
        }
        
        // Calculate frequency directly from ring buffer
        stats->frequency = calculate_frequency_from_buffer_direct(index);
        
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
        return true;
    }
    
    return false;
}

float Sigscoper::calculate_frequency_from_buffer_direct(size_t channel_index) const {
    if (SIGNAL_BUFFER_SIZE < 2) {
        return 0.0f;
    }
    
    // Simple frequency detection algorithm through zero crossing
    uint64_t sum = 0;
    uint32_t valid_samples = 0;
    size_t start_idx = buffer_indices_[channel_index];
    
    // Calculate average value
    for (size_t i = 0; i < SIGNAL_BUFFER_SIZE; i++) {
        size_t buf_idx = (start_idx + i) % SIGNAL_BUFFER_SIZE;
        uint16_t sample = signal_buffers_[channel_index][buf_idx];
        if (sample > 0) {
            sum += sample;
            valid_samples++;
        }
    }
    
    if (valid_samples == 0) {
        return 0.0f;
    }
    
    float avg_value = static_cast<float>(sum) / valid_samples;
    
    // Find min and max for hysteresis calculation
    uint16_t min_val = UINT16_MAX;
    uint16_t max_val = 0;
    
    for (size_t i = 0; i < SIGNAL_BUFFER_SIZE; i++) {
        size_t buf_idx = (start_idx + i) % SIGNAL_BUFFER_SIZE;
        uint16_t sample = signal_buffers_[channel_index][buf_idx];
        if (sample > 0) {
            if (sample < min_val) min_val = sample;
            if (sample > max_val) max_val = sample;
        }
    }
    
    uint16_t signal_range = (max_val > min_val) ? (max_val - min_val) : 0;
    uint16_t hysteresis = signal_range / 5;
    float upper_threshold = avg_value + hysteresis / 2.0;
    float lower_threshold = avg_value - hysteresis / 2.0;
    
    // Count crossings through average value
    bool signal_was_high = false;
    uint32_t crossing_count = 0;
    uint64_t total_delta = 0;
    uint32_t last_crossing_index = 0;
    
    for (size_t i = 0; i < SIGNAL_BUFFER_SIZE; i++) {
        size_t buf_idx = (start_idx + i) % SIGNAL_BUFFER_SIZE;
        uint16_t sample = signal_buffers_[channel_index][buf_idx];
        if (sample > 0) {
            if (!signal_was_high && sample > upper_threshold) {
                signal_was_high = true;
                
                if (crossing_count > 0) {
                    uint32_t delta = i - last_crossing_index;
                    if (delta >= 4) { // Minimum 200 μs between transitions at 20kHz
                        total_delta += delta;
                    }
                }
                last_crossing_index = i;
                crossing_count++;
            } else if (signal_was_high && sample < lower_threshold) {
                signal_was_high = false;
            }
        }
    }
    
    // Calculate frequency with decimation consideration
    if (crossing_count > 1 && total_delta > 0) {
        float avg_delta = static_cast<float>(total_delta) / (crossing_count - 1);
        if (avg_delta > 0) {
            // Consider decimation: effective sampling rate
            float effective_sample_rate = static_cast<float>(config_.sampling_rate);
            return effective_sample_rate / avg_delta;
        }
    }
    
    return 0.0f;
}

bool Sigscoper::get_buffer(size_t index, size_t size, uint16_t* buffer, size_t* position) const {
    if (!buffer || index >= config_.channel_count || size == 0) {
        return false;
    }
    
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        size_t copy_size = (size < SIGNAL_BUFFER_SIZE) ? size : SIGNAL_BUFFER_SIZE;
        size_t start_idx = buffer_indices_[index];
        
        // Copy data from ring buffer
        for (size_t i = 0; i < copy_size; i++) {
            size_t buf_idx = (start_idx + i) % SIGNAL_BUFFER_SIZE;
            buffer[i] = signal_buffers_[index][buf_idx];
        }

        *position = start_idx;
        
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
        return true;
    }
    
    return false;
}

void Sigscoper::read_task_wrapper(void* parameter) {
    Sigscoper* signal = static_cast<Sigscoper*>(parameter);
    signal->read_task();
}

void Sigscoper::read_task() {
    uint8_t adc_read_buffer[CONV_FRAME_SIZE];
    uint32_t total_sample_count = 0;
    
    while (true) {
        // Wait for signal to start work
        xSemaphoreTake((SemaphoreHandle_t)start_semaphore_, portMAX_DELAY);
        
        // Reset counter on each start
        total_sample_count = 0;
        trigger_.reset();
        
        while (!stop_requested_) {
            uint32_t current_bytes_read;
            
            esp_err_t ret = adc_continuous_read(adc_handle_, adc_read_buffer, 
                                              CONV_FRAME_SIZE, &current_bytes_read, 100);
            
            if (ret == ESP_OK && current_bytes_read > 0) {
                int samples_read = current_bytes_read / SOC_ADC_DIGI_RESULT_BYTES;
                
                for (int i = 0; i < samples_read; i++) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)
                        &adc_read_buffer[i * SOC_ADC_DIGI_RESULT_BYTES];
                    
                    // Find channel index
                    size_t channel_index = config_.channel_count;
                    for (size_t j = 0; j < config_.channel_count; j++) {
                        if (config_.channels[j] == p->type1.channel) {
                            channel_index = j;
                            break;
                        }
                    }
                    
                    if (channel_index < config_.channel_count) {
                        uint16_t current_sample = p->type1.data;
                        uint16_t filtered_sample = apply_median_filter(current_sample);
                        
                        // Decimation: process only every N-th sample
                        sample_counter_++;
                        if (sample_counter_ >= decimation_factor_) {
                            // Check trigger only for first channel on decimated samples
                            if (channel_index == 0) {
                                TriggerState state = trigger_.check_trigger(filtered_sample);
                                
                                // If buffer is ready, set flag
                                if (state.buffer_ready) {
                                    is_ready_ = true;
                                }
                                
                                // If need to stop work
                                if (!state.continue_work) {
                                    stop_requested_ = true;
                                    break; // Exit reading loop
                                }
                            }
                            
                            process_sample(channel_index, filtered_sample);
                            sample_counter_ = 0;
                        }
                        total_sample_count++;
                    }
                }
                

            } else if (ret == ESP_ERR_TIMEOUT) {
                // Normal timeout, continue
                continue;
            } else {
                // Read error
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
    }
}



void Sigscoper::process_sample(size_t channel_index, uint16_t sample) {
    if (channel_index >= config_.channel_count) {
        return;
    }
    
    if (xSemaphoreTake((SemaphoreHandle_t)mutex_, portMAX_DELAY) == pdTRUE) {
        // Save filtered sample to buffer
        signal_buffers_[channel_index][buffer_indices_[channel_index]] = sample;
        buffer_indices_[channel_index] = (buffer_indices_[channel_index] + 1) % SIGNAL_BUFFER_SIZE;
        
        xSemaphoreGive((SemaphoreHandle_t)mutex_);
    }
}

uint16_t Sigscoper::apply_median_filter(uint16_t sample) {
    // Simple median filter implementation with buffer
    static uint16_t median_buffer[MEDIAN_FILTER_WINDOW];
    static size_t median_index = 0;
    static bool median_initialized = false;
    
    // Add new sample to buffer
    median_buffer[median_index] = sample;
    median_index = (median_index + 1) % MEDIAN_FILTER_WINDOW;
    
    // If buffer is not yet filled, return original sample
    if (!median_initialized && median_index == 0) {
        median_initialized = true;
    }
    
    if (!median_initialized) {
        return sample;
    }
    
    // Copy buffer for sorting
    uint16_t temp_buffer[MEDIAN_FILTER_WINDOW];
    memcpy(temp_buffer, median_buffer, sizeof(median_buffer));
    
    // Sort buffer
    std::sort(temp_buffer, temp_buffer + MEDIAN_FILTER_WINDOW);
    
    // Return median value (middle element)
    return temp_buffer[MEDIAN_FILTER_WINDOW / 2];
}
