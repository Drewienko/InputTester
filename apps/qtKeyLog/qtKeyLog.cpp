#include <deque>
#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QString>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "inputtester/core/inputEvent.h"
#include "inputtester/core/inputEventQueue.h"
#include "inputtester/platform/inputBackend.h"
#include "keyboardView.h"

class KeyLogWindow final : public QWidget
{
public:
    KeyLogWindow()
        : m_textLabel{ new QPlainTextEdit{} }, m_modeCombo{ new QComboBox{} }, m_keyboard{ new KeyboardView{ this } },
          m_eventTimer{ new QTimer{ this } }
    {
        setWindowTitle("InputTester");
        resize(g_defaultWindowWidth, g_defaultWindowHeight);
        setFocusPolicy(Qt::StrongFocus);

        auto* layout{ new QVBoxLayout{ this } }; // NOLINT(cppcoreguidelines-owning-memory)
        auto* modeLayout{ new QHBoxLayout{} };   // NOLINT(cppcoreguidelines-owning-memory)
        auto* modeLabel{ new QLabel{ "key id mode:" } };

        m_modeCombo->addItem("virtualKey", static_cast<int>(KeyboardView::KeyIdMode::virtualKey));
        m_modeCombo->addItem("scanCode", static_cast<int>(KeyboardView::KeyIdMode::scanCode));
        m_loadButton = new QPushButton{ "load layout" };               // NOLINT(cppcoreguidelines-owning-memory)
        m_resetButton = new QPushButton{ "reset keys" };               // NOLINT(cppcoreguidelines-owning-memory)
        m_layoutStatus = new QLabel{ "layout: none (load KLE json)" }; // NOLINT(cppcoreguidelines-owning-memory)
        modeLayout->addWidget(modeLabel);
        modeLayout->addWidget(m_modeCombo);
        modeLayout->addWidget(m_loadButton);
        modeLayout->addWidget(m_resetButton);
        modeLayout->addWidget(m_layoutStatus);
        modeLayout->addStretch(1);

        m_statsLabel = new QLabel{ "NKRO: 0 | Rate: 0 Hz" }; // NOLINT(cppcoreguidelines-owning-memory)
        QFont statsFont{ m_statsLabel->font() };
        statsFont.setBold(true);
        m_statsLabel->setFont(statsFont);
        m_infoLabel =                                                      // NOLINT(cppcoreguidelines-owning-memory)
            new QLabel{ "state=none dev=0 vKey=0 scan=0 repeat=0 ext=0" }; // NOLINT(cppcoreguidelines-owning-memory)
        m_textLabel->setReadOnly(true);
        m_textLabel->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        m_textLabel->setWordWrapMode(QTextOption::WrapAnywhere);
        m_textLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_textLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_textLabel->setFixedHeight(g_textLabelHeight);
        m_textLabel->setMinimumWidth(0);

        layout->addLayout(modeLayout);
        layout->addWidget(m_statsLabel);
        layout->addWidget(m_infoLabel);
        layout->addWidget(m_textLabel);
        layout->addWidget(m_keyboard, 1);

        m_backend = inputTester::createInputBackend();
        m_backend->setSink(&m_eventQueue);
        QString backendError{};
        const bool backendStarted{ m_backend->start(this, &backendError) };

        m_eventTimer->setInterval(g_timerIntervalMs);
        QObject::connect(m_eventTimer, &QTimer::timeout, this, &KeyLogWindow::drainEvents);
        if (backendStarted)
        {
            m_eventTimer->start();
        }
        else
        {
            const auto message{ backendError.isEmpty() ? QString{ "input backend failed to start" } : backendError };
            m_infoLabel->setText(QString("input: %1").arg(message));
            QMessageBox::critical(this, "Input backend error", message);
        }

        QObject::connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                         [this](int index)
                         {
                             const auto data{ m_modeCombo->itemData(index) };
                             if (!data.isValid())
                             {
                                 return;
                             }
                             const auto modeValue{ data.toInt() };
                             m_keyboard->setKeyIdMode(static_cast<KeyboardView::KeyIdMode>(modeValue));
                             QSettings settings{};
                             settings.setValue("ui/keyIdMode", modeValue);
                         });
        QObject::connect(m_modeCombo, QOverload<int>::of(&QComboBox::activated), m_modeCombo, &QComboBox::hidePopup);
        QObject::connect(m_resetButton, &QPushButton::clicked, this,
                         [this]()
                         {
                             m_keyboard->resetPressedKeys();
                             m_keyboard->resetTestedKeys();
                             m_currentMaxKeys = 0;
                         });

        QObject::connect(
            m_loadButton, &QPushButton::clicked, this,
            [this]()
            {
                const QString initialPath{ QDir{ QCoreApplication::applicationDirPath() }.filePath("layouts") };

                const auto geometryPath{ QFileDialog::getOpenFileName(this, "Open KLE layout", initialPath,
                                                                      "KLE JSON (*.json)") };
                if (geometryPath.isEmpty())
                {
                    return;
                }
                const auto mappingPath{ QFileDialog::getOpenFileName(this, "Open mapping file (Cancel for Auto-Map)",
                                                                     QFileInfo{ geometryPath }.absolutePath(),
                                                                     "Mapping JSON (*.json)") };

                QString errorMessage{};
                if (!m_keyboard->loadLayoutFromFiles(geometryPath, mappingPath, &errorMessage))
                {
                    QMessageBox::warning(this, "Layout load failed", errorMessage);
                    return;
                }

                const QFileInfo info{ geometryPath };
                m_layoutStatus->setText(QString("layout: %1").arg(info.fileName()));
            });

        static constexpr KeyboardView::KeyIdMode defaultKeyMode{ KeyboardView::KeyIdMode::virtualKey };
        QSettings settings{};
        int defaultIndex{ 0 };
        for (int index{ 0 }; index < m_modeCombo->count(); ++index)
        {
            if (m_modeCombo->itemData(index).toInt() == static_cast<int>(defaultKeyMode))
            {
                defaultIndex = index;
                break;
            }
        }
        const auto storedMode{ settings.value("ui/keyIdMode") };
        if (storedMode.isValid())
        {
            const auto storedValue{ storedMode.toInt() };
            for (int index{ 0 }; index < m_modeCombo->count(); ++index)
            {
                if (m_modeCombo->itemData(index).toInt() == storedValue)
                {
                    defaultIndex = index;
                    break;
                }
            }
        }

        m_modeCombo->blockSignals(true);
        m_modeCombo->setCurrentIndex(defaultIndex);
        m_modeCombo->blockSignals(false);
        m_keyboard->setKeyIdMode(static_cast<KeyboardView::KeyIdMode>(m_modeCombo->itemData(defaultIndex).toInt()));
        QTimer::singleShot(0, this, [this]() { loadDefaultLayout(); });
    }

    ~KeyLogWindow() override
    {
        if (m_backend)
        {
            m_backend->stop();
        }
    }

private:
    static constexpr int g_defaultWindowWidth{ 980 };
    static constexpr int g_defaultWindowHeight{ 520 };
    static constexpr int g_timerIntervalMs{ 16 };
    static constexpr int g_textBufferLimit{ 100 };
    static constexpr int g_textLabelHeight{ 64 };
    static constexpr std::size_t g_timestampBufferSize{ 32 };
    static constexpr double g_nanosecondsPerSecond{ 1'000'000'000.0 };

    void drainEvents()
    {
        inputTester::inputEvent event{};
        while (m_eventQueue.tryPop(event))
        {
            if (!event.isTextEvent)
            {
                updateInfo(event);
            }
            if (event.kind == inputTester::eventKind::keyDown && event.text != U'\0')
            {
                handleText(event.text);
            }

            if (event.device == inputTester::deviceType::keyboard)
            {
                m_timestampBuffer.push_back(event.timestampNs);
                if (m_timestampBuffer.size() > g_timestampBufferSize)
                {
                    m_timestampBuffer.pop_front();
                }
            }

            m_keyboard->handleInputEvent(event);

            const auto currentKeys{ m_keyboard->getPressedKeyCount() };
            if (currentKeys > m_currentMaxKeys)
            {
                m_currentMaxKeys = currentKeys;
            }
        }

        if (!m_timestampBuffer.empty())
        {
            updateStats();
        }
    }

    void updateInfo(const inputTester::inputEvent& event)
    {
        const auto state{ event.kind == inputTester::eventKind::keyDown ? "down" : "up" };
        m_infoLabel->setText(QString("state=%1 dev=%2 vKey=%3 scan=%4 repeat=%5 ext=%6")
                                 .arg(state)
                                 .arg(event.deviceId)
                                 .arg(event.virtualKey)
                                 .arg(event.scanCode)
                                 .arg(event.repeatCount)
                                 .arg(event.isExtended ? 1 : 0));
    }

    void updateStats()
    {
        int hz{ 0 };
        if (m_timestampBuffer.size() >= 2)
        {
            const auto duration{ m_timestampBuffer.back() - m_timestampBuffer.front() };
            const auto count{ m_timestampBuffer.size() - 1 };
            if (duration > 0)
            {
                hz = static_cast<int>(g_nanosecondsPerSecond * static_cast<double>(count) /
                                      static_cast<double>(duration));
            }
        }
        m_statsLabel->setText(QString("NKRO: %1 (Max) | Rate: %2 Hz").arg(m_currentMaxKeys).arg(hz));
    }

    void handleText(char32_t text)
    {
        if (text == U'\b')
        {
            if (!m_textBuffer.isEmpty())
            {
                m_textBuffer.chop(1);
            }
        }
        else if (text == U'\n')
        {
            m_textBuffer += QLatin1Char{ '\n' };
        }
        else
        {
            m_textBuffer += QString::fromUcs4(&text, 1);
        }
        if (m_textBuffer.size() > g_textBufferLimit)
        {
            m_textBuffer = m_textBuffer.right(g_textBufferLimit);
        }
        m_textLabel->setPlainText(m_textBuffer);
    }

    void loadDefaultLayout()
    {
        const QDir appDir{ QCoreApplication::applicationDirPath() };
        const auto geometryPath{ appDir.filePath("layouts/ansi_full/ansi_full_kle.json") };
        const auto mappingPath{ appDir.filePath("layouts/ansi_full/ansi_full_mapping.json") };
        if (!QFileInfo::exists(geometryPath) || !QFileInfo::exists(mappingPath))
        {
            m_layoutStatus->setText("layout: default not found");
            return;
        }

        QString errorMessage{};
        if (!m_keyboard->loadLayoutFromFiles(geometryPath, mappingPath, &errorMessage))
        {
            m_layoutStatus->setText(QString("layout: default failed (%1)").arg(errorMessage));
            qWarning().noquote() << "Default layout failed:" << errorMessage << "geometry:" << geometryPath
                                 << "mapping:" << mappingPath;
            return;
        }

        m_layoutStatus->setText("layout: ansi_full (default)");
    }

    QLabel* m_statsLabel{};
    QLabel* m_infoLabel{};
    QPlainTextEdit* m_textLabel{};
    QComboBox* m_modeCombo{};
    KeyboardView* m_keyboard{};
    QPushButton* m_loadButton{};
    QPushButton* m_resetButton{};
    QLabel* m_layoutStatus{};
    QString m_textBuffer;
    QTimer* m_eventTimer{};
    inputTester::inputEventQueue m_eventQueue{};
    std::unique_ptr<inputTester::inputBackend> m_backend;
    std::size_t m_currentMaxKeys{ 0 };
    std::deque<std::uint64_t> m_timestampBuffer;
};

int main(int argc, char** argv)
{
    QApplication app{ argc, argv };
    QCoreApplication::setOrganizationName("InputTester");
    QCoreApplication::setApplicationName("InputTester");
    QApplication::setWindowIcon(QIcon{ ":/inputtester/icons/InputTester.png" });

    KeyLogWindow window{};
    window.show();

    return QApplication::exec();
}
