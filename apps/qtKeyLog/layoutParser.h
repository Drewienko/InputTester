#ifndef inputTesterAppsLayoutParserH
#define inputTesterAppsLayoutParserH

#include <cstddef>
#include <cstdint>
#include <vector>

#include <QByteArray>
#include <QRectF>
#include <QString>

namespace layoutParser
{

    struct geometryKey
    {
        QString label{};
        QRectF rect{};
    };

    struct mappingEntry
    {
        std::uint32_t virtualKey{};
        std::uint32_t scanCode{};
    };

    bool parseKleGeometry(const QByteArray &data, std::vector<geometryKey> *outKeys,
                          std::vector<QString> *errors);
    bool parseMapping(const QByteArray &data, std::size_t keyCount, std::vector<mappingEntry> *outEntries,
                      std::vector<QString> *errors);

} // namespace layoutParser

#endif // inputTesterAppsLayoutParserH
