#ifndef inputTesterCoreSpscRingBufferH
#define inputTesterCoreSpscRingBufferH

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace inputTester
{

template <typename T, std::size_t bufferCapacity> class spscRingBuffer
{
    static_assert(bufferCapacity >= 2, "bufferCapacity must be at least 2");
    static_assert((bufferCapacity & (bufferCapacity - 1)) == 0, "bufferCapacity must be power of two");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
    bool tryPush(const T& item) noexcept
    {
        const auto headIndex{ head_.load(std::memory_order_relaxed) };
        const auto tailIndex{ tail_.load(std::memory_order_acquire) };
        if (isFull(headIndex, tailIndex))
        {
            return false;
        }
        buffer_[indexFor(headIndex)] = item;
        head_.store(headIndex + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(T& out) noexcept
    {
        const auto tailIndex{ tail_.load(std::memory_order_relaxed) };
        const auto headIndex{ head_.load(std::memory_order_acquire) };
        if (isEmpty(headIndex, tailIndex))
        {
            return false;
        }
        out = buffer_[indexFor(tailIndex)];
        tail_.store(tailIndex + 1, std::memory_order_release);
        return true;
    }

    void reset() noexcept
    {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t indexMask{ bufferCapacity - 1 };

    static constexpr std::size_t indexFor(std::size_t absoluteIndex) noexcept
    {
        return absoluteIndex & indexMask;
    }

    static constexpr bool isEmpty(std::size_t headIndex, std::size_t tailIndex) noexcept
    {
        return headIndex == tailIndex;
    }

    static constexpr bool isFull(std::size_t headIndex, std::size_t tailIndex) noexcept
    {
        return (headIndex - tailIndex) == bufferCapacity;
    }

    alignas(64) std::atomic<std::size_t> head_{};
    alignas(64) std::atomic<std::size_t> tail_{};
    std::array<T, bufferCapacity> buffer_{};
};

} // namespace inputTester

#endif // inputTesterCoreSpscRingBufferH
