#include "linuxKeymapParser.h"

#include <cmath>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>

namespace LinuxKeymapParser
{

namespace
{

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

void appendError(std::vector<QString>* errors, const QString& path, const QString& message)
{
    if (errors == nullptr)
    {
        return;
    }
    errors->push_back(QString("%1: %2").arg(path, message));
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

bool tryParseQtKey(const QJsonValue& value, const QMetaEnum& metaEnum, int* out)
{
    if (out == nullptr)
    {
        return false;
    }
    if (value.isDouble())
    {
        const auto number{ value.toDouble() };
        if (std::floor(number) != number)
        {
            return false;
        }
        *out = static_cast<int>(number);
        return true;
    }
    if (value.isString())
    {
        const auto name{ value.toString().toLatin1() };
        const auto key{ metaEnum.keyToValue(name.constData()) };
        if (key == -1)
        {
            return false;
        }
        *out = key;
        return true;
    }
    return false;
}

bool parseNativeScanCodeOffset(const QJsonObject& root, LinuxKeyMap& result, std::vector<QString>* errors)
{
    if (!root.contains("nativeScanCodeOffset"))
    {
        appendError(errors, "nativeScanCodeOffset", "missing entry");
        return false;
    }
    std::uint32_t offset{};
    if (!tryParseUnsigned(root.value("nativeScanCodeOffset"), &offset))
    {
        appendError(errors, "nativeScanCodeOffset", "expected unsigned integer");
        return false;
    }
    result.nativeScanCodeOffset = offset;
    return true;
}

bool parseQtKeyMapping(const QJsonObject& root, LinuxKeyMap& result, std::vector<QString>* errors)
{
    const auto qtKeyArrayValue{ root.value("qtKeyToVirtualKey") };
    if (!qtKeyArrayValue.isArray())
    {
        appendError(errors, "qtKeyToVirtualKey", QString("missing array (got %1)").arg(valueTypeName(qtKeyArrayValue)));
        return false;
    }

    auto qtKeyArray{ qtKeyArrayValue.toArray() };
    if (qtKeyArray.size() == 1 && qtKeyArray.at(0).isArray())
    {
        qtKeyArray = qtKeyArray.at(0).toArray();
    }

    bool ok{ true };
    const auto keyEnum{ QMetaEnum::fromType<Qt::Key>() };

    for (int index{ 0 }; index < qtKeyArray.size(); ++index)
    {
        const auto item{ qtKeyArray.at(index) };
        const auto itemPath{ QString("qtKeyToVirtualKey[%1]").arg(index) };
        if (!item.isObject())
        {
            appendError(errors, itemPath, QString("expected object (got %1)").arg(valueTypeName(item)));
            ok = false;
            continue;
        }

        const auto obj{ item.toObject() };
        int qtKey{};
        if (!tryParseQtKey(obj.value("qtKey"), keyEnum, &qtKey))
        {
            appendError(errors, itemPath + ".qtKey", "expected Qt::Key");
            ok = false;
            continue;
        }

        std::uint32_t virtualKey{};
        if (!tryParseUnsigned(obj.value("virtualKey"), &virtualKey))
        {
            appendError(errors, itemPath + ".virtualKey", "expected unsigned integer");
            ok = false;
            continue;
        }

        bool keypad{ false };
        if (obj.contains("keypad"))
        {
            const auto keypadValue{ obj.value("keypad") };
            if (!keypadValue.isBool())
            {
                appendError(errors, itemPath + ".keypad", "expected boolean");
                ok = false;
            }
            else
            {
                keypad = keypadValue.toBool();
            }
        }

        const auto mapKey{ makeQtKeyMapKey(qtKey, keypad) };
        if (result.qtKeyToVirtualKey.find(mapKey) != result.qtKeyToVirtualKey.end())
        {
            appendError(errors, itemPath, "duplicate entry");
            ok = false;
            continue;
        }
        result.qtKeyToVirtualKey.emplace(mapKey, virtualKey);
    }
    return ok;
}

bool parseScanCodeMapping(const QJsonObject& root, LinuxKeyMap& result, std::vector<QString>* errors)
{
    const auto scanArrayValue{ root.value("linuxScanToWinScan") };
    if (!scanArrayValue.isArray())
    {
        appendError(errors, "linuxScanToWinScan", QString("missing array (got %1)").arg(valueTypeName(scanArrayValue)));
        return false;
    }

    auto scanArray{ scanArrayValue.toArray() };
    if (scanArray.size() == 1 && scanArray.at(0).isArray())
    {
        scanArray = scanArray.at(0).toArray();
    }

    bool ok{ true };
    for (int index{ 0 }; index < scanArray.size(); ++index)
    {
        const auto item{ scanArray.at(index) };
        const auto itemPath{ QString("linuxScanToWinScan[%1]").arg(index) };
        if (!item.isObject())
        {
            appendError(errors, itemPath, QString("expected object (got %1)").arg(valueTypeName(item)));
            ok = false;
            continue;
        }

        const auto obj{ item.toObject() };
        std::uint32_t linuxScan{};
        if (!tryParseUnsigned(obj.value("linuxScanCode"), &linuxScan))
        {
            appendError(errors, itemPath + ".linuxScanCode", "expected unsigned integer");
            ok = false;
            continue;
        }

        std::uint32_t winScan{};
        if (!tryParseUnsigned(obj.value("winScanCode"), &winScan))
        {
            appendError(errors, itemPath + ".winScanCode", "expected unsigned integer");
            ok = false;
            continue;
        }

        bool isExtended{ false };
        if (obj.contains("extended"))
        {
            const auto extendedValue{ obj.value("extended") };
            if (!extendedValue.isBool())
            {
                appendError(errors, itemPath + ".extended", "expected boolean");
                ok = false;
            }
            else
            {
                isExtended = extendedValue.toBool();
            }
        }

        if (result.linuxScanToWinScan.find(linuxScan) != result.linuxScanToWinScan.end())
        {
            appendError(errors, itemPath, "duplicate entry");
            ok = false;
            continue;
        }

        result.linuxScanToWinScan.emplace(linuxScan, ScanTranslation{ winScan, isExtended });
    }
    return ok;
}

} // namespace

std::uint64_t makeQtKeyMapKey(int qtKey, bool keypad)
{
    const auto keyValue{ static_cast<std::uint64_t>(static_cast<std::uint32_t>(qtKey)) };
    return (keyValue << 1) | (keypad ? 1U : 0U);
}

QString formatErrors(const std::vector<QString>& errors)
{
    QString message{};
    for (const auto& error : errors)
    {
        if (!message.isEmpty())
        {
            message += QLatin1Char{ '\n' };
        }
        message += error;
    }
    return message;
}

bool parseLinuxKeyMap(const QByteArray& data, LinuxKeyMap* outMap, std::vector<QString>* errors)
{
    if (errors != nullptr)
    {
        errors->clear();
    }

    QJsonParseError parseError{};
    const auto doc{ QJsonDocument::fromJson(data, &parseError) };
    if (doc.isNull() || !doc.isObject())
    {
        appendError(errors, "json",
                    QString("invalid json (%1@%2)").arg(parseError.errorString()).arg(parseError.offset));
        return false;
    }

    const auto root{ doc.object() };
    LinuxKeyMap result{};
    bool ok{ true };

    if (!parseNativeScanCodeOffset(root, result, errors))
    {
        ok = false;
    }

    if (!parseQtKeyMapping(root, result, errors))
    {
        ok = false;
    }

    if (!parseScanCodeMapping(root, result, errors))
    {
        ok = false;
    }

    if (result.qtKeyToVirtualKey.empty())
    {
        appendError(errors, "qtKeyToVirtualKey", "empty mapping");
        ok = false;
    }
    if (result.linuxScanToWinScan.empty())
    {
        appendError(errors, "linuxScanToWinScan", "empty mapping");
        ok = false;
    }

    if (!ok)
    {
        return false;
    }

    if (outMap != nullptr)
    {
        *outMap = std::move(result);
    }
    return true;
}

} // namespace LinuxKeymapParser
