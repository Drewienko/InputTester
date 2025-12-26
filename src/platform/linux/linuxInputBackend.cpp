#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QObject>
#include <QString>

#include "inputtester/core/inputEvent.h"
#include "inputtester/platform/inputBackend.h"
#include "linuxKeymapParser.h"

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

        bool loadLinuxKeyMap(linuxKeymapParser::linuxKeyMap *out, QString *errorMessage)
        {
            QFile file{":/inputtester/linux_keymap.json"};
            if (!file.open(QIODevice::ReadOnly))
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = "linux keymap: failed to open resource";
                }
                return false;
            }

            linuxKeymapParser::linuxKeyMap map{};
            std::vector<QString> errors{};
            const bool ok{linuxKeymapParser::parseLinuxKeyMap(file.readAll(), &map, &errors)};
            if (!ok)
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = linuxKeymapParser::formatErrors(errors);
                }
                return false;
            }

            if (out != nullptr)
            {
                *out = std::move(map);
            }
            return true;
        }

        std::uint32_t mapQtKeyToWinVirtualKey(const linuxKeymapParser::linuxKeyMap &map, int qtKey,
                                              Qt::KeyboardModifiers modifiers)
        {
            const bool isKeypad{(modifiers & Qt::KeypadModifier) != 0};
            const auto keypadKey{linuxKeymapParser::makeQtKeyMapKey(qtKey, isKeypad)};
            const auto keypadIt{map.qtKeyToVirtualKey.find(keypadKey)};
            if (keypadIt != map.qtKeyToVirtualKey.end())
            {
                return keypadIt->second;
            }
            if (isKeypad)
            {
                const auto fallbackKey{linuxKeymapParser::makeQtKeyMapKey(qtKey, false)};
                const auto fallbackIt{map.qtKeyToVirtualKey.find(fallbackKey)};
                if (fallbackIt != map.qtKeyToVirtualKey.end())
                {
                    return fallbackIt->second;
                }
            }
            return 0;
        }

        linuxKeymapParser::scanTranslation translateScanCode(const linuxKeymapParser::linuxKeyMap &map,
                                                             const QKeyEvent *event)
        {
            std::uint32_t nativeScan{static_cast<std::uint32_t>(event->nativeScanCode())};
            const auto offset{map.nativeScanCodeOffset};
            if (offset != 0 && nativeScan >= offset)
            {
                nativeScan -= offset;
            }

            const auto entry{map.linuxScanToWinScan.find(nativeScan)};
            if (entry != map.linuxScanToWinScan.end())
            {
                return entry->second;
            }

            return linuxKeymapParser::scanTranslation{nativeScan, false};
        }

        inputEvent makeTextKeyEvent(const linuxKeymapParser::linuxKeyMap &map, const QKeyEvent *event, eventKind kind)
        {
            inputEvent keyEvent{};
            keyEvent.timestampNs = nowTimestampNs();
            keyEvent.device = deviceType::keyboard;
            keyEvent.kind = kind;
            keyEvent.virtualKey = mapQtKeyToWinVirtualKey(map, event->key(), event->modifiers());
            const auto translated{translateScanCode(map, event)};
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

        bool start(QObject *eventSource, QString *errorMessage) override
        {
            stop();
            if (eventSource == nullptr)
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = "linux backend: event source is null";
                }
                return false;
            }

            linuxKeymapParser::linuxKeyMap map{};
            if (!loadLinuxKeyMap(&map, errorMessage))
            {
                return false;
            }

            keyMap_ = std::move(map);
            eventSource_ = eventSource;
            eventSource_->installEventFilter(this);
            isReady_ = true;
            return true;
        }

        void stop() override
        {
            if (eventSource_ != nullptr)
            {
                eventSource_->removeEventFilter(this);
                eventSource_ = nullptr;
            }
            isReady_ = false;
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
                    const auto input{makeTextKeyEvent(keyMap_, keyEvent, kind)};
                    sink_->onInputEvent(input);
                }
            }

            return QObject::eventFilter(watched, event);
        }

    private:
        QObject *eventSource_{};
        inputEventSink *sink_{};
        linuxKeymapParser::linuxKeyMap keyMap_{};
        bool isReady_{false};
    };

    std::unique_ptr<inputBackend> createInputBackend()
    {
        return std::make_unique<linuxInputBackend>();
    }

} // namespace inputTester
