#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <unordered_map>
#include <vector>

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QString>

#include "inputtester/core/inputEvent.h"
#include "inputtester/platform/inputBackend.h"

namespace inputTester
{

namespace
{

inputEvent makeKeyEvent(const RAWKEYBOARD& keyboard, std::uint32_t deviceId)
{
    inputEvent keyEvent{};
    keyEvent.timestampNs = nowTimestampNs();
    keyEvent.deviceId = deviceId;
    keyEvent.device = deviceType::keyboard;
    keyEvent.kind = (keyboard.Flags & RI_KEY_BREAK) ? eventKind::keyUp : eventKind::keyDown;
    keyEvent.virtualKey = static_cast<std::uint32_t>(keyboard.VKey);
    keyEvent.scanCode = static_cast<std::uint32_t>(keyboard.MakeCode);
    keyEvent.repeatCount = static_cast<std::uint16_t>(keyEvent.kind == eventKind::keyDown ? 1 : 0);
    keyEvent.isExtended = (keyboard.Flags & RI_KEY_E0) != 0 || (keyboard.Flags & RI_KEY_E1) != 0;
    return keyEvent;
}

char32_t normalizeChar(wchar_t ch)
{
    if (ch == L'\r')
    {
        return U'\n';
    }
    return static_cast<char32_t>(ch);
}

} // namespace

class winInputBackend final : public inputBackend, public QAbstractNativeEventFilter
{
public:
    winInputBackend() = default;

    bool start(QObject* eventSource, QString* errorMessage) override
    {
        Q_UNUSED(eventSource)
        stop();
        auto* app{ QCoreApplication::instance() };
        if (app != nullptr)
        {
            app->installNativeEventFilter(this);
        }
        else
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = "windows backend: QCoreApplication instance missing";
            }
            return false;
        }

        if (!registerRawInputDevices(errorMessage))
        {
            app->removeNativeEventFilter(this);
            return false;
        }
        return true;
    }

    void stop() override
    {
        auto* app{ QCoreApplication::instance() };
        if (app != nullptr)
        {
            app->removeNativeEventFilter(this);
        }
    }

    void setSink(inputEventSink* sink) override
    {
        sink_ = sink;
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
    {
        if (sink_ == nullptr)
        {
            return false;
        }

        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        {
            return false;
        }

        const auto* msg{ static_cast<const MSG*>(message) };
        if (msg == nullptr)
        {
            return false;
        }

        switch (msg->message)
        {
        case WM_INPUT:
            handleRawInput(msg->lParam);
            break;
        case WM_CHAR:
        {
            inputEvent textEvent{};
            textEvent.timestampNs = nowTimestampNs();
            textEvent.device = deviceType::keyboard;
            textEvent.kind = eventKind::keyDown;
            textEvent.isTextEvent = true;
            textEvent.text = normalizeChar(static_cast<wchar_t>(msg->wParam));
            sink_->onInputEvent(textEvent);
            if (result != nullptr)
            {
                *result = 0;
            }
            break;
        }
        default:
            break;
        }

        return false;
    }

private:
    bool registerRawInputDevices(QString* errorMessage)
    {
        RAWINPUTDEVICE devices[1]{};
        devices[0].usUsagePage = 0x01;
        devices[0].usUsage = 0x06;
        devices[0].dwFlags = 0;
        devices[0].hwndTarget = nullptr;
        if (!RegisterRawInputDevices(devices, 1, sizeof(RAWINPUTDEVICE)))
        {
            if (errorMessage != nullptr)
            {
                const auto errorCode{ static_cast<unsigned long>(GetLastError()) };
                *errorMessage = QString("windows backend: RegisterRawInputDevices failed (error=%1)").arg(errorCode);
            }
            return false;
        }
        return true;
    }

    void handleRawInput(LPARAM lParam)
    {
        UINT size{};
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) !=
            0)
        {
            return;
        }
        if (size == 0)
        {
            return;
        }

        if (rawBuffer_.size() < size)
        {
            rawBuffer_.resize(size);
        }
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawBuffer_.data(), &size,
                            sizeof(RAWINPUTHEADER)) != size)
        {
            return;
        }

        const auto* raw{ reinterpret_cast<const RAWINPUT*>(rawBuffer_.data()) };
        if (raw == nullptr || raw->header.dwType != RIM_TYPEKEYBOARD)
        {
            return;
        }

        const auto deviceId{ getDeviceId(raw->header.hDevice) };
        const auto keyEvent{ makeKeyEvent(raw->data.keyboard, deviceId) };
        sink_->onInputEvent(keyEvent);
    }

    std::uint32_t getDeviceId(HANDLE deviceHandle)
    {
        const auto it{ deviceIds_.find(deviceHandle) };
        if (it != deviceIds_.end())
        {
            return it->second;
        }
        const auto nextId{ nextDeviceId_ };
        ++nextDeviceId_;
        deviceIds_.emplace(deviceHandle, nextId);
        return nextId;
    }

    inputEventSink* sink_{};
    std::vector<std::uint8_t> rawBuffer_{};
    std::unordered_map<HANDLE, std::uint32_t> deviceIds_{};
    std::uint32_t nextDeviceId_{ 1 };
};

std::unique_ptr<inputBackend> createInputBackend()
{
    return std::make_unique<winInputBackend>();
}

} // namespace inputTester
