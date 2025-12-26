#include "layoutParser.h"

#include <algorithm>
#include <cmath>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace
{
    void appendError(std::vector<QString> *errors, const QString &path, const QString &message)
    {
        if (errors == nullptr)
        {
            return;
        }
        errors->push_back(QString("%1: %2").arg(path, message));
    }

    bool tryParseDouble(const QJsonValue &value, double *out)
    {
        if (out == nullptr)
        {
            return false;
        }
        if (value.isDouble())
        {
            *out = value.toDouble();
            return true;
        }
        if (value.isString())
        {
            bool ok{false};
            const auto parsed{value.toString().toDouble(&ok)};
            if (!ok)
            {
                return false;
            }
            *out = parsed;
            return true;
        }
        return false;
    }

    bool tryParseUnsigned(const QJsonValue &value, std::uint32_t *out)
    {
        if (out == nullptr)
        {
            return false;
        }
        if (value.isDouble())
        {
            const auto number{value.toDouble()};
            if (number < 0.0 || std::floor(number) != number)
            {
                return false;
            }
            *out = static_cast<std::uint32_t>(number);
            return true;
        }
        if (value.isString())
        {
            bool ok{false};
            const auto parsed{value.toString().toUInt(&ok, 0)};
            if (!ok)
            {
                return false;
            }
            *out = parsed;
            return true;
        }
        return false;
    }

    bool tryParseIndex(const QJsonValue &value, int *out)
    {
        if (out == nullptr)
        {
            return false;
        }
        if (value.isDouble())
        {
            const auto number{value.toDouble()};
            if (number < 0.0 || std::floor(number) != number)
            {
                return false;
            }
            *out = static_cast<int>(number);
            return true;
        }
        if (value.isString())
        {
            bool ok{false};
            const auto parsed{value.toString().toInt(&ok, 0)};
            if (!ok || parsed < 0)
            {
                return false;
            }
            *out = parsed;
            return true;
        }
        return false;
    }

    bool isLabelArray(const QJsonArray &array)
    {
        if (array.isEmpty())
        {
            return false;
        }
        return std::all_of(array.begin(), array.end(),
                           [](const QJsonValue &item)
                           { return item.isString(); });
    }

    QString valueTypeName(const QJsonValue &value)
    {
        if (value.isString())
        {
            return QStringLiteral("string");
        }
        if (value.isDouble())
        {
            return QStringLiteral("number");
        }
        if (value.isBool())
        {
            return QStringLiteral("bool");
        }
        if (value.isArray())
        {
            return QStringLiteral("array");
        }
        if (value.isObject())
        {
            return QStringLiteral("object");
        }
        if (value.isNull())
        {
            return QStringLiteral("null");
        }
        return QStringLiteral("undefined");
    }

    QString joinLabelArray(const QJsonArray &labels)
    {
        QStringList parts{};
        for (const auto &item : labels)
        {
            if (item.isString())
            {
                parts.push_back(item.toString());
            }
        }
        return parts.join(QLatin1Char{'\n'});
    }

    void appendLabelValue(const QJsonValue &value, QStringList *parts, bool *hasLabel,
                          std::vector<QString> *errors, const QString &path, int depth)
    {
        if (parts == nullptr || hasLabel == nullptr)
        {
            return;
        }
        if (depth > 32)
        {
            appendError(errors, path, "label nesting too deep");
            return;
        }
        if (value.isArray())
        {
            const auto nested{value.toArray()};
            if (nested.isEmpty())
            {
                appendError(errors, path, "label array is empty");
                return;
            }
            for (int nestedIndex{0}; nestedIndex < nested.size(); ++nestedIndex)
            {
                const auto nestedItem{nested.at(nestedIndex)};
                const auto nestedPath{QString("%1[%2]").arg(path).arg(nestedIndex)};
                appendLabelValue(nestedItem, parts, hasLabel, errors, nestedPath, depth + 1);
            }
            return;
        }
        if (value.isString())
        {
            parts->push_back(value.toString());
            *hasLabel = true;
            return;
        }
        if (value.isDouble())
        {
            parts->push_back(QString::number(value.toDouble()));
            *hasLabel = true;
            return;
        }
        if (value.isNull() || value.isUndefined())
        {
            parts->push_back(QString{});
            return;
        }
        appendError(errors, path, QString("expected string label (got %1)").arg(valueTypeName(value)));
    }

} // namespace

namespace layoutParser
{

    bool parseKleGeometry(const QByteArray &data, std::vector<geometryKey> *outKeys,
                          std::vector<QString> *errors)
    {
        if (outKeys == nullptr)
        {
            appendError(errors, "geometry", "output is null");
            return false;
        }

        QJsonParseError parseError{};
        const auto doc{QJsonDocument::fromJson(data, &parseError)};
        if (doc.isNull() || !doc.isArray())
        {
            appendError(errors, "geometry", QString("invalid KLE json (%1@%2)").arg(parseError.errorString()).arg(parseError.offset));
            return false;
        }

        auto root{doc.array()};
        if (root.size() == 1 && root.at(0).isArray())
        {
            root = root.at(0).toArray();
        }

        int rowStart{0};
        while (rowStart < root.size() && root.at(rowStart).isObject())
        {
            ++rowStart;
        }
        if (rowStart >= root.size())
        {
            appendError(errors, "geometry", "KLE json has no rows");
            return false;
        }

        std::vector<geometryKey> parsedKeys{};
        qreal currentY{0.0};
        int rowIndex{0};
        for (int index{rowStart}; index < root.size(); ++index, ++rowIndex)
        {
            const auto rowValue{root.at(index)};
            const auto rowPath{QString("geometry.rows[%1]").arg(rowIndex)};
            if (!rowValue.isArray())
            {
                appendError(errors, rowPath, "expected row array");
                continue;
            }

            auto row{rowValue.toArray()};
            int unwrapDepth{0};
            while (row.size() == 1 && row.at(0).isArray() && !isLabelArray(row.at(0).toArray()))
            {
                row = row.at(0).toArray();
                ++unwrapDepth;
                if (unwrapDepth > 4)
                {
                    appendError(errors, rowPath, "row nesting too deep");
                    break;
                }
            }
            qreal currentX{0.0};
            qreal currentWidth{1.0};
            qreal currentHeight{1.0};
            qreal rowHeight{1.0};

            for (int itemIndex{0}; itemIndex < row.size(); ++itemIndex)
            {
                const auto item{row.at(itemIndex)};
                const auto itemPath{QString("%1[%2]").arg(rowPath).arg(itemIndex)};
                if (item.isObject())
                {
                    const auto obj{item.toObject()};
                    if (obj.contains("x"))
                    {
                        double value{0.0};
                        if (tryParseDouble(obj.value("x"), &value))
                        {
                            currentX += value;
                        }
                        else
                        {
                            appendError(errors, itemPath + ".x", "expected number");
                        }
                    }
                    if (obj.contains("y"))
                    {
                        double value{0.0};
                        if (tryParseDouble(obj.value("y"), &value))
                        {
                            currentY += value;
                        }
                        else
                        {
                            appendError(errors, itemPath + ".y", "expected number");
                        }
                    }
                    if (obj.contains("w"))
                    {
                        double value{0.0};
                        if (tryParseDouble(obj.value("w"), &value))
                        {
                            if (value <= 0.0)
                            {
                                appendError(errors, itemPath + ".w", "expected positive number");
                            }
                            else
                            {
                                currentWidth = value;
                            }
                        }
                        else
                        {
                            appendError(errors, itemPath + ".w", "expected number");
                        }
                    }
                    if (obj.contains("h"))
                    {
                        double value{0.0};
                        if (tryParseDouble(obj.value("h"), &value))
                        {
                            if (value <= 0.0)
                            {
                                appendError(errors, itemPath + ".h", "expected positive number");
                            }
                            else
                            {
                                currentHeight = value;
                            }
                        }
                        else
                        {
                            appendError(errors, itemPath + ".h", "expected number");
                        }
                    }
                    continue;
                }

                QString label{};
                if (item.isString())
                {
                    label = item.toString();
                }
                else if (item.isArray())
                {
                    auto labelArray{item.toArray()};
                    int labelUnwrapDepth{0};
                    while (labelArray.size() == 1 && labelArray.at(0).isArray())
                    {
                        labelArray = labelArray.at(0).toArray();
                        ++labelUnwrapDepth;
                        if (labelUnwrapDepth > 8)
                        {
                            appendError(errors, itemPath, "label nesting too deep");
                            labelArray = QJsonArray{};
                            break;
                        }
                    }
                    if (labelArray.isEmpty())
                    {
                        appendError(errors, itemPath, "label array is empty");
                        continue;
                    }
                    if (isLabelArray(labelArray))
                    {
                        label = joinLabelArray(labelArray);
                    }
                    else
                    {
                        QStringList parts{};
                        bool hasLabel{false};
                        for (int labelIndex{0}; labelIndex < labelArray.size(); ++labelIndex)
                        {
                            const auto labelItem{labelArray.at(labelIndex)};
                            const auto labelPath{QString("%1[%2]").arg(itemPath).arg(labelIndex)};
                            appendLabelValue(labelItem, &parts, &hasLabel, errors, labelPath, 0);
                        }
                        if (!hasLabel)
                        {
                            appendError(errors, itemPath, "label array contains no strings");
                            continue;
                        }
                        label = parts.join(QLatin1Char{'\n'});
                    }
                }
                else
                {
                    appendError(errors, itemPath, "expected string label");
                    continue;
                }

                geometryKey key{};
                key.label = label;
                key.rect = QRectF{currentX, currentY, currentWidth, currentHeight};
                parsedKeys.push_back(key);

                rowHeight = std::max(rowHeight, currentHeight);
                currentX += currentWidth;
                currentWidth = 1.0;
                currentHeight = 1.0;
            }

            currentY += rowHeight;
        }

        if (parsedKeys.empty())
        {
            appendError(errors, "geometry", "KLE json contains no keys");
        }

        if (errors != nullptr && !errors->empty())
        {
            return false;
        }

        *outKeys = std::move(parsedKeys);
        return true;
    }

    bool parseMapping(const QByteArray &data, std::size_t keyCount, std::vector<mappingEntry> *outEntries,
                      std::vector<QString> *errors)
    {
        if (outEntries == nullptr)
        {
            appendError(errors, "mapping", "output is null");
            return false;
        }
        if (keyCount == 0)
        {
            appendError(errors, "mapping", "key count is zero");
            return false;
        }

        QJsonParseError parseError{};
        const auto doc{QJsonDocument::fromJson(data, &parseError)};
        if (doc.isNull() || !doc.isObject())
        {
            appendError(errors, "mapping", QString("invalid mapping json (%1@%2)").arg(parseError.errorString()).arg(parseError.offset));
            return false;
        }

        const auto root{doc.object()};
        const auto keysValue{root.value("keys")};
        if (!keysValue.isArray())
        {
            appendError(errors, "mapping", "mapping json must contain 'keys' array");
            return false;
        }

        auto keyArray{keysValue.toArray()};
        int unwrapDepth{0};
        while (keyArray.size() == 1 && keyArray.at(0).isArray())
        {
            keyArray = keyArray.at(0).toArray();
            ++unwrapDepth;
            if (unwrapDepth > 4)
            {
                appendError(errors, "mapping.keys", "nesting too deep");
                break;
            }
        }
        if (keyArray.size() != static_cast<int>(keyCount))
        {
            appendError(errors, "mapping.keys",
                        QString("expected %1 entries, got %2").arg(keyCount).arg(keyArray.size()));
        }

        struct entryState
        {
            bool hasEntry{};
            bool hasVirtualKey{};
            bool hasScanCode{};
            std::uint32_t virtualKey{};
            std::uint32_t scanCode{};
        };

        std::vector<entryState> entries{};
        entries.resize(keyCount);

        for (int entryIndex{0}; entryIndex < keyArray.size(); ++entryIndex)
        {
            const auto item{keyArray.at(entryIndex)};
            const auto entryPath{QString("mapping.keys[%1]").arg(entryIndex)};
            if (!item.isObject())
            {
                appendError(errors, entryPath, "expected object");
                continue;
            }

            const auto obj{item.toObject()};
            const auto indexValue{obj.value("index")};
            int index{-1};
            if (!tryParseIndex(indexValue, &index))
            {
                appendError(errors, entryPath + ".index", "expected integer");
                continue;
            }
            if (index < 0 || index >= static_cast<int>(keyCount))
            {
                appendError(errors, entryPath + ".index",
                            QString("out of range (index=%1 keys=%2)").arg(index).arg(keyCount));
                continue;
            }

            auto &entry{entries[static_cast<std::size_t>(index)]};
            if (entry.hasEntry)
            {
                appendError(errors, entryPath + ".index", "duplicate index");
                continue;
            }
            entry.hasEntry = true;

            if (!obj.contains("virtualKey"))
            {
                appendError(errors, entryPath + ".virtualKey", "missing");
            }
            else
            {
                std::uint32_t virtualKey{0};
                if (tryParseUnsigned(obj.value("virtualKey"), &virtualKey))
                {
                    entry.virtualKey = virtualKey;
                    entry.hasVirtualKey = true;
                }
                else
                {
                    appendError(errors, entryPath + ".virtualKey", "expected unsigned integer");
                }
            }

            if (!obj.contains("scanCode"))
            {
                appendError(errors, entryPath + ".scanCode", "missing");
            }
            else
            {
                std::uint32_t scanCode{0};
                if (tryParseUnsigned(obj.value("scanCode"), &scanCode))
                {
                    entry.scanCode = scanCode;
                    entry.hasScanCode = true;
                }
                else
                {
                    appendError(errors, entryPath + ".scanCode", "expected unsigned integer");
                }
            }
        }

        for (std::size_t index{0}; index < entries.size(); ++index)
        {
            const auto &entry{entries[index]};
            const auto entryPath{QString("mapping.keys[%1]").arg(index)};
            if (!entry.hasEntry)
            {
                appendError(errors, entryPath + ".index", "missing entry");
                continue;
            }
            if (!entry.hasVirtualKey)
            {
                appendError(errors, entryPath + ".virtualKey", "missing");
            }
            if (!entry.hasScanCode)
            {
                appendError(errors, entryPath + ".scanCode", "missing");
            }
        }

        if (errors != nullptr && !errors->empty())
        {
            return false;
        }

        std::vector<mappingEntry> parsedEntries{};
        parsedEntries.resize(entries.size());
        for (std::size_t index{0}; index < entries.size(); ++index)
        {
            parsedEntries[index].virtualKey = entries[index].virtualKey;
            parsedEntries[index].scanCode = entries[index].scanCode;
        }

        *outEntries = std::move(parsedEntries);
        return true;
    }

} // namespace layoutParser
