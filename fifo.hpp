#pragma once
#include <stdint.h>
#include <string.h>

// One writer (IRQ), one reader (main). Leave one slot empty so full != empty.
// Each slot is one contiguous string so EasyDMA can TX it later.
template<int SIZE, int MAXLEN>
struct Fifo {
    uint8_t buf[SIZE][MAXLEN]{};
    int len[SIZE]{};
    volatile int head = 0; // next write
    volatile int tail = 0; // next read

    bool push(const uint8_t *data, int n) {
        if (n < 0 || n > MAXLEN) {
            return false;
        }
        int next = (head + 1) % SIZE;
        if (next == tail) {
            return false;
        }
        memcpy(buf[head], data, n);
        len[head] = n;
        head = next;
        return true;
    }

    // Returns length, or -1 if empty.
    int pop(uint8_t *out) {
        if (head == tail) {
            return -1;
        }
        int n = len[tail];
        memcpy(out, buf[tail], n);
        tail = (tail + 1) % SIZE;
        return n;
    }
};
