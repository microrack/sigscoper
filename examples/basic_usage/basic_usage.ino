#include "sigscoper.h"

Sigscoper sigscoper;
SigscoperConfig config;
SigscoperStats stats;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Sigscoper Basic Usage Example");
    
    // Configure signal acquisition
    config.channel_count = 1;
    config.channels[0] = ADC_CHANNEL_0;  // GPIO36
    config.trigger_mode = TriggerMode::AUTO_FALL;
    config.trigger_level = 1000;
    config.sampling_rate = 20000;
    
    // Start signal monitoring
    if (!sigscoper.start(config)) {
        Serial.println("Failed to start signal monitoring");
        return;
    }
    
    Serial.println("Signal monitoring started successfully");
    Serial.println("Waiting for trigger...");
}

void loop() {
    if (sigscoper.is_ready()) {
        // Get signal statistics
        if (sigscoper.get_stats(0, &stats)) {
            Serial.printf("Signal captured!\n");
            Serial.printf("Min: %d, Max: %d, Avg: %.1f\n", 
                stats.min_value, stats.max_value, stats.avg_value);
            Serial.printf("Frequency: %.1f Hz\n", stats.frequency);
        }
        
        // Get signal buffer
        uint16_t buffer[128];
        if (sigscoper.get_buffer(0, 128, buffer)) {
            Serial.println("Signal buffer:");
            for (int i = 0; i < 128; i += 8) {
                Serial.printf("%4d: %4d %4d %4d %4d %4d %4d %4d %4d\n",
                    i, buffer[i], buffer[i+1], buffer[i+2], buffer[i+3],
                    buffer[i+4], buffer[i+5], buffer[i+6], buffer[i+7]);
            }
        }
        
        Serial.println("Restarting for next acquisition...");
        sigscoper.restart();
    }
    
    delay(100);
} 