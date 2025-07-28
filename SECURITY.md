# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security vulnerability within Sigscoper, please send an email to security@microrack.org. All security vulnerabilities will be promptly addressed.

Please include the following information in your report:

- Description of the vulnerability
- Steps to reproduce the issue
- Potential impact
- Suggested fix (if any)

## Security Considerations

This library is designed for ESP32 signal acquisition and analysis. Please note:

1. **ADC Safety**: The library uses ESP32's ADC for signal acquisition. Ensure input signals are within the safe voltage range (0-3.3V).

2. **Memory Safety**: The library uses FreeRTOS tasks and semaphores. Proper memory management is implemented.

3. **Real-time Constraints**: The library operates in real-time. Avoid blocking operations in callback functions.

4. **Hardware Protection**: Always use appropriate voltage dividers and protection circuits for analog inputs.

## Best Practices

- Always validate input signals before processing
- Use appropriate filtering for noise reduction
- Monitor system resources during long-running acquisitions
- Test thoroughly on your specific hardware configuration 