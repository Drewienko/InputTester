#include <QEvent>
#include <QKeyEvent>
#include <QObject>

#include "inputtester/core/inputEvent.h"
#include "inputtester/platform/inputBackend.h"

namespace inputTester {

namespace {

char32_t firstCodepoint(const QString& text) {
    if (text.isEmpty()) {
        return U'\0';
    }
    const auto ucs4{text.toUcs4()};
    if (ucs4.isEmpty()) {
        return U'\0';
    }
    return static_cast<char32_t>(ucs4.constFirst());
}

inputEvent makeTextKeyEvent(const QKeyEvent* event, eventKind kind) {
    inputEvent keyEvent{};
    keyEvent.timestampNs = nowTimestampNs();
    keyEvent.device = deviceType::keyboard;
    keyEvent.kind = kind;
    keyEvent.virtualKey = static_cast<std::uint32_t>(event->nativeVirtualKey());
    keyEvent.scanCode = static_cast<std::uint32_t>(event->nativeScanCode());
    keyEvent.repeatCount = static_cast<std::uint16_t>(event->isAutoRepeat() ? 1 : 0);
    keyEvent.isExtended = false;

    if (kind == eventKind::keyDown) {
        if (event->key() == Qt::Key_Backspace) {
            keyEvent.text = U'\b';
        } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            keyEvent.text = U'\n';
        } else {
            keyEvent.text = firstCodepoint(event->text());
        }
    }

    return keyEvent;
}

} // namespace

class linuxInputBackend final : public QObject, public inputBackend {
public:
    linuxInputBackend() = default;

    void start(QObject* eventSource) override {
        stop();
        eventSource_ = eventSource;
        if (eventSource_ != nullptr) {
            eventSource_->installEventFilter(this);
        }
    }

    void stop() override {
        if (eventSource_ != nullptr) {
            eventSource_->removeEventFilter(this);
            eventSource_ = nullptr;
        }
    }

    void setSink(inputEventSink* sink) override {
        sink_ = sink;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            const auto* keyEvent{static_cast<const QKeyEvent*>(event)};
            const auto kind{event->type() == QEvent::KeyPress ? eventKind::keyDown : eventKind::keyUp};
            if (sink_ != nullptr) {
                const auto input{makeTextKeyEvent(keyEvent, kind)};
                sink_->onInputEvent(input);
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QObject* eventSource_{};
    inputEventSink* sink_{};
};

std::unique_ptr<inputBackend> createInputBackend() {
    return std::make_unique<linuxInputBackend>();
}

} // namespace inputTester
