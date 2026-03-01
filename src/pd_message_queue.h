#ifndef PD_MESSAGE_QUEUE_H
#define PD_MESSAGE_QUEUE_H

#include <cstdint>
#include <cstring>
#include <atomic>

// Lock-free single-producer single-consumer ring buffer for Pd messages.
// Producer: Arduino loop() thread. Consumer: audio task thread.

enum PdMsgType : uint8_t {
    PD_MSG_FLOAT,
    PD_MSG_BANG,
    PD_MSG_SYMBOL
};

struct PdMessage {
    PdMsgType type;
    char receiver[32];
    union {
        float floatVal;
        char symbolVal[32];
    };
};

class PdMessageQueue {
public:
    static constexpr int CAPACITY = 64;

    PdMessageQueue() : _head(0), _tail(0) {}

    // Push a message (producer side). Returns false if full.
    bool push(const PdMessage& msg) {
        uint32_t head = _head.load(std::memory_order_relaxed);
        uint32_t next = (head + 1) % CAPACITY;
        if (next == _tail.load(std::memory_order_acquire)) {
            return false; // full
        }
        _buffer[head] = msg;
        _head.store(next, std::memory_order_release);
        return true;
    }

    // Pop a message (consumer side). Returns false if empty.
    bool pop(PdMessage& msg) {
        uint32_t tail = _tail.load(std::memory_order_relaxed);
        if (tail == _head.load(std::memory_order_acquire)) {
            return false; // empty
        }
        msg = _buffer[tail];
        _tail.store((tail + 1) % CAPACITY, std::memory_order_release);
        return true;
    }

    bool isEmpty() const {
        return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
    }

private:
    PdMessage _buffer[CAPACITY];
    std::atomic<uint32_t> _head;
    std::atomic<uint32_t> _tail;
};

#endif // PD_MESSAGE_QUEUE_H
