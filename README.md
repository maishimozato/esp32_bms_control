# ESP32 BMS Control

This PlatformIO project targets the Olimex ESP32-POE-ISO and reads Modbus TCP
holding registers from the BAMS device. The sketch is in `src/ping_bams.cpp`.

## Build

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html)
if the `pio` command is unavailable, and make sure its shell command is on your
`PATH`.

Then run from this project directory:

```sh
pio run
```

## Converter PWM

The sketch uses the ESP32 MCPWM peripheral for a 20 kHz complementary output
candidate on GPIO13 and GPIO14. Converter PWM is disabled by default:
`PWM_OUTPUTS_VERIFIED`, `PWM_DEAD_TIME_NS`, and `PWM_DUTY_PERCENT` must all be
set appropriately in `src/main.cpp` before the MCPWM pins are connected.

Do not enable the outputs until the EVLSTGAP3SXS-H input mapping, polarity,
startup behavior, and a suitable dead time have been verified for the assembled
power stage. Confirm the selected GPIOs are available on the specific board and
that an external driver-disable mechanism keeps the switches off during startup.
The ESP32 GPIOs are logic outputs, not gate drivers. Validate waveforms with
appropriate instruments before connecting or energizing the power stage.