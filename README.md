# nRF52840 DK — LED1 on

Bare-metal C++ for nRF52840 MCU. Turns on P0.13 (LED1, active low) and sits there.

## Requirements

**Build**

- GNU Make
- Arm GNU Toolchain (`arm-none-eabi-g++`, `arm-none-eabi-objcopy`)

```
sudo apt install gcc-arm-none-eabi make
```

**Flash**

- [nrfutil](https://www.nordicsemi.com/Products/Development-tools/nRF-Util) with the `device` command
- SEGGER J-Link software (`nrfutil device` uses it to program the hex)
- nRF52840 DK (PCA10056)

## Usage

```
make          # build/firmware.hex, prints FLASH/RAM usage
make install  # flash with nrfutil
make clean
```
