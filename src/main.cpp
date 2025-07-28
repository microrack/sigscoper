#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/ledc.h>
#include <driver/gptimer.h>
#include <soc/adc_channel.h>
#include "signal.h"

// Конфигурация OLED дисплея
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Конфигурация LEDC
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (2) // GPIO2
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_DUTY               (4095) // 50%
#define LEDC_FREQUENCY          (1000) // 1kHz

// Конфигурация таймера
#define TIMER_RESOLUTION_HZ   (1000000) // 1MHz
#define TIMER_INTERVAL_MS     (1000)    // 1ms

Signal* signal_monitor = nullptr;
gptimer_handle_t gptimer = NULL;

bool IRAM_ATTR onTimer(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    // Переключаем состояние LED
    static bool led_state = false;
    led_state = !led_state;
    ledcWrite(LEDC_CHANNEL, led_state ? LEDC_DUTY : 0);
    return false; // Не перезапускать таймер автоматически
}

void setup() {
    Serial.begin(115200);
    
    // Инициализация OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    // rotate display
    display.setRotation(2);
    
    // Настройка LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    
    ledc_channel_config_t ledc_channel = {
        .gpio_num   = LEDC_OUTPUT_IO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ledc_channel);
    
    // Настройка таймера
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_APB,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RESOLUTION_HZ,
    };
    gptimer_new_timer(&timer_config, &gptimer);
    
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = TIMER_RESOLUTION_HZ / 1000 * TIMER_INTERVAL_MS,
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = true,
        },
    };
    gptimer_set_alarm_action(gptimer, &alarm_config);
    
    gptimer_event_callbacks_t cbs = {
        .on_alarm = onTimer,
    };
    gptimer_register_event_callbacks(gptimer, &cbs, NULL);
    
    gptimer_start(gptimer);
    
    // Создаем конфигурацию сигнала
    SignalConfig signal_config;
    signal_config.channel_count = 1;
    signal_config.channels[0] = static_cast<adc_channel_t>(ADC1_GPIO36_CHANNEL);
    signal_config.trigger_mode = TriggerMode::FREE;
    signal_config.trigger_level = 2048;
    
    // Создаем монитор сигнала
    signal_monitor = new Signal();
    if (signal_monitor) {
        Serial.println("Starting signal monitoring with AUTO_RISE trigger...");
        if (signal_monitor->start(signal_config)) {
            Serial.println("Signal monitoring started successfully");
        } else {
            Serial.println("Failed to start signal monitoring");
        }
    }
}

SignalStats stats;
uint16_t buffer[64];

void loop() {
    static uint32_t last_update = 0;

    signal_monitor->get_stats(0, &stats);
    if(signal_monitor->get_buffer(0, 64, buffer)) {
        last_update++;
        signal_monitor->restart();
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("%.1f %.1f %.1f %.1f Hz", 
        std::min(std::max(-9.0, stats.min_value / 1000.0), 9.0),
        std::min(std::max(-9.0, stats.avg_value / 1000.0), 9.0),
        std::min(std::max(-9.0, stats.max_value / 1000.0), 9.0),
        stats.frequency
    );
    display.setCursor(0, 10);
    display.printf("T: %s %u %u",
        signal_monitor->is_trigger_fired() ? "FIRED" : "WAIT",
        last_update,
        signal_monitor->get_auto_trigger_level()
    );

    int graph_y = 40;
    for (int i = 0; i < 64; i++) {
        int y = graph_y + (buffer[i] * 20) / 4095;
        if (y >= 0 && y < SCREEN_HEIGHT) {
            display.drawPixel(i * 2, y, SSD1306_WHITE);
        }
    }
    
    display.display();
    
    delay(10);
}