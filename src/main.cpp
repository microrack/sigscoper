#include <Arduino.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <esp_err.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buffer for graph
uint16_t signal_graph_buffer[SCREEN_WIDTH];
int signal_graph_index = 0;
bool zero_crossing_buffer[SCREEN_WIDTH];
int zero_crossing_index = 0;

// Pin Configuration
const int ledcPin = 26;

// LEDC Configuration
// const int ledcChannel = 0;  // Удалено: больше не нужно в новом API
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
bool signal_was_high = false;
float last_avg_value = 0;
uint16_t last_min_value = 0;
uint16_t last_max_value = 0;
uint32_t last_crossing_time = 0;
uint64_t total_crossing_delta = 0;
uint32_t crossing_delta_count = 0;

void IRAM_ATTR onTimer() {
    phase_accumulator += phase_increment;
    uint8_t index = phase_accumulator >> 24;
    ledcWrite(ledcPin, sin_lut[index]);  // Изменено: используем pin вместо channel
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

    // Initialize OLED display
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }

    display.setRotation(2);
    memset(signal_graph_buffer, 0, sizeof(signal_graph_buffer));
    memset(zero_crossing_buffer, false, sizeof(zero_crossing_buffer));

    // Generate sine lookup table
    for (int i = 0; i < 256; i++) {
        float angle = 2.0 * PI * (float)i / 256.0;
        sin_lut[i] = (uint8_t)((sin(angle) + 1.0) * 0.5 * 255.0);
    }

    // Configure LEDC (новый API)
    ledcAttach(ledcPin, ledcFreq, ledcResolution);

    // Configure Timer (новый API)
    timer = timerBegin(1000000);  // Устанавливаем частоту таймера 1MHz
    timerAttachInterrupt(timer, &onTimer);   // Убран параметр edge
    timerAlarm(timer, 1000000 / sampling_frequency, true, 0);  // Интервал для достижения sampling_frequency

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

                // Store sample for graph
                signal_graph_buffer[signal_graph_index] = sample;
                zero_crossing_buffer[signal_graph_index] = false;
                signal_graph_index = (signal_graph_index + 1) % SCREEN_WIDTH;

                // Frequency detection with hysteresis
                uint16_t signal_range = (last_max_value > last_min_value) ? (last_max_value - last_min_value) : 0;
                uint16_t hysteresis = signal_range / 5;
                float upper_threshold = last_avg_value + hysteresis / 2.0;
                float lower_threshold = last_avg_value - hysteresis / 2.0;

                uint32_t current_absolute_sample = total_sample_count + i;

                if (!signal_was_high && sample > upper_threshold) {
                    signal_was_high = true;
                    // Check minimum time between crossings (200 μs = 4 samples at 20kHz)
                    if (last_crossing_time > 0) {
                        uint32_t delta = current_absolute_sample - last_crossing_time;
                        if (delta >= 4) { // 200 μs minimum
                            total_crossing_delta += delta;
                            crossing_delta_count++;
                            // Mark zero crossing in buffer
                            zero_crossing_buffer[(signal_graph_index - 1 + SCREEN_WIDTH) % SCREEN_WIDTH] = true;
                        }
                    }
                    last_crossing_time = current_absolute_sample;
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

    if (millis() - last_stats_time >= 10) {
        if (total_sample_count > 0) {
            float avg_value = (float)total_sample_value / total_sample_count;
            
            float frequency = 0.0;
            if (crossing_delta_count > 0) {
                float avg_delta = (float)total_crossing_delta / crossing_delta_count;
                if (avg_delta > 0) {
                    frequency = (float)I2S_SAMPLE_RATE / avg_delta;
                }
            }

            /*
              Serial.printf("Signal | min: %u, max: %u, avg: %.2f, freq: %.1f Hz\n",
                            min_sample_value,
                            max_sample_value,
                            avg_value,
                            frequency);
             // */
  
            // Draw on OLED
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.printf("%u %u %.1f", min_sample_value, max_sample_value, frequency);

            // Draw graph
            int graph_y = 10;
            int graph_height = SCREEN_HEIGHT - graph_y;

            for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
                int read_idx1 = (signal_graph_index + i) % SCREEN_WIDTH;
                int read_idx2 = (signal_graph_index + i + 1) % SCREEN_WIDTH;

                int y1 = map(signal_graph_buffer[read_idx1], 0, 4096, graph_y + graph_height, graph_y);
                int y2 = map(signal_graph_buffer[read_idx2], 0, 4096, graph_y + graph_height, graph_y);
                
                display.drawLine(i, y1, i + 1, y2, SSD1306_WHITE);
            }

            // Draw average line
            int avg_y = map((int)last_avg_value, 0, 4096, graph_y + graph_height, graph_y);
            for (int i = 0; i < SCREEN_WIDTH; i += 4) {
                display.drawPixel(i, avg_y, SSD1306_WHITE);
            }

            // Draw zero crossings
            for (int i = 0; i < SCREEN_WIDTH; i++) {
                int read_idx = (signal_graph_index + i) % SCREEN_WIDTH;
                if (zero_crossing_buffer[read_idx]) {
                    int cross_y = map(signal_graph_buffer[read_idx], 0, 4096, graph_y + graph_height, graph_y);
                    // Draw 3x3 square
                    display.fillRect(i - 1, cross_y - 1, 3, 3, SSD1306_WHITE);
                }
            }

            display.display();
            
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
        last_crossing_time = 0;
        total_crossing_delta = 0;
        crossing_delta_count = 0;
    }
}