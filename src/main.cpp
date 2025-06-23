#include <Arduino.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <esp_err.h>

// Pin Configuration
const int ledcPin = 26;

// LEDC Configuration
const int ledcChannel = 0;
const int ledcResolution = 8;
const int ledcFreq = 100000; // 100 kHz

// Sine wave configuration
const uint32_t sampling_frequency = 20000; // 20 kHz
const float sine_frequency = 440.0; // 440 Hz

// Globals for ISR
hw_timer_t *timer = NULL;
volatile uint32_t phase_accumulator = 0;
// (sine_frequency / sampling_frequency) * 2^32
const uint32_t phase_increment = (uint32_t)((sine_frequency / (float)sampling_frequency) * 4294967296.0);
uint8_t sin_lut[256];

// I2S and ADC configuration
#define I2S_ADC_UNIT                ADC_UNIT_1
#define I2S_ADC_CHANNEL             ADC1_CHANNEL_0 // SENSOR_VP
#define I2S_PORT                    I2S_NUM_0
#define I2S_SAMPLE_RATE             (20000)
#define I2S_READ_BUFFER_SIZE        (256)

// Signal statistics tracking
unsigned long last_stats_time = 0;
uint16_t min_sample_value = UINT16_MAX;
uint16_t max_sample_value = 0;
uint64_t total_sample_value = 0;
uint32_t total_sample_count = 0;

// Frequency calculation
uint32_t zero_crossings = 0;
bool signal_was_high = false;
float last_avg_value = 0;
uint16_t last_min_value = 0;
uint16_t last_max_value = 0;

void IRAM_ATTR onTimer() {
    phase_accumulator += phase_increment;
    uint8_t index = phase_accumulator >> 24;
    ledcWrite(ledcChannel, sin_lut[index]);
}

void setup_i2s_adc() {
    // I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // Explicitly set to mono mode
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = I2S_READ_BUFFER_SIZE,
        .use_apll = false
    };

    esp_err_t err;

    // Install and start I2S driver
    err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("I2S driver install failed: %s\n", esp_err_to_name(err));
    } else {
        Serial.println("I2S driver installed successfully.");
    }
    
    // Configure ADC
    err = i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL);
    if (err != ESP_OK) {
        Serial.printf("I2S set ADC mode failed: %s\n", esp_err_to_name(err));
    } else {
        Serial.println("I2S ADC mode set successfully.");
    }

    // Enable ADC after setup
    err = i2s_adc_enable(I2S_PORT);
    if (err != ESP_OK) {
        Serial.printf("I2S ADC enable failed: %s\n", esp_err_to_name(err));
    } else {
        Serial.println("I2S ADC enabled successfully.");
    }
}

void setup() {
    delay(1000);
    Serial.begin(115200);

    // Generate sine lookup table
    for (int i = 0; i < 256; i++) {
        float angle = 2.0 * PI * (float)i / 256.0;
        sin_lut[i] = (uint8_t)((sin(angle) + 1.0) * 0.5 * 255.0);
    }

    // Configure LEDC
    ledcSetup(ledcChannel, ledcFreq, ledcResolution);
    ledcAttachPin(ledcPin, ledcChannel);

    // Configure Timer
    // APB_CLK is 80MHz, prescaler of 80 gives 1MHz timer clock
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    // Set timer to fire at the configured sampling frequency
    timerAlarmWrite(timer, 1000000 / sampling_frequency, true);
    timerAlarmEnable(timer);

    setup_i2s_adc();
}

void loop() {
    // Read from I2S in a loop to drain the DMA buffer.
    uint16_t i2s_read_buffer[I2S_READ_BUFFER_SIZE];
    size_t total_bytes_read_in_cycle = 0;
    size_t current_bytes_read;
    do {
        // Read with 0 timeout to avoid blocking.
        i2s_read(I2S_PORT, i2s_read_buffer, sizeof(i2s_read_buffer), &current_bytes_read, 0);
        if (current_bytes_read > 0) {
            total_bytes_read_in_cycle += current_bytes_read;
            int samples_read = current_bytes_read / sizeof(uint16_t);
            for (int i = 0; i < samples_read; i++) {
                uint16_t sample = i2s_read_buffer[i];
                if (sample < min_sample_value) min_sample_value = sample;
                if (sample > max_sample_value) max_sample_value = sample;
                total_sample_value += sample;

                // Frequency detection with hysteresis
                uint16_t signal_range = (last_max_value > last_min_value) ? (last_max_value - last_min_value) : 0;
                uint16_t hysteresis = signal_range / 3;
                float upper_threshold = last_avg_value + hysteresis;
                float lower_threshold = last_avg_value - hysteresis;

                if (!signal_was_high && sample > upper_threshold) {
                    signal_was_high = true;
                    zero_crossings++;
                } else if (signal_was_high && sample < lower_threshold) {
                    signal_was_high = false;
                }
            }
            total_sample_count += samples_read;
        }
    } while (current_bytes_read > 0);

    // Check if the DMA buffer is getting close to full, which indicates the loop is not running fast enough.
    if (total_bytes_read_in_cycle >= I2S_READ_BUFFER_SIZE * 7) {
        Serial.printf("Warning: I2S DMA buffer nearly full! Read %u bytes.\n", (unsigned int)total_bytes_read_in_cycle);
    }

    // Check if one second has passed
    if (millis() - last_stats_time >= 1000) {
        if (total_sample_count > 0) {
            float avg_value = (float)total_sample_value / total_sample_count;
            float frequency = (float)zero_crossings; // Each full wave is counted once
            Serial.printf("Signal | min: %u, max: %u, avg: %.2f, freq: %.1f Hz\n",
                          min_sample_value,
                          max_sample_value,
                          avg_value,
                          frequency);
            
            // Store values for next second's calculation
            last_avg_value = avg_value;
            last_min_value = min_sample_value;
            last_max_value = max_sample_value;
        }
        
        // Reset stats for the next second
        last_stats_time = millis();
        min_sample_value = UINT16_MAX;
        max_sample_value = 0;
        total_sample_value = 0;
        total_sample_count = 0;
        zero_crossings = 0;
    }

    delay(10);
}