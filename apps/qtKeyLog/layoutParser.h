#ifndef inputTesterAppsLayoutParserH
#define inputTesterAppsLayoutParserH

#include <cstddef>
#include <cstdint>
#include <vector>

#include <QByteArray>
#include <QRectF>
#include <QString>

namespace LayoutParser
{

struct GeometryKey
{
    QString label;
    QRectF rect;
    qreal rotation{};
    qreal rx{};
    qreal ry{};
};

struct MappingEntry
{
    std::uint32_t virtualKey{};
    std::uint32_t scanCode{};
};

bool parseKleGeometry(const QByteArray& data, std::vector<GeometryKey>* outKeys, std::vector<QString>* errors);
bool parseMapping(const QByteArray& data, std::size_t keyCount, std::vector<MappingEntry>* outEntries,
                  std::vector<QString>* errors);

} // namespace LayoutParser

#endif // inputTesterAppsLayoutParserH
