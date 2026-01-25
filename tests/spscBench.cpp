#include <benchmark/benchmark.h>

#include "spscRingBufferAdapter.h"

#include "inputtester/core/inputEvent.h"
#include "inputtester/core/spscRingBuffer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#if !defined(__linux__)
#error "tests/spscBench.cpp is intended to run on Linux only."
#endif

#include <pthread.h>
#include <sched.h>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace
{
using steadyClock = std::chrono::steady_clock;
static constexpr int g_consumerCpu = 1;
static constexpr int g_producerCpu = 2;

inline void cpuRelax() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}
} // namespace

static void pinThread(int cpu)
{
    if (cpu < 0)
    {
        return;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    const int rc = ::pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
    {
        std::cerr << "pthread_setaffinity_np: " << std::strerror(rc) << "\n";
        std::exit(EXIT_FAILURE);
    }
}

// Tight-loop throughput-ish benchmark (enqueue as fast as possible; consumer drains continuously).
static void bmSpscRingBuffer(benchmark::State& state)
{
    using value_type = std::int_fast64_t;
    using fifo_type = SpscRingBufferAdapter<value_type>;

    fifo_type fifo(g_benchSize);

    std::thread consumerThread([&]() {
        pinThread(g_consumerCpu);
        for (auto expected = value_type{};; ++expected)
        {
            value_type val = 0;
            while (not fifo.pop(val))
            {
                cpuRelax();
            }
            benchmark::DoNotOptimize(val);
            if (val == -1)
            {
                break;
            }

            if (val != expected)
            {
                throw std::runtime_error("invalid value");
            }
        }
    });

    auto value = value_type{};
    pinThread(g_producerCpu);
    for (auto _ : state)
    {
        while (not fifo.push(value))
        {
            cpuRelax();
        }
        ++value;
    }

    state.counters["ops/sec"] = benchmark::Counter(double(value), benchmark::Counter::kIsRate);
    {
        // Signal the consumer to stop.
        while (not fifo.push(-1))
        {
            cpuRelax();
        }
    }

    consumerThread.join();
}

// Rate + periodic drain benchmark for "mouse @ N Hz, UI drains every M ms".
//
// This models InputTester’s architecture:
// - Producer: input backend thread pushing events continuously
// - Consumer: UI thread waking periodically and draining the queue
//
// It reports drop rate (when tryPush fails) and event "age" on consumption (p50/p99).
static void bmSpscMouseRateDrain(benchmark::State& state)
{
    const auto producerHz = static_cast<std::uint32_t>(state.range(0));
    const auto drainMs = static_cast<std::uint32_t>(state.range(1));
    const auto durationMs = static_cast<std::uint32_t>(state.range(2));

    using Queue = inputTester::spscRingBuffer<inputTester::inputEvent, 1024>;
    Queue queue{};

    std::atomic_bool stopRequested{ false };
    std::atomic<std::uint64_t> produced{ 0 };
    std::atomic<std::uint64_t> enqueued{ 0 };

    std::vector<std::uint64_t> agesNs;
    const auto expectedEvents = static_cast<std::uint64_t>(producerHz) * durationMs / 1000ULL;
    agesNs.reserve(static_cast<std::size_t>(expectedEvents));

    std::thread producerThread([&]() {
        pinThread(g_producerCpu);

        const auto period = std::chrono::nanoseconds(1'000'000'000ULL / producerHz);
        auto next = steadyClock::now();
        std::uint64_t seq = 0;

        while (!stopRequested.load(std::memory_order_relaxed))
        {
            next += period;

            inputTester::inputEvent event{};
            event.timestampNs = inputTester::nowTimestampNs();
            event.deviceId = 1;
            event.device = inputTester::deviceType::mouse;
            event.kind = inputTester::eventKind::unknown;
            event.scanCode = static_cast<std::uint32_t>(seq);
            ++seq;

            produced.fetch_add(1, std::memory_order_relaxed);
            if (queue.tryPush(event))
            {
                enqueued.fetch_add(1, std::memory_order_relaxed);
            }

            while (steadyClock::now() < next)
            {
                if (stopRequested.load(std::memory_order_relaxed))
                {
                    break;
                }
                cpuRelax();
            }
        }
    });

    pinThread(g_consumerCpu);

    for (auto _ : state)
    {
        const auto start = steadyClock::now();
        const auto end = start + std::chrono::milliseconds(durationMs);
        const auto drainPeriod = std::chrono::milliseconds(drainMs);
        auto nextDrain = start + drainPeriod;

        while (steadyClock::now() < end)
        {
            while (steadyClock::now() < nextDrain)
            {
                cpuRelax();
            }

            inputTester::inputEvent event{};
            while (queue.tryPop(event))
            {
                const std::uint64_t now = inputTester::nowTimestampNs();
                agesNs.push_back(now - event.timestampNs);
            }

            nextDrain += drainPeriod;
        }

        stopRequested.store(true, std::memory_order_relaxed);
    }

    producerThread.join();

    const std::uint64_t producedCount = produced.load(std::memory_order_relaxed);
    const std::uint64_t enqueuedCount = enqueued.load(std::memory_order_relaxed);
    const std::uint64_t droppedCount = producedCount - enqueuedCount;
    const std::uint64_t consumedCount = static_cast<std::uint64_t>(agesNs.size());

    std::sort(agesNs.begin(), agesNs.end());

    auto ageAtPercentile = [&](double p) -> std::uint64_t {
        if (agesNs.empty())
        {
            return 0;
        }
        const auto idx = static_cast<std::size_t>((p / 100.0) * (static_cast<double>(agesNs.size() - 1)));
        return agesNs[idx];
    };

    const double durationSeconds = static_cast<double>(durationMs) / 1000.0;
    state.counters["producer_hz"] = static_cast<double>(producerHz);
    state.counters["drain_ms"] = static_cast<double>(drainMs);
    state.counters["duration_ms"] = static_cast<double>(durationMs);
    state.counters["produced"] = static_cast<double>(producedCount);
    state.counters["enqueued"] = static_cast<double>(enqueuedCount);
    state.counters["consumed"] = static_cast<double>(consumedCount);
    state.counters["dropped"] = static_cast<double>(droppedCount);
    state.counters["drops/sec"] = durationSeconds > 0.0 ? (static_cast<double>(droppedCount) / durationSeconds) : 0.0;
    state.counters["p50_age_ns"] = static_cast<double>(ageAtPercentile(50.0));
    state.counters["p99_age_ns"] = static_cast<double>(ageAtPercentile(99.0));
    state.counters["max_age_ns"] = static_cast<double>(ageAtPercentile(100.0));
}

BENCHMARK(bmSpscRingBuffer);
BENCHMARK(bmSpscMouseRateDrain)
    ->Args({ 8000, 16, 2000 })
    ->Args({ 8000, 32, 2000 })
    ->Args({ 8000, 100, 2000 })
    ->Args({ 16000, 16, 2000 })
    ->Args({ 32000, 16, 2000 })
    ->Args({ 64000, 16, 2000 })
    ->Iterations(1);

BENCHMARK_MAIN();
