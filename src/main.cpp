#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <soc/adc_channel.h>
#include "signal.h"

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buffer for graph
uint16_t signal_graph_buffer[SCREEN_WIDTH];
int signal_graph_index = 0;

// Pin Configuration
const int ledcPin = 26;

// LEDC Configuration
const int ledcResolution = 8;
const int ledcFreq = 100000; // 100 kHz

// Sine wave configuration
const uint32_t sampling_frequency = 20000; // 20 kHz
const float sine_frequency = 440.0; // 440 Hz

// Globals for ISR
hw_timer_t *timer = NULL;
volatile uint32_t phase_accumulator = 0;
const uint32_t phase_increment = (uint32_t)((sine_frequency / (float)sampling_frequency) * 4294967296.0);
uint8_t sin_lut[256];

// Signal object
Signal* signal_monitor = nullptr;

void IRAM_ATTR onTimer() {
    phase_accumulator += phase_increment;
    uint8_t index = phase_accumulator >> 24;
    ledcWrite(ledcPin, sin_lut[index]);
}

void setup() {
    delay(1000);
    Serial.begin(115200);

    // Initialize OLED display
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    display.setRotation(2);
    memset(signal_graph_buffer, 0, sizeof(signal_graph_buffer));

    // Generate sine lookup table
    for (int i = 0; i < 256; i++) {
        float angle = 2.0 * PI * (float)i / 256.0;
        sin_lut[i] = (uint8_t)((sin(angle) + 1.0) * 0.5 * 255.0);
    }

    // Configure LEDC (новый API)
    ledcAttach(ledcPin, ledcFreq, ledcResolution);

    // Configure Timer (новый API)
    timer = timerBegin(1000000);
    timerAttachInterrupt(timer, &onTimer);
    timerAlarm(timer, 1000000 / sampling_frequency, true, 0);

    // Создаем Signal объект для мониторинга пина 36 (SENSOR_VP)
    adc_channel_t channels[] = {static_cast<adc_channel_t>(ADC1_GPIO36_CHANNEL)};
    signal_monitor = new Signal(channels, 1);
    
    if (signal_monitor) {
        Serial.println("Starting signal monitoring...");
        signal_monitor->start();
    }
}

void loop() {
    static unsigned long last_display_time = 0;
    
    // Обновляем дисплей каждые 100 мс
    if (millis() - last_display_time >= 100) {
        if (signal_monitor && signal_monitor->is_running()) {
            // Получаем статистику для первого канала (индекс 0)
            SignalStats stats = signal_monitor->get_stats(0);
            
            // Получаем буфер данных
            uint16_t buffer[SCREEN_WIDTH];
            if (signal_monitor->get_buffer(0, SCREEN_WIDTH, buffer)) {
                // Копируем в график
                memcpy(signal_graph_buffer, buffer, sizeof(signal_graph_buffer));
            }

            // Отображаем на OLED
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.printf("Min:%u Max:%u", stats.min_value, stats.max_value);
            display.setCursor(0, 10);
            display.printf("Avg:%.1f Freq:%.1f Hz", stats.avg_value, stats.frequency);

            // Рисуем график
            int graph_y = 20;
            int graph_height = SCREEN_HEIGHT - graph_y;

            for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
                int y1 = map(signal_graph_buffer[i], 0, 4096, graph_y + graph_height, graph_y);
                int y2 = map(signal_graph_buffer[i + 1], 0, 4096, graph_y + graph_height, graph_y);
                display.drawLine(i, y1, i + 1, y2, SSD1306_WHITE);
            }

            // Рисуем линию среднего значения
            if (stats.avg_value > 0) {
                int avg_y = map((int)stats.avg_value, 0, 4096, graph_y + graph_height, graph_y);
                for (int i = 0; i < SCREEN_WIDTH; i += 4) {
                    display.drawPixel(i, avg_y, SSD1306_WHITE);
                }
            }

            signal_monitor->reset_stats();

            display.display();
        }
        
        last_display_time = millis();
    }
    
    // Небольшая задержка для предотвращения перегрузки
    delay(10);
}