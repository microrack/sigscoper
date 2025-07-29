#include <Arduino.h>
#include <soc/adc_channel.h>
#include "sigscoper.h"

Sigscoper signal_monitor;

void setup() {
    Serial.begin(115200);
    Serial.println("Sigscoper Test");

    if(!signal_monitor.begin()) {
        Serial.println("Failed to initialize signal monitor");
        for(;;);
    }
    
    // Create signal configuration
    SigscoperConfig signal_config;
    signal_config.channel_count = 1;
    signal_config.channels[0] = static_cast<adc_channel_t>(ADC1_GPIO36_CHANNEL);
    signal_config.trigger_mode = TriggerMode::FREE;
    signal_config.trigger_level = 0;
    signal_config.sampling_rate = 10000;
    
    // Create signal monitor
    if (!signal_monitor.start(signal_config)) {
        Serial.println("Failed to start signal monitoring");
        for(;;);
    }

    delay(10);

    while(!signal_monitor.is_ready()) {
        Serial.println("Waiting for signal monitor to be ready");
        delay(10);
    }
    
    SigscoperStats stats;
    signal_monitor.get_stats(0, &stats);
    Serial.printf("Min: %d, Max: %d, Avg: %f, Frequency: %f\n",
        stats.min_value,
        stats.max_value,
        stats.avg_value,
        stats.frequency
    );

    signal_monitor.stop();

    signal_config.sampling_rate = 25000;
    signal_monitor.start(signal_config);

    delay(10);

    while(!signal_monitor.is_ready()) {
        Serial.println("Waiting for signal monitor to be ready");
        delay(10);
    }

    signal_monitor.get_stats(0, &stats);
    Serial.printf("Min: %d, Max: %d, Avg: %f, Frequency: %f\n",
        stats.min_value,
        stats.max_value,
        stats.avg_value,
        stats.frequency
    );
}

void loop() {
    // Empty loop
}