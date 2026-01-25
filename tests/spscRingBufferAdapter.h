#pragma once

#include "inputtester/core/spscRingBuffer.h"
#include <cassert>
#include <cstddef>

constexpr std::size_t g_benchSize = 131072;

template <typename T> class SpscRingBufferAdapter
{
public:
    using value_type = T;

    explicit SpscRingBufferAdapter(std::size_t capacity) : m_buffer{}
    {
        assert(capacity == g_benchSize && "This adapter uses a fixed g_benchSize capacity");
    }
    bool push(const T& item)
    {
        return m_buffer.tryPush(item);
    }

    bool pop(T& item)
    {
        return m_buffer.tryPop(item);
    }

    bool empty() const
    {
        return m_buffer.isEmpty();
    }

private:
    inputTester::spscRingBuffer<T, g_benchSize> m_buffer;
};
