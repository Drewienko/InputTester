#include <cmath>
#include <unordered_map>
#include <vector>

#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMetaEnum>
#include <QObject>
#include <QString>

#include "inputtester/core/inputEvent.h"
#include "inputtester/platform/inputBackend.h"

namespace inputTester
{

    namespace
    {

        char32_t firstCodepoint(const QString &text)
        {
            if (text.isEmpty())
            {
                return U'\0';
            }
            const auto ucs4{text.toUcs4()};
            if (ucs4.isEmpty())
            {
                return U'\0';
            }
            return static_cast<char32_t>(ucs4.constFirst());
        }

        struct scanTranslation
        {
            std::uint32_t scanCode{};
            bool isExtended{};
        };

        struct linuxKeyMap
        {
            std::unordered_map<std::uint64_t, std::uint32_t> qtKeyToVirtualKey{};
            std::unordered_map<std::uint32_t, scanTranslation> linuxScanToWinScan{};
        };

        std::uint64_t makeQtKeyMapKey(int qtKey, bool keypad)
        {
            const auto keyValue{static_cast<std::uint64_t>(static_cast<std::uint32_t>(qtKey))};
            return (keyValue << 1) | (keypad ? 1u : 0u);
        }

        void appendError(std::vector<QString> *errors, const QString &path, const QString &message)
        {
            if (errors == nullptr)
            {
                return;
            }
            errors->push_back(QString("%1: %2").arg(path, message));
        }

        QString joinErrors(const std::vector<QString> &errors)
        {
            QString message{};
            for (const auto &error : errors)
            {
                if (!message.isEmpty())
                {
                    message += QLatin1Char{'\n'};
                }
                message += error;
            }
            return message;
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

        bool tryParseQtKey(const QJsonValue &value, const QMetaEnum &metaEnum, int *out)
        {
            if (out == nullptr)
            {
                return false;
            }
            if (value.isDouble())
            {
                const auto number{value.toDouble()};
                if (std::floor(number) != number)
                {
                    return false;
                }
                *out = static_cast<int>(number);
                return true;
            }
            if (value.isString())
            {
                const auto name{value.toString().toLatin1()};
                const auto key{metaEnum.keyToValue(name.constData())};
                if (key == -1)
                {
                    return false;
                }
                *out = key;
                return true;
            }
            return false;
        }

        linuxKeyMap loadLinuxKeyMap()
        {
            linuxKeyMap result{};
            QFile file{":/inputtester/linux_keymap.json"};
            if (!file.open(QIODevice::ReadOnly))
            {
                qWarning().noquote() << "linux keymap: failed to open resource";
                return result;
            }

            QJsonParseError parseError{};
            const auto doc{QJsonDocument::fromJson(file.readAll(), &parseError)};
            if (doc.isNull() || !doc.isObject())
            {
                qWarning().noquote()
                    << QString("linux keymap: invalid json (%1@%2)")
                           .arg(parseError.errorString())
                           .arg(parseError.offset);
                return result;
            }

            const auto root{doc.object()};
            const auto keyEnum{QMetaEnum::fromType<Qt::Key>()};
            std::vector<QString> errors{};

            const auto qtKeyArrayValue{root.value("qtKeyToVirtualKey")};
            if (!qtKeyArrayValue.isArray())
            {
                appendError(&errors, "qtKeyToVirtualKey", "missing array");
            }
            else
            {
                const auto qtKeyArray{qtKeyArrayValue.toArray()};
                for (int index{0}; index < qtKeyArray.size(); ++index)
                {
                    const auto item{qtKeyArray.at(index)};
                    const auto itemPath{QString("qtKeyToVirtualKey[%1]").arg(index)};
                    if (!item.isObject())
                    {
                        appendError(&errors, itemPath, "expected object");
                        continue;
                    }

                    const auto obj{item.toObject()};
                    int qtKey{};
                    if (!tryParseQtKey(obj.value("qtKey"), keyEnum, &qtKey))
                    {
                        appendError(&errors, itemPath + ".qtKey", "expected Qt::Key");
                        continue;
                    }

                    std::uint32_t virtualKey{};
                    if (!tryParseUnsigned(obj.value("virtualKey"), &virtualKey))
                    {
                        appendError(&errors, itemPath + ".virtualKey", "expected unsigned integer");
                        continue;
                    }

                    bool keypad{false};
                    if (obj.contains("keypad"))
                    {
                        const auto keypadValue{obj.value("keypad")};
                        if (!keypadValue.isBool())
                        {
                            appendError(&errors, itemPath + ".keypad", "expected boolean");
                        }
                        else
                        {
                            keypad = keypadValue.toBool();
                        }
                    }

                    const auto mapKey{makeQtKeyMapKey(qtKey, keypad)};
                    if (result.qtKeyToVirtualKey.find(mapKey) != result.qtKeyToVirtualKey.end())
                    {
                        appendError(&errors, itemPath, "duplicate entry");
                        continue;
                    }
                    result.qtKeyToVirtualKey.emplace(mapKey, virtualKey);
                }
            }

            const auto scanArrayValue{root.value("linuxScanToWinScan")};
            if (!scanArrayValue.isArray())
            {
                appendError(&errors, "linuxScanToWinScan", "missing array");
            }
            else
            {
                const auto scanArray{scanArrayValue.toArray()};
                for (int index{0}; index < scanArray.size(); ++index)
                {
                    const auto item{scanArray.at(index)};
                    const auto itemPath{QString("linuxScanToWinScan[%1]").arg(index)};
                    if (!item.isObject())
                    {
                        appendError(&errors, itemPath, "expected object");
                        continue;
                    }

                    const auto obj{item.toObject()};
                    std::uint32_t linuxScan{};
                    if (!tryParseUnsigned(obj.value("linuxScanCode"), &linuxScan))
                    {
                        appendError(&errors, itemPath + ".linuxScanCode", "expected unsigned integer");
                        continue;
                    }

                    std::uint32_t winScan{};
                    if (!tryParseUnsigned(obj.value("winScanCode"), &winScan))
                    {
                        appendError(&errors, itemPath + ".winScanCode", "expected unsigned integer");
                        continue;
                    }

                    bool isExtended{false};
                    if (obj.contains("extended"))
                    {
                        const auto extendedValue{obj.value("extended")};
                        if (!extendedValue.isBool())
                        {
                            appendError(&errors, itemPath + ".extended", "expected boolean");
                        }
                        else
                        {
                            isExtended = extendedValue.toBool();
                        }
                    }

                    if (result.linuxScanToWinScan.find(linuxScan) != result.linuxScanToWinScan.end())
                    {
                        appendError(&errors, itemPath, "duplicate entry");
                        continue;
                    }

                    result.linuxScanToWinScan.emplace(linuxScan, scanTranslation{winScan, isExtended});
                }
            }

            if (!errors.empty())
            {
                qWarning().noquote() << "linux keymap:" << joinErrors(errors);
            }

            if (result.qtKeyToVirtualKey.empty())
            {
                qWarning().noquote() << "linux keymap: qtKeyToVirtualKey is empty";
            }
            if (result.linuxScanToWinScan.empty())
            {
                qWarning().noquote() << "linux keymap: linuxScanToWinScan is empty";
            }

            return result;
        }

        const linuxKeyMap &linuxKeyMapData()
        {
            static const linuxKeyMap map{loadLinuxKeyMap()};
            return map;
        }

        std::uint32_t mapQtKeyToWinVirtualKey(int qtKey, Qt::KeyboardModifiers modifiers)
        {
            const auto &map{linuxKeyMapData()};
            const bool isKeypad{(modifiers & Qt::KeypadModifier) != 0};
            const auto keypadKey{makeQtKeyMapKey(qtKey, isKeypad)};
            const auto keypadIt{map.qtKeyToVirtualKey.find(keypadKey)};
            if (keypadIt != map.qtKeyToVirtualKey.end())
            {
                return keypadIt->second;
            }
            if (isKeypad)
            {
                const auto fallbackKey{makeQtKeyMapKey(qtKey, false)};
                const auto fallbackIt{map.qtKeyToVirtualKey.find(fallbackKey)};
                if (fallbackIt != map.qtKeyToVirtualKey.end())
                {
                    return fallbackIt->second;
                }
            }
            return 0;
        }

        scanTranslation translateScanCode(const QKeyEvent *event)
        {
            std::uint32_t nativeScan{static_cast<std::uint32_t>(event->nativeScanCode())};
            if (nativeScan >= 8)
            {
                nativeScan -= 8;
            }

            const auto &map{linuxKeyMapData()};
            const auto entry{map.linuxScanToWinScan.find(nativeScan)};
            if (entry != map.linuxScanToWinScan.end())
            {
                return entry->second;
            }

            return scanTranslation{nativeScan, false};
        }

        inputEvent makeTextKeyEvent(const QKeyEvent *event, eventKind kind)
        {
            inputEvent keyEvent{};
            keyEvent.timestampNs = nowTimestampNs();
            keyEvent.device = deviceType::keyboard;
            keyEvent.kind = kind;
            keyEvent.virtualKey = mapQtKeyToWinVirtualKey(event->key(), event->modifiers());
            const auto translated{translateScanCode(event)};
            keyEvent.scanCode = translated.scanCode;
            keyEvent.repeatCount = static_cast<std::uint16_t>(event->isAutoRepeat() ? 1 : 0);
            keyEvent.isExtended = translated.isExtended;

            if (kind == eventKind::keyDown)
            {
                if (event->key() == Qt::Key_Backspace)
                {
                    keyEvent.text = U'\b';
                }
                else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
                {
                    keyEvent.text = U'\n';
                }
                else
                {
                    keyEvent.text = firstCodepoint(event->text());
                }
            }

            return keyEvent;
        }

    } // namespace

    class linuxInputBackend final : public QObject, public inputBackend
    {
    public:
        linuxInputBackend() = default;

        void start(QObject *eventSource) override
        {
            stop();
            eventSource_ = eventSource;
            if (eventSource_ != nullptr)
            {
                eventSource_->installEventFilter(this);
            }
        }

        void stop() override
        {
            if (eventSource_ != nullptr)
            {
                eventSource_->removeEventFilter(this);
                eventSource_ = nullptr;
            }
        }

        void setSink(inputEventSink *sink) override
        {
            sink_ = sink;
        }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event == nullptr)
            {
                return QObject::eventFilter(watched, event);
            }

            if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
            {
                const auto *keyEvent{static_cast<const QKeyEvent *>(event)};
                const auto kind{event->type() == QEvent::KeyPress ? eventKind::keyDown : eventKind::keyUp};
                if (sink_ != nullptr)
                {
                    const auto input{makeTextKeyEvent(keyEvent, kind)};
                    sink_->onInputEvent(input);
                }
            }

            return QObject::eventFilter(watched, event);
        }

    private:
        QObject *eventSource_{};
        inputEventSink *sink_{};
    };

    std::unique_ptr<inputBackend> createInputBackend()
    {
        return std::make_unique<linuxInputBackend>();
    }

} // namespace inputTester
