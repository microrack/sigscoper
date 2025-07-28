# Contributing to Sigscoper

Thank you for your interest in contributing to Sigscoper! This document provides guidelines for contributing to this project.

## Getting Started

1. Fork the repository
2. Clone your fork locally
3. Create a new branch for your feature/fix
4. Make your changes
5. Test your changes
6. Submit a pull request

## Development Setup

### Prerequisites

- PlatformIO Core
- ESP32 development board
- Arduino IDE (optional)

### Building the Library

```bash
# Clone the repository
git clone https://github.com/microrack/sigscoper.git
cd sigscoper

# Install dependencies and build
pio run
```

### Running Tests

```bash
# Run tests
pio test
```

## Code Style Guidelines

- Follow the existing code style
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions focused and small
- Use proper error handling

## Pull Request Guidelines

1. **Title**: Use a clear, descriptive title
2. **Description**: Explain what the PR does and why
3. **Testing**: Describe how you tested your changes
4. **Breaking Changes**: Note any breaking changes

## Issue Reporting

When reporting issues, please include:

- PlatformIO version
- ESP32 board type
- Framework version (Arduino/ESP-IDF)
- Minimal code example to reproduce the issue
- Expected vs actual behavior

## License

By contributing to this project, you agree that your contributions will be licensed under the same license as the project (CC-BY-SA-4.0).

## Questions?

If you have questions about contributing, please open an issue or contact the maintainers. 