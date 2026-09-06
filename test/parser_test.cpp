#include "parser.hpp"
#include <cassert>
#include <stdio.h>

static void feed(Parser& p, const char* s) {
    while (*s) {
        p.feed(static_cast<uint8_t>(*s++));
    }
}

int main() {
    {
        Parser p;
        feed(p, "set-led 0,500\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.cmd == Parser::Cmd::SetLed);
        assert(p.led_id == 0);
        assert(p.time_ms == 500);
    }
    {
        Parser p;
        feed(p, "set-led 1,1\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.led_id == 1);
        assert(p.time_ms == 1);
    }
    {
        Parser p;
        feed(p, "set-led 0,4999\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.time_ms == 4999);
    }
    {
        Parser p;
        feed(p, "set-led 0,0500\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.time_ms == 500);
    }
    {
        Parser p;
        feed(p, "\nset-led 0,1\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.led_id == 0);
        assert(p.time_ms == 1);
    }
    {
        Parser p;
        feed(p, "set-led 0,5000\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "set-led 0,0\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "set-led 2,100\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "set-led 0,\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "set-led 0,500x\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "set\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "echo 1,x\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.cmd == Parser::Cmd::Echo);
        assert(p.len == 1);
        assert(p.data[0] == 'x');
    }
    {
        Parser p;
        feed(p, "echo 0,\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.cmd == Parser::Cmd::Echo);
        assert(p.len == 0);
    }
    {
        Parser p;
        feed(p, "echo 5,hello\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.len == 5);
        assert(p.data[0] == 'h');
        assert(p.data[4] == 'o');
    }
    {
        Parser p;
        feed(p, "echo 3,");
        p.feed('a');
        p.feed('\r');
        p.feed('b');
        p.feed('\r');
        assert(p.result == Parser::Result::Ok);
        assert(p.len == 3);
        assert(p.data[0] == 'a');
        assert(p.data[1] == '\r');
        assert(p.data[2] == 'b');
    }
    {
        Parser p;
        feed(p, "echo 1,xy\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "echo ,x\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "echo 301,\r");
        assert(p.result == Parser::Result::Error);
    }
    {
        Parser p;
        feed(p, "echo 300,");
        for (int n = 0; n < 300; ++n) {
            p.feed('z');
        }
        p.feed('\r');
        assert(p.result == Parser::Result::Ok);
        assert(p.len == 300);
        assert(p.data[0] == 'z');
        assert(p.data[299] == 'z');
    }
    {
        Parser p;
        feed(p, "set-led 0,10");
        assert(p.result == Parser::Result::None);
        p.feed('\r');
        assert(p.result == Parser::Result::Ok);
        assert(p.time_ms == 10);
    }
    {
        Parser p;
        feed(p, "set-led 0,1\r");
        p.feed('x');
        assert(p.result == Parser::Result::Ok);
        p.reset();
        feed(p, "set-led 1,2\r");
        assert(p.result == Parser::Result::Ok);
        assert(p.led_id == 1);
        assert(p.time_ms == 2);
    }
    puts("ok");
    return 0;
}
