# Installation Guide

This guide will help you install and set up Sigscoper library for your ESP32 projects.

## Prerequisites

Before installing Sigscoper, make sure you have:

- **PlatformIO Core** (recommended) or **Arduino IDE**
- **ESP32 development board**
- **USB cable** for programming
- **Basic knowledge** of ESP32 development

## Installation Methods

### Method 1: PlatformIO (Recommended)

#### Using Library Registry

1. Open your `platformio.ini` file
2. Add the library dependency:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/microrack/sigscoper.git
```

#### Using Git Repository

```bash
# Clone the repository
git clone https://github.com/microrack/sigscoper.git

# Copy to your project's lib directory
cp -r sigscoper ~/.platformio/lib/
```

#### Using PlatformIO Library Manager

```bash
# Install via PlatformIO CLI
pio lib install "https://github.com/microrack/sigscoper.git"
```

### Method 2: Arduino IDE

#### Manual Installation

1. Download the library from GitHub:
   - Go to https://github.com/microrack/sigscoper
   - Click "Code" → "Download ZIP"
   - Extract the ZIP file

2. Install in Arduino IDE:
   - Open Arduino IDE
   - Go to **Sketch** → **Include Library** → **Add .ZIP Library**
   - Select the extracted folder
   - Click "Open"

#### Using Git

```bash
# Navigate to Arduino libraries directory
cd ~/Arduino/libraries

# Clone the repository
git clone https://github.com/microrack/sigscoper.git
```

## Configuration

### PlatformIO Configuration

Add the following to your `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
build_flags = 
    -std=gnu++14
    -D_GLIBCXX_USE_C99
    -DARDUINO_USB_CDC_ON_BOOT=0
    -fno-strict-aliasing
    -Wno-error=narrowing
build_unflags = 
    -std=gnu++11
    -std=c++17
lib_deps =
    https://github.com/microrack/sigscoper.git
```

### Arduino IDE Configuration

1. Select your ESP32 board:
   - Go to **Tools** → **Board** → **ESP32 Arduino** → **ESP32 Dev Module**

2. Configure board settings:
   - **Upload Speed**: 921600
   - **CPU Frequency**: 240MHz
   - **Flash Frequency**: 80MHz
   - **Flash Mode**: QIO
   - **Flash Size**: 4MB
   - **Partition Scheme**: Default 4MB with spiffs

## Verification

### Test Installation

Create a simple test sketch to verify the installation:

```cpp
#include "sigscoper.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Sigscoper library test");
    
    Sigscoper sigscoper;
    Serial.printf("Max channels: %d\n", sigscoper.get_max_channels());
    
    Serial.println("Library installed successfully!");
}

void loop() {
    delay(1000);
}
```

### Expected Output

```
Sigscoper library test
Max channels: 8
Library installed successfully!
```

## Troubleshooting

### Common Issues

#### 1. Compilation Errors

**Problem**: Library not found
```
fatal error: sigscoper.h: No such file or directory
```

**Solution**: 
- Check that the library is properly installed
- Verify the include path in your IDE
- Try reinstalling the library

#### 2. PlatformIO Build Errors

**Problem**: Build fails with C++ errors
```
error: 'freertos/FreeRTOS.h' file not found
```

**Solution**:
- Make sure you're using the ESP32 platform
- Update PlatformIO: `pio update`
- Clean and rebuild: `pio run -t clean && pio run`

#### 3. Arduino IDE Issues

**Problem**: Library doesn't appear in library manager

**Solution**:
- Restart Arduino IDE
- Check that the library folder is in the correct location
- Verify the library structure matches Arduino requirements

#### 4. Runtime Errors

**Problem**: ADC initialization fails

**Solution**:
- Check your ESP32 board configuration
- Verify ADC pin connections
- Ensure proper voltage levels (0-3.3V)

### Getting Help

If you encounter issues:

1. **Check the documentation**: [README.md](../README.md)
2. **Search existing issues**: [GitHub Issues](https://github.com/microrack/sigscoper/issues)
3. **Create a new issue**: Include your environment details and error messages

## Next Steps

After successful installation:

1. **Read the documentation**: [API Reference](API.md)
2. **Try the examples**: [Examples](../examples/)
3. **Start your project**: Use the basic usage example as a starting point

## Support

For additional help:

- **Email**: support@microrack.org
- **GitHub**: https://github.com/microrack/sigscoper
- **Documentation**: [docs/](docs/) 