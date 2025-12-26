#ifndef inputTesterPlatformLinuxKeymapParserH
#define inputTesterPlatformLinuxKeymapParserH

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <QByteArray>
#include <QString>

namespace linuxKeymapParser
{

    struct scanTranslation
    {
        std::uint32_t scanCode{};
        bool isExtended{};
    };

    struct linuxKeyMap
    {
        std::unordered_map<std::uint64_t, std::uint32_t> qtKeyToVirtualKey{};
        std::unordered_map<std::uint32_t, scanTranslation> linuxScanToWinScan{};
        std::uint32_t nativeScanCodeOffset{};
    };

    std::uint64_t makeQtKeyMapKey(int qtKey, bool keypad);
    bool parseLinuxKeyMap(const QByteArray &data, linuxKeyMap *outMap, std::vector<QString> *errors);
    QString formatErrors(const std::vector<QString> &errors);

} // namespace linuxKeymapParser

#endif // inputTesterPlatformLinuxKeymapParserH
