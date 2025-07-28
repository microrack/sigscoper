# Sigscoper API Documentation

## Overview

Sigscoper is a powerful signal oscilloscope library for ESP32 that provides real-time signal acquisition, triggering, and analysis capabilities.

## Table of Contents

- [Data Structures](#data-structures)
- [Enumerations](#enumerations)
- [Class Reference](#class-reference)
- [Examples](#examples)

## Data Structures

### SigscoperConfig

Configuration structure for signal acquisition.

```cpp
struct SigscoperConfig {
    size_t channel_count;           // Number of channels (1-8)
    adc_channel_t channels[8];      // ADC channels
    TriggerMode trigger_mode;       // Trigger mode
    uint16_t trigger_level;         // Trigger level
    uint32_t sampling_rate;         // Sampling rate in Hz
};
```

**Members:**
- `channel_count`: Number of channels to acquire (1-8)
- `channels`: Array of ADC channels to use
- `trigger_mode`: Trigger mode (see TriggerMode enum)
- `trigger_level`: Trigger level in ADC units (0-4095)
- `sampling_rate`: Sampling rate in Hz

### SigscoperStats

Statistics structure for signal analysis.

```cpp
struct SigscoperStats {
    uint16_t min_value;    // Minimum signal value
    uint16_t max_value;    // Maximum signal value
    float avg_value;       // Average signal value
    float frequency;       // Calculated frequency in Hz
};
```

**Members:**
- `min_value`: Minimum signal value in ADC units
- `max_value`: Maximum signal value in ADC units
- `avg_value`: Average signal value
- `frequency`: Calculated frequency in Hz

## Enumerations

### TriggerMode

Available trigger modes for signal acquisition.

```cpp
enum class TriggerMode {
    FREE,           // No triggering, continuous acquisition
    AUTO_RISE,      // Auto trigger on rising edge
    AUTO_FALL,      // Auto trigger on falling edge
    FIXED_RISE,     // Fixed level trigger on rising edge
    FIXED_FALL      // Fixed level trigger on falling edge
};
```

## Class Reference

### Sigscoper

Main class for signal acquisition and analysis.

#### Constructor

```cpp
Sigscoper();
Sigscoper(size_t buffer_size);
```

#### Methods

##### Configuration

```cpp
bool start(const SigscoperConfig& config);
```
Starts signal acquisition with the specified configuration.

**Parameters:**
- `config`: Configuration structure

**Returns:**
- `true` if successful, `false` otherwise

```cpp
void stop();
```
Stops signal acquisition.

```cpp
void restart();
```
Restarts signal acquisition with the current configuration.

##### Status

```cpp
bool is_running() const;
```
Checks if signal acquisition is running.

**Returns:**
- `true` if running, `false` otherwise

```cpp
bool is_ready() const;
```
Checks if the signal buffer is ready for reading.

**Returns:**
- `true` if ready, `false` otherwise

```cpp
bool is_trigger_fired() const;
```
Checks if the trigger has fired.

**Returns:**
- `true` if trigger fired, `false` otherwise

##### Data Access

```cpp
bool get_stats(size_t index, SigscoperStats* stats) const;
```
Gets signal statistics for the specified channel.

**Parameters:**
- `index`: Channel index (0-based)
- `stats`: Pointer to statistics structure

**Returns:**
- `true` if successful, `false` otherwise

```cpp
bool get_buffer(size_t index, size_t size, uint16_t* buffer) const;
```
Gets signal buffer data for the specified channel.

**Parameters:**
- `index`: Channel index (0-based)
- `size`: Number of samples to retrieve
- `buffer`: Pointer to buffer array

**Returns:**
- `true` if successful, `false` otherwise

##### Utility

```cpp
uint16_t get_trigger_threshold() const;
```
Gets the current trigger threshold.

**Returns:**
- Trigger threshold in ADC units

```cpp
size_t get_max_channels() const;
```
Gets the maximum number of supported channels.

**Returns:**
- Maximum number of channels

## Examples

### Basic Usage

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

### Multi-Channel Example

```cpp
#include "sigscoper.h"

Sigscoper sigscoper;
SigscoperConfig config;

void setup() {
    Serial.begin(115200);
    
    // Configure multi-channel acquisition
    config.channel_count = 2;
    config.channels[0] = ADC_CHANNEL_0;  // GPIO36
    config.channels[1] = ADC_CHANNEL_1;  // GPIO37
    config.trigger_mode = TriggerMode::FREE;
    config.sampling_rate = 10000;
    
    if (!sigscoper.start(config)) {
        Serial.println("Failed to start multi-channel monitoring");
        return;
    }
}

void loop() {
    if (sigscoper.is_ready()) {
        // Process each channel
        for (size_t ch = 0; ch < config.channel_count; ch++) {
            SigscoperStats stats;
            if (sigscoper.get_stats(ch, &stats)) {
                Serial.printf("Channel %d: Min=%d, Max=%d, Avg=%.1f, Freq=%.1f Hz\n",
                    ch, stats.min_value, stats.max_value, stats.avg_value, stats.frequency);
            }
        }
        
        sigscoper.restart();
    }
    
    delay(100);
}
```

## Error Handling

The library provides error checking through return values. Always check return values from methods that can fail:

```cpp
if (!sigscoper.start(config)) {
    Serial.println("Failed to start signal acquisition");
    return;
}

if (!sigscoper.get_stats(0, &stats)) {
    Serial.println("Failed to get statistics");
    return;
}
```

## Performance Considerations

- **Sampling Rate**: Higher sampling rates require more CPU resources
- **Buffer Size**: Larger buffers use more memory but provide more data
- **Channel Count**: More channels increase processing overhead
- **Trigger Modes**: Complex trigger modes may add latency

## Hardware Requirements

- ESP32 development board
- ADC input signals (0-3.3V range)
- Proper voltage dividers for signals > 3.3V 