#ifndef inputTesterPlatformLinuxKeymapParserH
#define inputTesterPlatformLinuxKeymapParserH

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <QByteArray>
#include <QString>

namespace LinuxKeymapParser
{

struct ScanTranslation
{
    std::uint32_t scanCode{};
    bool isExtended{};
};

struct LinuxKeyMap
{
    std::unordered_map<std::uint64_t, std::uint32_t> qtKeyToVirtualKey;
    std::unordered_map<std::uint32_t, ScanTranslation> linuxScanToWinScan;
    std::uint32_t nativeScanCodeOffset{};
};

std::uint64_t makeQtKeyMapKey(int qtKey, bool keypad);
bool parseLinuxKeyMap(const QByteArray& data, LinuxKeyMap* outMap, std::vector<QString>* errors);
QString formatErrors(const std::vector<QString>& errors);

} // namespace LinuxKeymapParser

#endif // inputTesterPlatformLinuxKeymapParserH
