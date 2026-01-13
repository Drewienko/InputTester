#include "layoutParser.h"

#include <algorithm>
#include <cmath>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace
{
void appendError(std::vector<QString>* errors, const QString& path, const QString& message)
{
    if (errors == nullptr)
    {
        return;
    }
    errors->push_back(QString("%1: %2").arg(path, message));
}

bool tryParseDouble(const QJsonValue& value, double* out)
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
        bool ok{ false };
        const auto parsed{ value.toString().toDouble(&ok) };
        if (!ok)
        {
            return false;
        }
        *out = parsed;
        return true;
    }
    return false;
}

bool tryParseUnsigned(const QJsonValue& value, std::uint32_t* out)
{
    if (out == nullptr)
    {
        return false;
    }
    if (value.isDouble())
    {
        const auto number{ value.toDouble() };
        if (number < 0.0 || std::floor(number) != number)
        {
            return false;
        }
        *out = static_cast<std::uint32_t>(number);
        return true;
    }
    if (value.isString())
    {
        bool ok{ false };
        const auto parsed{ value.toString().toUInt(&ok, 0) };
        if (!ok)
        {
            return false;
        }
        *out = parsed;
        return true;
    }
    return false;
}

bool tryParseIndex(const QJsonValue& value, int* out)
{
    if (out == nullptr)
    {
        return false;
    }
    if (value.isDouble())
    {
        const auto number{ value.toDouble() };
        if (number < 0.0 || std::floor(number) != number)
        {
            return false;
        }
        *out = static_cast<int>(number);
        return true;
    }
    if (value.isString())
    {
        bool ok{ false };
        const auto parsed{ value.toString().toInt(&ok, 0) };
        if (!ok || parsed < 0)
        {
            return false;
        }
        *out = parsed;
        return true;
    }
    return false;
}

bool isLabelArray(const QJsonArray& array)
{
    if (array.isEmpty())
    {
        return false;
    }
    return std::all_of(array.begin(), array.end(), [](const QJsonValue& item) { return item.isString(); });
}

QString valueTypeName(const QJsonValue& value)
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

QString joinLabelArray(const QJsonArray& labels)
{
    QStringList parts{};
    for (const auto& item : labels)
    {
        if (item.isString())
        {
            parts.push_back(item.toString());
        }
    }
    return parts.join(QLatin1Char{ '\n' });
}

void appendLabelValue(const QJsonValue& value, QStringList* parts, bool* hasLabel, std::vector<QString>* errors,
                      const QString& path, int depth)
{
    if (parts == nullptr || hasLabel == nullptr)
    {
        return;
    }
    int constexpr maxDepth{ 32 };
    if (depth > maxDepth)
    {
        appendError(errors, path, "label nesting too deep");
        return;
    }
    if (value.isArray())
    {
        const auto nested{ value.toArray() };
        if (nested.isEmpty())
        {
            appendError(errors, path, "label array is empty");
            return;
        }
        for (int nestedIndex{ 0 }; nestedIndex < nested.size(); ++nestedIndex)
        {
            const auto nestedItem{ nested.at(nestedIndex) };
            const auto nestedPath{ QString("%1[%2]").arg(path).arg(nestedIndex) };
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

struct KleState
{
    qreal rotation{ 0.0 };
    qreal rx{ 0.0 };
    qreal ry{ 0.0 };
    qreal x{ 0.0 };
    qreal y{ 0.0 };
    qreal width{ 1.0 };
    qreal height{ 1.0 };
    qreal rowHeight{ 1.0 };
    static constexpr double g_rotationEpsilon{ 0.0001 };
};

void updateRotationAndOrigin(const QJsonObject& obj, KleState& state, std::vector<QString>* errors, const QString& path)
{
    if (obj.contains("r"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("r"), &value))
        {
            state.rotation = value;
        }
        else
        {
            appendError(errors, path + ".r", "expected number");
        }
    }

    bool originChanged{ false };
    if (obj.contains("rx"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("rx"), &value))
        {
            state.rx = value;
            originChanged = true;
        }
        else
        {
            appendError(errors, path + ".rx", "expected number");
        }
    }
    if (obj.contains("ry"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("ry"), &value))
        {
            state.ry = value;
            originChanged = true;
        }
        else
        {
            appendError(errors, path + ".ry", "expected number");
        }
    }
    if (originChanged)
    {
        state.x = state.rx;
        state.y = state.ry;
    }
}

void updatePosition(const QJsonObject& obj, KleState& state, std::vector<QString>* errors, const QString& path)
{
    if (obj.contains("x"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("x"), &value))
        {
            state.x += value;
        }
        else
        {
            appendError(errors, path + ".x", "expected number");
        }
    }
    if (obj.contains("y"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("y"), &value))
        {
            state.y += value;
        }
        else
        {
            appendError(errors, path + ".y", "expected number");
        }
    }
}

void updateSize(const QJsonObject& obj, KleState& state, std::vector<QString>* errors, const QString& path)
{
    if (obj.contains("w"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("w"), &value))
        {
            if (value <= 0.0)
            {
                appendError(errors, path + ".w", "expected positive number");
            }
            else
            {
                state.width = value;
            }
        }
        else
        {
            appendError(errors, path + ".w", "expected number");
        }
    }
    if (obj.contains("h"))
    {
        double value{ 0.0 };
        if (tryParseDouble(obj.value("h"), &value))
        {
            if (value <= 0.0)
            {
                appendError(errors, path + ".h", "expected positive number");
            }
            else
            {
                state.height = value;
            }
        }
        else
        {
            appendError(errors, path + ".h", "expected number");
        }
    }
}

void updateStateFromObject(const QJsonObject& obj, KleState& state, std::vector<QString>* errors, const QString& path)
{
    updateRotationAndOrigin(obj, state, errors, path);
    updatePosition(obj, state, errors, path);
    updateSize(obj, state, errors, path);
}

bool parseKeyLabel(const QJsonValue& item, QString& label, std::vector<QString>* errors, const QString& path)
{
    if (item.isString())
    {
        label = item.toString();
        return true;
    }

    if (item.isArray())
    {
        auto labelArray{ item.toArray() };
        int labelUnwrapDepth{ 0 };
        while (labelArray.size() == 1 && labelArray.at(0).isArray())
        {
            labelArray = labelArray.at(0).toArray();
            ++labelUnwrapDepth;
            constexpr int gLabelUnwrapDepth{ 8 };
            if (labelUnwrapDepth > gLabelUnwrapDepth)
            {
                appendError(errors, path, "label nesting too deep");
                return false;
            }
        }
        if (labelArray.isEmpty())
        {
            appendError(errors, path, "label array is empty");
            return false;
        }
        if (isLabelArray(labelArray))
        {
            label = joinLabelArray(labelArray);
            return true;
        }

        QStringList parts{};
        bool hasLabel{ false };
        for (int labelIndex{ 0 }; labelIndex < labelArray.size(); ++labelIndex)
        {
            const auto labelItem{ labelArray.at(labelIndex) };
            const auto labelPath{ QString("%1[%2]").arg(path).arg(labelIndex) };
            appendLabelValue(labelItem, &parts, &hasLabel, errors, labelPath, 0);
        }
        if (!hasLabel)
        {
            appendError(errors, path, "label array contains no strings");
            return false;
        }
        label = parts.join(QLatin1Char{ '\n' });
        return true;
    }

    appendError(errors, path, "expected string label");
    return false;
}

struct EntryState
{
    bool hasEntry{};
    bool hasVirtualKey{};
    bool hasScanCode{};
    std::uint32_t virtualKey{};
    std::uint32_t scanCode{};
};

void parseMappingEntry(const QJsonValue& item, int entryIndex, std::size_t keyCount, std::vector<EntryState>& entries,
                       std::vector<QString>* errors)
{
    const auto entryPath{ QString("mapping.keys[%1]").arg(entryIndex) };
    if (!item.isObject())
    {
        appendError(errors, entryPath, "expected object");
        return;
    }

    const auto obj{ item.toObject() };
    const auto indexValue{ obj.value("index") };
    int index{ -1 };
    if (!tryParseIndex(indexValue, &index))
    {
        appendError(errors, entryPath + ".index", "expected integer");
        return;
    }
    if (index < 0 || index >= static_cast<int>(keyCount))
    {
        appendError(errors, entryPath + ".index", QString("out of range (index=%1 keys=%2)").arg(index).arg(keyCount));
        return;
    }

    auto& entry{ entries[static_cast<std::size_t>(index)] };
    if (entry.hasEntry)
    {
        appendError(errors, entryPath + ".index", "duplicate index");
        return;
    }
    entry.hasEntry = true;

    if (!obj.contains("virtualKey"))
    {
        appendError(errors, entryPath + ".virtualKey", "missing");
    }
    else
    {
        std::uint32_t virtualKey{ 0 };
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
        std::uint32_t scanCode{ 0 };
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

} // namespace

namespace LayoutParser
{

void parseKleRow(const QJsonValue& rowValue, int rowIndex, KleState& state, std::vector<GeometryKey>& parsedKeys,
                 std::vector<QString>* errors)
{
    const auto rowPath{ QString("geometry.rows[%1]").arg(rowIndex) };
    if (!rowValue.isArray())
    {
        appendError(errors, rowPath, "expected row array");
        return;
    }

    auto row{ rowValue.toArray() };
    int unwrapDepth{ 0 };
    while (row.size() == 1 && row.at(0).isArray() && !isLabelArray(row.at(0).toArray()))
    {
        row = row.at(0).toArray();
        ++unwrapDepth;
        if (unwrapDepth > 4)
        {
            appendError(errors, rowPath, "row nesting too deep");
            return;
        }
    }

    state.x = (std::abs(state.rotation) > KleState::g_rotationEpsilon) ? state.rx : 0.0;
    state.width = 1.0;
    state.height = 1.0;
    state.rowHeight = 1.0;

    for (int itemIndex{ 0 }; itemIndex < row.size(); ++itemIndex)
    {
        const auto item{ row.at(itemIndex) };
        const auto itemPath{ QString("%1[%2]").arg(rowPath).arg(itemIndex) };
        if (item.isObject())
        {
            updateStateFromObject(item.toObject(), state, errors, itemPath);
            continue;
        }

        QString label{};
        if (!parseKeyLabel(item, label, errors, itemPath))
        {
            continue;
        }

        GeometryKey key{};
        key.label = label;
        key.rect = QRectF{ state.x, state.y, state.width, state.height };
        key.rotation = state.rotation;
        key.rx = state.rx;
        key.ry = state.ry;
        parsedKeys.push_back(key);

        state.rowHeight = std::max(state.rowHeight, state.height);
        state.x += state.width;
        state.width = 1.0;
        state.height = 1.0;
    }

    state.y += 1.0;
}

bool parseKleGeometry(const QByteArray& data, std::vector<GeometryKey>* outKeys, std::vector<QString>* errors)
{
    if (outKeys == nullptr)
    {
        appendError(errors, "geometry", "output is null");
        return false;
    }

    QJsonParseError parseError{};
    const auto doc{ QJsonDocument::fromJson(data, &parseError) };
    if (doc.isNull() || !doc.isArray())
    {
        appendError(errors, "geometry",
                    QString("invalid KLE json (%1@%2)").arg(parseError.errorString()).arg(parseError.offset));
        return false;
    }

    auto root{ doc.array() };
    if (root.size() == 1 && root.at(0).isArray())
    {
        root = root.at(0).toArray();
    }

    int rowStart{ 0 };
    while (rowStart < root.size() && root.at(rowStart).isObject())
    {
        ++rowStart;
    }
    if (rowStart >= root.size())
    {
        appendError(errors, "geometry", "KLE json has no rows");
        return false;
    }

    std::vector<GeometryKey> parsedKeys{};
    KleState state{};
    int rowIndex{ 0 };

    for (int i{ 0 }; i < rowStart; ++i)
    {
        if (root.at(i).isObject())
        {
            updateStateFromObject(root.at(i).toObject(), state, errors, QString("geometry[%1]").arg(i));
        }
    }

    for (int index{ rowStart }; index < root.size(); ++index, ++rowIndex)
    {
        parseKleRow(root.at(index), rowIndex, state, parsedKeys, errors);
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

bool parseMapping(const QByteArray& data, std::size_t keyCount, std::vector<MappingEntry>* outEntries,
                  std::vector<QString>* errors)
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
    const auto doc{ QJsonDocument::fromJson(data, &parseError) };
    if (doc.isNull() || !doc.isObject())
    {
        appendError(errors, "mapping",
                    QString("invalid mapping json (%1@%2)").arg(parseError.errorString()).arg(parseError.offset));
        return false;
    }

    const auto root{ doc.object() };
    const auto keysValue{ root.value("keys") };
    if (!keysValue.isArray())
    {
        appendError(errors, "mapping", "mapping json must contain 'keys' array");
        return false;
    }

    auto keyArray{ keysValue.toArray() };
    int unwrapDepth{ 0 };
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
        appendError(errors, "mapping.keys", QString("expected %1 entries, got %2").arg(keyCount).arg(keyArray.size()));
    }

    std::vector<EntryState> entries{};
    entries.resize(keyCount);

    for (int entryIndex{ 0 }; entryIndex < keyArray.size(); ++entryIndex)
    {
        parseMappingEntry(keyArray.at(entryIndex), entryIndex, keyCount, entries, errors);
    }

    for (std::size_t index{ 0 }; index < entries.size(); ++index)
    {
        const auto& entry{ entries[index] };
        const auto entryPath{ QString("mapping.keys[%1]").arg(index) };
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

    std::vector<MappingEntry> parsedEntries{};
    parsedEntries.resize(entries.size());
    for (std::size_t index{ 0 }; index < entries.size(); ++index)
    {
        parsedEntries[index].virtualKey = entries[index].virtualKey;
        parsedEntries[index].scanCode = entries[index].scanCode;
    }

    *outEntries = std::move(parsedEntries);
    return true;
}

} // namespace LayoutParser
