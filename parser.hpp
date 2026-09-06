#pragma once
#include <stdint.h>

struct Parser {
    enum class Result : uint8_t { None, Ok, Error };
    enum class Cmd : uint8_t { None, SetLed, Echo };

    volatile Result result = Result::None;
    Cmd cmd = Cmd::None;
    uint8_t led_id = 0;
    uint16_t time_ms = 0;
    uint16_t len = 0;
    uint8_t data[300]{};

    void reset() { *this = Parser{}; }

    void feed(uint8_t c) {
        if (result != Result::None) {
            return;
        }
        switch (state) {
        case State::Start:
            if (c == '\n') {
                return;
            }
            if (c == '\r') {
                result = Result::Error;
                return;
            }
            if (c == 's') {
                cmd = Cmd::SetLed;
                i = 1;
                state = State::Prefix;
                return;
            }
            if (c == 'e') {
                cmd = Cmd::Echo;
                i = 1;
                state = State::Prefix;
                return;
            }
            state = State::Drain;
            return;
        case State::Prefix: {
            const char* pfx = (cmd == Cmd::Echo) ? kEcho : kSetLed;
            if (c != pfx[i]) {
                reject(c);
                return;
            }
            if (pfx[++i] == '\0') {
                if (cmd == Cmd::Echo) {
                    acc = 0;
                    i = 0;
                    state = State::Len;
                } else {
                    state = State::LedId;
                }
            }
            return;
        }
        case State::LedId:
            if (c != '0' && c != '1') {
                reject(c);
                return;
            }
            led_id = static_cast<uint8_t>(c - '0'); // 49-48=1, 49-49=0
            state = State::Comma;
            return;
        case State::Comma:
            if (c != ',') {
                reject(c);
                return;
            }
            acc = 0;
            state = State::Time;
            return;
        case State::Time:
            if (c == '\r') {
                if (acc >= 1 && acc <= 4999) {
                    time_ms = acc;
                    result = Result::Ok;
                } else {
                    result = Result::Error;
                }
                return;
            }
            if (c < '0' || c > '9') {
                state = State::Drain;
                return;
            }
            {
                const uint32_t next = uint32_t(acc) * 10u + uint32_t(c - '0'); // Slide one decimal digit left and add new digit.
                if (next > 4999) {
                    state = State::Drain;
                    return;
                }
                acc = static_cast<uint16_t>(next);
            }
            return;
        case State::Len:
            if (c == ',') {
                if (i == 0) {
                    reject(c);
                    return;
                }
                i = 0;
                state = State::Bin;
                return;
            }
            if (c < '0' || c > '9') {
                reject(c);
                return;
            }
            {
                const uint32_t next = uint32_t(acc) * 10u + uint32_t(c - '0');
                if (next > 300) {
                    reject(c);
                    return;
                }
                acc = static_cast<uint16_t>(next);
                ++i;
            }
            return;
        case State::Bin:
            if (i < acc) {
                data[i++] = c;
                return;
            }
            if (c == '\r') {
                len = acc;
                result = Result::Ok;
                return;
            }
            reject(c);
            return;
        case State::Drain:
            if (c == '\r') {
                result = Result::Error;
            }
            return;
        }
    }

private:
    enum class State : uint8_t { Start, Prefix, LedId, Comma, Time, Len, Bin, Drain };

    static constexpr char kSetLed[] = "set-led ";
    static constexpr char kEcho[] = "echo ";

    State state = State::Start;
    uint16_t i = 0;
    uint16_t acc = 0;

    void reject(uint8_t c) {
        state = State::Drain;
        if (c == '\r') {
            result = Result::Error;
        }
    }
};
