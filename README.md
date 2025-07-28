# Sigscoper - ESP32 Signal Oscilloscope Library

A powerful signal oscilloscope library for ESP32 that provides real-time signal acquisition, triggering, and analysis capabilities.

**Created by:** MICRORACK inc.  
**Repository:** https://github.com/microrack/sigscoper  
**Website:** https://microrack.org/  
**License:** CC-BY-SA-4.0

## Features

- **Real-time ADC sampling** with configurable sampling rates
- **Advanced triggering** with multiple modes (FREE, AUTO_RISE, AUTO_FALL, FIXED_RISE, FIXED_FALL)
- **Hysteresis support** for reliable triggering
- **Decimation** for lower effective sampling rates
- **Median filtering** for noise reduction
- **Frequency calculation** using zero-crossing detection
- **Multi-channel support** (up to 8 channels)
- **FreeRTOS integration** with proper task management

## Installation

### PlatformIO

Add this library to your `platformio.ini`:

```ini
lib_deps = 
    https://github.com/microrack/sigscoper.git
```

### Arduino IDE

1. Download this repository
2. Extract to your Arduino libraries folder
3. Restart Arduino IDE

## Usage

### Basic Example

```cpp
#include "sigscoper.h"

Sigscoper sigscoper;
SigscoperConfig config;
SigscoperStats stats;

void setup() {
    Serial.begin(115200);
    
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
    
    Serial.println("Signal monitoring started");
}

void loop() {
    if (sigscoper.is_ready()) {
        // Get signal statistics
        if (sigscoper.get_stats(0, &stats)) {
            Serial.printf("Min: %d, Max: %d, Avg: %.1f, Freq: %.1f Hz\n",
                stats.min_value, stats.max_value, stats.avg_value, stats.frequency);
        }
        
        // Get signal buffer
        uint16_t buffer[128];
        if (sigscoper.get_buffer(0, 128, buffer)) {
            // Process signal data
            for (int i = 0; i < 128; i++) {
                // Use buffer[i] for your processing
            }
        }
        
        // Restart for next acquisition
        sigscoper.restart();
    }
    
    delay(100);
}
```

## API Reference

### SigscoperConfig

Configuration structure for signal acquisition:

```cpp
struct SigscoperConfig {
    size_t channel_count;           // Number of channels (1-8)
    adc_channel_t channels[8];      // ADC channels
    TriggerMode trigger_mode;       // Trigger mode
    uint16_t trigger_level;         // Trigger level
    uint32_t sampling_rate;         // Sampling rate in Hz
};
```

### TriggerMode

Available trigger modes:

- `TriggerMode::FREE` - No triggering, continuous acquisition
- `TriggerMode::AUTO_RISE` - Auto trigger on rising edge
- `TriggerMode::AUTO_FALL` - Auto trigger on falling edge
- `TriggerMode::FIXED_RISE` - Fixed level trigger on rising edge
- `TriggerMode::FIXED_FALL` - Fixed level trigger on falling edge

### SigscoperStats

Statistics structure:

```cpp
struct SigscoperStats {
    uint16_t min_value;    // Minimum signal value
    uint16_t max_value;    // Maximum signal value
    float avg_value;       // Average signal value
    float frequency;       // Calculated frequency in Hz
};
```

### Sigscoper Class Methods

#### Configuration
- `bool start(const SigscoperConfig& config)` - Start signal acquisition
- `void stop()` - Stop signal acquisition
- `void restart()` - Restart with current configuration

#### Status
- `bool is_running()` - Check if acquisition is running
- `bool is_ready()` - Check if buffer is ready
- `bool is_trigger_fired()` - Check if trigger has fired

#### Data Access
- `bool get_stats(size_t index, SigscoperStats* stats)` - Get signal statistics
- `bool get_buffer(size_t index, size_t size, uint16_t* buffer)` - Get signal buffer
- `uint16_t get_trigger_threshold()` - Get current trigger threshold

## Hardware Requirements

- ESP32 development board
- ADC input signals (0-3.3V range)

## License

This work is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

**Title of work:** sigscoper  
**Creator of work:** MICRORACK inc.  
**Link to work:** https://github.com/microrack/sigscoper  
**Link to Creator Profile:** https://microrack.org/  
**Year of creation:** 2025 