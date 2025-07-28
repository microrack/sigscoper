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

Signal signal_monitor;
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
    signal_config.trigger_mode = TriggerMode::AUTO_RISE;
    signal_config.trigger_level = 0;
    signal_config.sampling_rate = 1000;
    
    // Создаем монитор сигнала
    if (!signal_monitor.start(signal_config)) {
        Serial.println("Failed to start signal monitoring");
        for(;;);
    }
}

SignalStats stats;
uint16_t buffer[SCREEN_WIDTH];

void loop() {
    static uint32_t last_trigger_wait = millis();

    if(!signal_monitor.is_ready() && last_trigger_wait == 0) {
        last_trigger_wait = millis();
    }

    if(millis() - last_trigger_wait > 1000 || signal_monitor.is_ready()) {
        signal_monitor.get_stats(0, &stats);
        signal_monitor.get_buffer(0, SCREEN_WIDTH, buffer);
        signal_monitor.restart();
        last_trigger_wait = 0;
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("%.1f %.1f %.1f %.1f Hz", 
        std::min(std::max(-9.0, stats.min_value / 1000.0), 9.0),
        std::min(std::max(-9.0, stats.avg_value / 1000.0), 9.0),
        std::min(std::max(-9.0, stats.max_value / 1000.0), 9.0),
        stats.frequency
    );
    /*
    display.setCursor(0, 10);
    display.printf("T: %s %u",
        signal_monitor.is_trigger_fired() ? "FIRED" : "WAIT",
        signal_monitor.get_trigger_threshold()
    );
    */

    int graph_y = 40;
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
        // map range 600 to 2400 to 64 to 10
        int y1 = map(buffer[i], 400, 2400, 64, 10);
        int y2 = map(buffer[i + 1], 400, 2400, 64, 10);
        
        if (y1 >= 0 && y1 < SCREEN_HEIGHT && y2 >= 0 && y2 < SCREEN_HEIGHT) {
            // Рисуем линию с шириной 2 пикселя
            display.drawLine(i, y1, i + 1, y2, SSD1306_WHITE);
            display.drawLine(i, y1 + 1, i + 1, y2 + 1, SSD1306_WHITE);
        }
    }

    // draw trigger level using dotted line
    int trigger_level =
        map(signal_monitor.get_trigger_threshold(), 400, 2400, 64, 10);
    for(int i = 0; i < SCREEN_WIDTH; i += 2) {
        display.drawPixel(i, trigger_level, SSD1306_WHITE);
    }

    // draw tirgger position using dotted line
    for(int i = 10; i < SCREEN_HEIGHT; i += 2) {
        display.drawPixel(SCREEN_WIDTH / 2, i, SSD1306_WHITE);
    }
    
    
    display.display();
    
    delay(10);
}