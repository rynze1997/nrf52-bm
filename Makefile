TOOLCHAIN_PATH = arm-none-eabi-
CXX      = $(TOOLCHAIN_PATH)g++
OBJCOPY  = $(TOOLCHAIN_PATH)objcopy

MDK      = mdk

CXXFLAGS = -mcpu=cortex-m4 -mfloat-abi=soft -ffreestanding -fno-exceptions -fno-rtti -I$(MDK)
LDFLAGS  = -T linker/nrf52840.ld -nostdlib -Wl,--print-memory-usage -Wl,-Map=$(BUILD)/firmware.map
SRCS     = main.cpp startup/startup_nrf52840.S
BUILD    = build

all: $(BUILD)/firmware.hex

$(BUILD):
	mkdir -p $@

$(BUILD)/firmware.elf: $(SRCS) linker/nrf52840.ld | $(BUILD)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRCS)

$(BUILD)/firmware.hex: $(BUILD)/firmware.elf
	$(OBJCOPY) -O ihex $< $@

install: $(BUILD)/firmware.hex
	nrfutil device program --firmware $< --options verify=VERIFY_READ,reset=RESET_SYSTEM

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
