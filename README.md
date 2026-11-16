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