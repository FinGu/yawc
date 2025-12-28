# Contributing to yawc

Thank you for your interest in contributing to yawc! This document provides guidelines for contributing to the project.

## Getting Started

1. **Fork the repository** and clone your fork
2. **Set up the development environment** by installing the required dependencies (see README.md)
3. **Build the project** using Meson to ensure everything works

```bash
meson setup build
meson compile -C build
```

## How to Contribute

### Reporting Bugs

- Check if the issue already exists in the issue tracker
- Provide a clear description of the problem
- Include steps to reproduce the issue
- Mention your system information (OS, wlroots version, etc.)

### Suggesting Features

- Open an issue describing the feature
- Explain the use case and benefits
- Be open to discussion about implementation details

### Code Contributions

1. **Create a new branch** for your feature or bugfix
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Follow the existing code style**
   - Use the existing `.clang-format` configuration
   - Keep code clean and well-commented
   - Prefer smart pointers over raw pointers where possible

3. **Test your changes**
   - Ensure the compositor builds without warnings
   - Test your changes thoroughly
   - If modifying the WM API, test with the default window manager

4. **Commit your changes**
   - Write clear, descriptive commit messages
   - Keep commits focused on a single change

5. **Submit a pull request**
   - Describe what your PR does
   - Reference any related issues
   - Be responsive to feedback

## Code Style

- Use C++23 features where appropriate
- Follow the existing naming conventions
- Keep functions focused and modular
- Document complex logic with comments

## Window Manager Plugin Development

If you're developing a window manager plugin:
- Study the `wm_api.h` header for available functions
- Look at the `default-wm` implementation as a reference
- Ensure your plugin properly registers and unregisters callbacks

## Questions?

If you have questions about contributing, feel free to open an issue for discussion.

## License

By contributing to yawc, you agree that your contributions will be licensed under the MIT License.
