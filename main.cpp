#include "nrf52840.h"
#include "nrf52840_bitfields.h"

struct Pin {
    NRF_GPIO_Type* port;
    uint8_t n;

    void output() const {
        port->PIN_CNF[n] =
            (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
            (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);
    }
    void set() const { port->OUTSET = 1u << n; }
    void clear() const { port->OUTCLR = 1u << n; }
};

extern "C" int main() {
    const Pin led1{NRF_P0, 13};
    led1.output();
    led1.clear();
    for (;;) {
    }
}
