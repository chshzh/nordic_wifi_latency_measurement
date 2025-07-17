# Contributing to Wi-Fi UDP Latency Test Application

Thank you for your interest in contributing to this project! We welcome contributions from the community.

## 🚀 Getting Started

### Prerequisites

- Nordic Connect SDK v3.0.2 or later
- nRF7002 DK development kit
- Git and GitHub account
- Basic knowledge of Zephyr RTOS and C programming

### Development Setup

1. **Fork the repository** on GitHub
2. **Clone your fork**:
   ```bash
   git clone https://github.com/yourusername/wifi_udp_latency.git
   cd wifi_udp_latency
   ```
3. **Set up NCS environment** (adjust path to your installation):
   ```bash
   export ZEPHYR_BASE=/opt/nordic/ncs/v3.0.2/zephyr
   export NRF_BASE=/opt/nordic/ncs/v3.0.2/nrf
   source $ZEPHYR_BASE/zephyr-env.sh
   ```
4. **Test the build**:
   ```bash
   west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-tx.conf
   ```

## 📝 How to Contribute

### Reporting Issues

1. **Search existing issues** to avoid duplicates
2. **Use issue templates** when available
3. **Provide detailed information**:
   - NCS version
   - Hardware used
   - Steps to reproduce
   - Expected vs actual behavior
   - Log output (if applicable)

### Submitting Changes

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**:
   - Write clean, documented code
   - Follow coding standards (see below)
   - Add tests if applicable
   - Update documentation

3. **Test your changes**:
   ```bash
   # Build all configurations
   west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-tx.conf
   west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-rx.conf
   west build -p auto -b nrf7002dk/nrf5340/cpuapp -- -DEXTRA_CONF_FILE=overlay-rx-softap.conf
   
   # Test on hardware if possible
   west flash --erase
   ```

4. **Commit your changes**:
   ```bash
   git add .
   git commit -m "feat: add your feature description"
   ```
   
   Use [conventional commits](https://www.conventionalcommits.org/):
   - `feat:` for new features
   - `fix:` for bug fixes
   - `docs:` for documentation changes
   - `style:` for formatting changes
   - `refactor:` for code refactoring
   - `test:` for adding tests
   - `chore:` for maintenance tasks

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

6. **Create a Pull Request**:
   - Use descriptive title and description
   - Reference related issues
   - Add screenshots/logs if relevant

## 📚 Coding Standards

### C Code Style

Follow [Zephyr Coding Guidelines](https://docs.zephyrproject.org/latest/guides/coding_guidelines/index.html):

- **Indentation**: Use tabs (width 8)
- **Line length**: Maximum 100 characters
- **Naming**: Use snake_case for functions and variables
- **Comments**: Use `/* */` for multi-line, `//` for single-line

```c
/* Good example */
static int wifi_connect_to_network(const char *ssid, const char *password)
{
    int ret;
    
    if (!ssid || !password) {
        LOG_ERR("Invalid parameters");
        return -EINVAL;
    }
    
    /* Attempt to connect */
    ret = wifi_mgmt_connect();
    if (ret < 0) {
        LOG_ERR("Connection failed: %d", ret);
        return ret;
    }
    
    return 0;
}
```

### Configuration Files

- **Kconfig**: Use consistent naming with project prefix
- **Device Tree**: Follow Zephyr DT conventions
- **CMake**: Keep simple and readable

### Documentation

- **Code comments**: Explain why, not what
- **README updates**: Keep installation and usage instructions current
- **API documentation**: Document public functions and structures

## 🧪 Testing Guidelines

### Manual Testing

1. **Build verification**: All overlay configurations must build successfully
2. **Hardware testing**: Test on actual nRF7002 DK when possible
3. **Functionality testing**: Verify both test scenarios work
4. **Measurement validation**: Check LED timing with oscilloscope/PPK2

### Test Scenarios

Ensure your changes work with:

- **Test 1**: External AP mode (both TX and RX devices)
- **Test 2**: SoftAP mode (RX as AP, TX as client)
- **Different Wi-Fi networks**: Various security types and frequencies
- **Parameter variations**: Different intervals, durations, ports

## 🤝 Code Review Process

1. **Automated checks**: CI will run build tests
2. **Manual review**: Maintainers will review code quality and functionality
3. **Testing**: May request additional testing on specific hardware
4. **Documentation**: Ensure all changes are properly documented

### Review Criteria

- ✅ Code compiles without warnings
- ✅ Follows coding standards
- ✅ Includes appropriate error handling
- ✅ Documentation is updated
- ✅ Backward compatibility maintained
- ✅ No unnecessary complexity

## 🐛 Debugging Help

### Common Issues

**Build failures**:
```bash
# Clean build environment
rm -rf build/
west build -p auto -b nrf7002dk/nrf5340/cpuapp
```

**Runtime issues**:
- Check serial logs at 115200 baud
- Verify Wi-Fi credentials
- Check GPIO connections for measurement equipment

**Memory issues**:
```bash
# Check memory usage
west build -t rom_report
west build -t ram_report
```

## 📞 Getting Help

- **GitHub Discussions**: For questions and ideas
- **Issues**: For bug reports and feature requests
- **Nordic DevZone**: For hardware-specific questions
- **Zephyr Discord**: For Zephyr-related questions

## 🎯 Priority Areas

We're particularly interested in contributions for:

1. **Additional board support** (nRF70 series variants)
2. **Measurement accuracy improvements**
3. **Performance optimizations**
4. **Documentation enhancements**
5. **Test automation**
6. **Power consumption optimization**

## 📄 License

By contributing, you agree that your contributions will be licensed under the same license as the project (LicenseRef-Nordic-5-Clause).

---

Thank you for contributing! 🙏 