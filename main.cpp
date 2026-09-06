#include "fifo.hpp"
#include "nrf52840.h"
#include "nrf52840_bitfields.h"
#include "parser.hpp"
#include <string.h>

struct Pin {
    uint8_t pin;
    uint8_t gpiote;
    uint8_t cc;
    uint8_t ppi;

    void init() const {
        NRF_P0->OUTSET = 1u << pin;
        NRF_P0->PIN_CNF[pin] =
            (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
            (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
        NRF_GPIOTE->CONFIG[gpiote] =
            (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
            (uint32_t(pin) << GPIOTE_CONFIG_PSEL_Pos) |
            (GPIOTE_CONFIG_POLARITY_None << GPIOTE_CONFIG_POLARITY_Pos) |
            (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);
        NRF_PPI->CH[ppi].EEP =
            reinterpret_cast<uint32_t>(&NRF_TIMER0->EVENTS_COMPARE[cc]);
        NRF_PPI->CH[ppi].TEP =
            reinterpret_cast<uint32_t>(&NRF_GPIOTE->TASKS_SET[gpiote]);
    }

    void pulse(uint32_t us) const {
        // Turn on pin with CPU. OFF is TIMER COMPARE → PPI → TASKS_SET
        NRF_PPI->CHENCLR = 1u << ppi;
        NRF_TIMER0->TASKS_CAPTURE[cc] = 1;
        NRF_TIMER0->CC[cc] = NRF_TIMER0->CC[cc] + us;
        NRF_TIMER0->EVENTS_COMPARE[cc] = 0;
        NRF_TIMER0->INTENSET = 1u << (TIMER_INTENSET_COMPARE0_Pos + cc);
        NRF_PPI->CHENSET = 1u << ppi;
        NRF_GPIOTE->TASKS_CLR[gpiote] = 1; // GPIOTE owns the pin; this is the OUTCLR
    }

    void ack_off() const {
        NRF_TIMER0->EVENTS_COMPARE[cc] = 0;
        NRF_TIMER0->INTENCLR = 1u << (TIMER_INTENSET_COMPARE0_Pos + cc);
    }
};

static volatile uint8_t uart_rx_buf[2];
static uint8_t uart_rx_i;
static uint8_t uart_tx_buffer[320];
static volatile bool uart_tx_busy;
static Fifo<512, 1> uart_rx_fifo;   // bytes from IRQ
static Fifo<8, 320> uart_tx_fifo;   // whole replies for DMA

struct Uart {
    NRF_UARTE_Type* u;

    static void pin_out(uint8_t n) {
        NRF_P0->OUTSET = 1u << n;
        NRF_P0->PIN_CNF[n] =
            (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
            (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    }

    static void pin_in(uint8_t n) {
        NRF_P0->PIN_CNF[n] =
            (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
            (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
            (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);
    }

    void init(uint8_t tx, uint8_t rx) const {
        pin_out(tx);
        pin_in(rx);

        u->PSEL.TXD = tx;
        u->PSEL.RXD = rx;
        u->BAUDRATE = UARTE_BAUD;
        // 8 data bits (fixed), no parity, 1 stop bit, no HWFC
        u->CONFIG = (UARTE_CONFIG_HWFC_Disabled << UARTE_CONFIG_HWFC_Pos) |
                    (UARTE_CONFIG_PARITY_Excluded << UARTE_CONFIG_PARITY_Pos) |
                    (UARTE_CONFIG_STOP_One << UARTE_CONFIG_STOP_Pos);
        u->ENABLE = UARTE_ENABLE_ENABLE_Enabled;
        u->EVENTS_ENDRX = 0;
        u->EVENTS_ENDTX = 0;
        u->ERRORSRC = 0xFFFFFFFF;
        u->SHORTS = UARTE_SHORTS_ENDRX_STARTRX_Msk;
        u->RXD.PTR = reinterpret_cast<uint32_t>(&uart_rx_buf[0]);
        u->RXD.MAXCNT = 1;
        u->INTENSET = UARTE_INTENSET_ENDRX_Msk | UARTE_INTENSET_ENDTX_Msk;
        u->TASKS_STARTRX = 1;
        u->RXD.PTR = reinterpret_cast<uint32_t>(&uart_rx_buf[1]); // EasyDMA next

        NVIC_SetPriority(UART0_UARTE0_IRQn, 1);
        NVIC_EnableIRQ(UART0_UARTE0_IRQn);
    }

    void send(const uint8_t* str, uint16_t len) const {
        uart_tx_busy = true;
        u->TXD.PTR = reinterpret_cast<uint32_t>(str);
        u->TXD.MAXCNT = len;
        u->TASKS_STARTTX = 1;
    }
};

static const Pin led1{LED1_PIN, 0, 0, 0};
static const Pin led2{LED2_PIN, 1, 1, 1};
static const Uart uart{NRF_UARTE0};
static Parser parser;
static volatile bool led1_off_pending;
static volatile bool led2_off_pending;

extern "C" int main() {
    led1.init();
    led2.init();

    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (!NRF_CLOCK->EVENTS_HFCLKSTARTED) {
    }

    // nRF52840 DK VCOM: P0.06 TX, P0.08 RX
    uart.init(6, 8);

    // TIMER0: 16 MHz / 2^4 = 1 MHz -> 1 us resolution
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    NRF_TIMER0->PRESCALER = 4;
    NRF_TIMER0->EVENTS_COMPARE[led1.cc] = 0;
    NRF_TIMER0->EVENTS_COMPARE[led2.cc] = 0;
    NRF_TIMER0->TASKS_CLEAR = 1;
    NVIC_SetPriority(TIMER0_IRQn, 5);
    NVIC_EnableIRQ(TIMER0_IRQn);
    NRF_TIMER0->TASKS_START = 1;


    for (;;) {
        if (led1_off_pending) {
            led1_off_pending = false;
            uart_tx_fifo.push(reinterpret_cast<const uint8_t*>("led-off: 0\r\n"), 12);
        }
        if (led2_off_pending) {
            led2_off_pending = false;
            uart_tx_fifo.push(reinterpret_cast<const uint8_t*>("led-off: 1\r\n"), 12);
        }

        while (parser.result == Parser::Result::None) {
            uint8_t c;
            int len = uart_rx_fifo.pop(&c);
            if (len <= 0) {
                break;
            }
            parser.feed(c);
        }

        if (parser.result == Parser::Result::Ok && parser.cmd == Parser::Cmd::SetLed) {
            if (parser.led_id == 0) {
                led1.pulse(uint32_t(parser.time_ms) * 1000u);
            } else {
                led2.pulse(uint32_t(parser.time_ms) * 1000u);
            }
            uart_tx_fifo.push(reinterpret_cast<const uint8_t*>("OK\r\n"), 4);
            parser.reset();
        } else if (parser.result == Parser::Result::Ok && parser.cmd == Parser::Cmd::Echo) {
            uint8_t reply_string[320];
            memcpy(reply_string, "data: ", 6);
            memcpy(reply_string + 6, parser.data, parser.len);
            memcpy(reply_string + 6 + parser.len, "\r\nOK\r\n", 6);
            uart_tx_fifo.push(reply_string, 12 + parser.len);
            parser.reset();
        } else if (parser.result == Parser::Result::Error) {
            uart_tx_fifo.push(reinterpret_cast<const uint8_t*>("ERROR\r\n"), 7);
            parser.reset();
        }

        if (uart_tx_busy) {
            continue;
        }

        int len = uart_tx_fifo.pop(&uart_tx_buffer[0]);
        if (len > 0) {
            uart.send(&uart_tx_buffer[0], len);
        }
    }
}


extern "C" void TIMER0_IRQHandler() {
    if (NRF_TIMER0->EVENTS_COMPARE[led1.cc]) {
        led1.ack_off();
        led1_off_pending = true;
    }
    if (NRF_TIMER0->EVENTS_COMPARE[led2.cc]) {
        led2.ack_off();
        led2_off_pending = true;
    }
}

extern "C" void UART0_UARTE0_IRQHandler() {

    if (NRF_UARTE0->EVENTS_ENDTX) {
        NRF_UARTE0->EVENTS_ENDTX = 0;
        uart_tx_busy = false;
    }

    if (NRF_UARTE0->EVENTS_ENDRX) {
        NRF_UARTE0->EVENTS_ENDRX = 0;
        uint8_t done = uart_rx_i;
        uart_rx_i ^= 1;
        uint8_t c = uart_rx_buf[done];
        NRF_UARTE0->RXD.PTR = reinterpret_cast<uint32_t>(&uart_rx_buf[done]);
        uart_rx_fifo.push(&c, 1);
    }
}