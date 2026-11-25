# ESP32 BMS Control

This project targets the Olimex ESP32-POE-ISO and provides separate firmware
applications for Modbus TCP readings and converter PWM generation.

## Build

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html)
if the `pio` command is unavailable, and make sure its shell command is on your
`PATH`.

Then run from this project directory. The default target is the Modbus reader:

```sh
pio run
```

Build either application explicitly:

```sh
pio run -e esp32-poe-iso  # Modbus readings
pio run -e pwm            # PWM generation
```

The entry points are `src/modbus/main.cpp` and `src/pwm/main.cpp`.

For ESP-IDF/CMake builds, select the application with `-DAPP_MODE=modbus` or
`-DAPP_MODE=pwm` when configuring the project.

## Converter PWM

The PWM application uses the ESP32 MCPWM peripheral for a 20 kHz complementary
output candidate on GPIO13 and GPIO14. Converter PWM is disabled by default:
`PWM_OUTPUTS_VERIFIED`, `PWM_DEAD_TIME_NS`, and `PWM_DUTY_PERCENT` must all be
set appropriately in `src/pwm/main.cpp` before the MCPWM pins are connected.

Do not enable the outputs until the EVLSTGAP3SXS-H input mapping, polarity,
startup behavior, and a suitable dead time have been verified for the assembled
power stage. Confirm the selected GPIOs are available on the specific board and
that an external driver-disable mechanism keeps the switches off during startup.
The ESP32 GPIOs are logic outputs, not gate drivers. Validate waveforms with
appropriate instruments before connecting or energizing the power stage.