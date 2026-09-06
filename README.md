# nRF52840 DK — LED pulses + UART commands

Bare-metal C++ on the nRF52840. No OS, no heap, no vendor HAL. Register headers only (Nordic MDK + ARM CMSIS-Core). Two DK LEDs are pulsed from `set-led`; `echo` returns a binary payload. UART0 is the command port.

`LED1` / `LED2` are P0 pin numbers and must differ. Allowed pins:

- 13 — board LED1
- 14 — board LED2
- 15 — board LED3
- 16 — board LED4

DK LEDs are **active-low** (pin sinks current through the LED to VDD). `set-led` drives the pin **low** for the pulse, then **high**.

Defaults: LED1=13, LED2=14, BAUD=115200.

## Build

Open-source toolchain:

- GNU Make
- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-g++`, `arm-none-eabi-objcopy`) — Debian/Ubuntu:
`sudo apt install gcc-arm-none-eabi make`

Flash:

- [nrfutil](https://www.nordicsemi.com/Products/Development-tools/nRF-Util)
(`device` command)
- [SEGGER J-Link](https://www.segger.com/downloads/jlink/) (`nrfutil device`
uses it)
- nRF52840 DK (PCA10056)

```
make
make test
make LED1=15 LED2=16
make BAUD=9600 install
make clean
```

`BAUD` must be an nRF UARTE rate: 1200, 2400, 4800, 9600, 14400, 19200, 28800,
31250, 38400, 56000, 57600, 76800, 115200, 230400, 250000, 460800, 921600,
1000000.

## UART
Pins: P0.06 TX, P0.08 RX.

Commands end with `\r`. Replies are `OK\r\n` or `ERROR\r\n`.


| Command               | Reply                                                         |
| --------------------- | ------------------------------------------------------------- |
| `set-led <id>,<ms>\r` | `OK\r\n` immediately; `led-off: <id>\r\n` when the pulse ends |
| `echo <len>,<data>\r` | `data: <data>\r\nOK\r\n`                                      |


`<id>` is `0` or `1`. `<ms>` is `[1, 5000)`. `<len>` is `[0, 300]`. `<data>` is
exactly `<len>` bytes and may contain `\r` (`0x0D`) or any other octet.

## Sizes

UART frame is 8N1 → **10 bits/byte**. Wire time for `N` bytes at baud `B`:

```
t = 10 · N / B
```

Worst-case `echo` (spec max payload 300):


| Direction  | Bytes | Contents                                  |
| ---------- | ----- | ----------------------------------------- |
| RX command | 310   | `"echo 300,"` (9) + 300 + `\r`            |
| TX reply   | 312   | `"data: "` (6) + 300 + `"\r\nOK\r\n"` (6) |
| round trip | 622   | RX + TX, no host gap                      |


Largest `set-led`: `"set-led 0,4999\r"` = 16 RX, then `"OK\r\n"` = 4 TX.
Later `"led-off: 0\r\n"` = 12 TX, not on the command deadline.
`ERROR\r\n` = 7 TX.

Buffers (no heap):

- parser payload `data[300]`
- TX DMA buffer 320 (≥ 312)
- RX FIFO 512 with one spare slot → 511 bytes (≥ 310)

CPU time after `\r` (parse + `memcpy` of 312 bytes on 64 MHz M4) is a few
microseconds. Deadline is UART wire time.

## 100 ms after `\r`

The spec asks for `OK` / `ERROR` within 100 ms of `\r`. For `set-led` / errors
that is 4 or 7 TX bytes. For `echo` the `OK` is *after* the 300-byte payload, so
the last bit of `OK` is 312 TX bytes after `\r`.

Need `10 · 312 / B ≤ 0.1` → **B ≥ 31200**.


| Baud       | t_byte  | echo `OK` (312 B) | `set-led` `OK` (4 B) | `ERROR` (7 B) | echo RTT (622 B) | echo cmds/s |
| ---------- | ------- | ----------------- | -------------------- | ------------- | ---------------- | ----------- |
| 1200       | 8.33 ms | 2600 ms           | 33.3 ms              | 58.3 ms       | 5183 ms          | 0.19        |
| 2400       | 4.17 ms | 1300 ms           | 16.7 ms              | 29.2 ms       | 2592 ms          | 0.39        |
| 4800       | 2.08 ms | 650 ms            | 8.3 ms               | 14.6 ms       | 1296 ms          | 0.77        |
| 9600       | 1.04 ms | 325 ms            | 4.2 ms               | 7.3 ms        | 648 ms           | 1.54        |
| 14400      | 694 µs  | 217 ms            | 2.8 ms               | 4.9 ms        | 432 ms           | 2.32        |
| 19200      | 521 µs  | 163 ms            | 2.1 ms               | 3.6 ms        | 324 ms           | 3.09        |
| 28800      | 347 µs  | 108 ms            | 1.4 ms               | 2.4 ms        | 216 ms           | 4.63        |
| 31250      | 320 µs  | 99.8 ms           | 1.3 ms               | 2.2 ms        | 199 ms           | 5.02        |
| 38400      | 260 µs  | 81.3 ms           | 1.0 ms               | 1.8 ms        | 162 ms           | 6.17        |
| 56000      | 179 µs  | 55.7 ms           | 0.71 ms              | 1.3 ms        | 111 ms           | 9.00        |
| 57600      | 174 µs  | 54.2 ms           | 0.69 ms              | 1.2 ms        | 108 ms           | 9.26        |
| 76800      | 130 µs  | 40.6 ms           | 0.52 ms              | 0.91 ms       | 81.0 ms          | 12.3        |
| **115200** | 86.8 µs | **27.1 ms**       | 0.35 ms              | 0.61 ms       | 54.0 ms          | 18.5        |
| 230400     | 43.4 µs | 13.5 ms           | 0.17 ms              | 0.30 ms       | 27.0 ms          | 37.0        |
| 250000     | 40.0 µs | 12.5 ms           | 0.16 ms              | 0.28 ms       | 24.9 ms          | 40.2        |
| 460800     | 21.7 µs | 6.8 ms            | 87 µs                | 0.15 ms       | 13.5 ms          | 74.1        |
| 921600     | 10.8 µs | 3.4 ms            | 43 µs                | 76 µs         | 6.75 ms          | 148         |
| 1000000    | 10.0 µs | 3.1 ms            | 40 µs                | 70 µs         | 6.22 ms          | 161         |


- Every listed baud meets 100 ms for `set-led` `OK` and `ERROR` (max 58 ms at 1200).
- Worst-case `echo` `OK` last bit in 100 ms: **31250 and up**. 31250 is 99.8 ms
with no margin (UARTE baud is a register approximation). **38400+** is the
first rate with headroom. Default **115200** is 27 ms.
- Spec minimum 1 command/s: any baud for `set-led`. Worst-case `echo` needs
`10 · 622 / B ≤ 1` → **B ≥ 6220**, so **9600+**.

nRF UARTE baud registers are approximations of the named rates. Times above
use the nominal value.

## LED pulse

TIMER0: HFCLK 16 MHz, prescaler 4 → **1 MHz, 1 µs tick**, 32-bit.
Duration `ms` becomes `ms · 1000` ticks. Range 1–4999 ms → 1000–4 999 000 µs.

LED on is a CPU `GPIOTE TASKS_CLR`. LED off is TIMER COMPARE → PPI →
`GPIOTE TASKS_SET` (no IRQ on the falling edge). COMPARE IRQ only queues
`led-off:`.

Pulse length is short by the CAPTURE → `TASKS_CLR` gap (a couple of
microseconds). Crystal / HFCLK error is tens of ppm (≈100 µs on a 5 s pulse).