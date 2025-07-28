# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-XX

### Added
- Initial release of Sigscoper library
- Real-time ADC sampling with configurable sampling rates
- Advanced triggering with multiple modes (FREE, AUTO_RISE, AUTO_FALL, FIXED_RISE, FIXED_FALL)
- Hysteresis support for reliable triggering
- Decimation for lower effective sampling rates
- Median filtering for noise reduction
- Frequency calculation using zero-crossing detection
- Multi-channel support (up to 8 channels)
- FreeRTOS integration with proper task management
- Comprehensive API for signal acquisition and analysis
- Support for ESP32 ADC channels
- Signal statistics calculation (min, max, average, frequency)
- Buffer management for signal data

### Features
- **SigscoperConfig**: Configuration structure for signal acquisition
- **TriggerMode**: Multiple trigger modes for different signal conditions
- **SigscoperStats**: Statistics structure for signal analysis
- **Sigscoper class**: Main class with methods for:
  - Starting/stopping signal acquisition
  - Getting signal buffers and statistics
  - Trigger management
  - Status checking

### Supported Platforms
- ESP32 (espressif32)
- Arduino Framework
- ESP-IDF Framework 