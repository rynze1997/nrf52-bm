#include "fifo.hpp"
#include <cassert>
#include <stdio.h>
#include <string.h>

static void eq(const uint8_t *a, const char *b, int n) {
    assert(n == (int)strlen(b));
    assert(memcmp(a, b, n) == 0);
}

int main() {
    {
        Fifo<4, 16> f;
        uint8_t out[16];
        assert(f.pop(out) == -1);
    }
    {
        Fifo<4, 16> f;
        uint8_t out[16];
        assert(f.push((const uint8_t *)"hi", 2));
        int n = f.pop(out);
        eq(out, "hi", n);
        assert(f.pop(out) == -1);
    }
    {
        Fifo<4, 16> f;
        uint8_t out[16];
        assert(f.push((const uint8_t *)"one", 3));
        assert(f.push((const uint8_t *)"two", 3));
        assert(f.push((const uint8_t *)"three", 5));
        int n = f.pop(out);
        eq(out, "one", n);
        n = f.pop(out);
        eq(out, "two", n);
        n = f.pop(out);
        eq(out, "three", n);
        assert(f.pop(out) == -1);
    }
    {
        Fifo<4, 8> f;
        assert(!f.push((const uint8_t *)"too long!", 9));
        assert(f.push((const uint8_t *)"12345678", 8));
    }
    {
        Fifo<4, 16> f;
        uint8_t out[16];
        assert(f.push((const uint8_t *)"a", 1));
        assert(f.push((const uint8_t *)"b", 1));
        assert(f.push((const uint8_t *)"c", 1));
        assert(!f.push((const uint8_t *)"d", 1));
        int n = f.pop(out);
        eq(out, "a", n);
        assert(f.push((const uint8_t *)"d", 1));
        n = f.pop(out);
        eq(out, "b", n);
        n = f.pop(out);
        eq(out, "c", n);
        n = f.pop(out);
        eq(out, "d", n);
        assert(f.pop(out) == -1);
    }

    printf("fifo tests passed\n");
    return 0;
}
