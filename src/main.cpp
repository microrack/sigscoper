#include <Arduino.h>
#include <soc/adc_channel.h>
#include "sigscoper.h"

Sigscoper signal_monitor;

void setup() {
    Serial.begin(115200);
    Serial.println("Sigscoper Test");
    
    // Create signal configuration
    SigscoperConfig signal_config;
    signal_config.channel_count = 1;
    signal_config.channels[0] = static_cast<adc_channel_t>(ADC1_GPIO36_CHANNEL);
    signal_config.trigger_mode = TriggerMode::FREE;
    signal_config.trigger_level = 0;
    signal_config.sampling_rate = 100;
    
    // Create signal monitor
    if (!signal_monitor.start(signal_config)) {
        Serial.println("Failed to start signal monitoring");
        for(;;);
    }
    
    Serial.println("Signal monitoring started");
}

void loop() {
    // Empty loop
}