#ifndef inputTesterCoreInputEventH
#define inputTesterCoreInputEventH

#include <chrono>
#include <cstdint>

namespace inputTester
{

enum class deviceType : std::uint8_t
{
    unknown = 0,
    keyboard,
    mouse,
};

enum class eventKind : std::uint8_t
{
    unknown = 0,
    keyDown,
    keyUp,
};

inline std::uint64_t nowTimestampNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct inputEvent
{
    std::uint64_t timestampNs{};
    std::uint32_t deviceId{};
    deviceType device{ deviceType::unknown };
    eventKind kind{ eventKind::unknown };

    std::uint32_t virtualKey{};
    std::uint32_t scanCode{};
    std::uint16_t repeatCount{};
    bool isExtended{ false };
    bool isTextEvent{ false };

    char32_t text{ U'\0' };
};

} // namespace inputTester

#endif // inputTesterCoreInputEventH
