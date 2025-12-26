#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QTextOption>
#include <QTimer>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "inputtester/core/inputEvent.h"
#include "inputtester/core/inputEventQueue.h"
#include "inputtester/platform/inputBackend.h"
#include "keyboardView.h"

class keyLogWindow final : public QWidget
{
public:
    keyLogWindow()
    {
        setWindowTitle("InputTester");
        resize(980, 520);
        setFocusPolicy(Qt::StrongFocus);

        auto *layout = new QVBoxLayout{this};
        auto *modeLayout = new QHBoxLayout{};
        auto *modeLabel = new QLabel{"key id mode:"};
        modeCombo = new QComboBox{};
        modeCombo->addItem("virtualKey", static_cast<int>(keyboardView::keyIdMode::virtualKey));
        modeCombo->addItem("scanCode", static_cast<int>(keyboardView::keyIdMode::scanCode));
        loadButton = new QPushButton{"load layout"};
        resetButton = new QPushButton{"reset keys"};
        layoutStatus = new QLabel{"layout: none (load KLE json)"};
        modeLayout->addWidget(modeLabel);
        modeLayout->addWidget(modeCombo);
        modeLayout->addWidget(loadButton);
        modeLayout->addWidget(resetButton);
        modeLayout->addWidget(layoutStatus);
        modeLayout->addStretch(1);

        infoLabel = new QLabel{"state=none dev=0 vKey=0 scan=0 repeat=0 ext=0"};
        textLabel = new QPlainTextEdit{};
        textLabel->setReadOnly(true);
        textLabel->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        textLabel->setWordWrapMode(QTextOption::WrapAnywhere);
        textLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        textLabel->setFixedHeight(textLabelHeight);
        textLabel->setMinimumWidth(0);
        keyboard = new keyboardView{this};

        layout->addLayout(modeLayout);
        layout->addWidget(infoLabel);
        layout->addWidget(textLabel);
        layout->addWidget(keyboard, 1);

        backend = inputTester::createInputBackend();
        backend->setSink(&eventQueue);
        QString backendError{};
        const bool backendStarted{backend->start(this, &backendError)};

        eventTimer = new QTimer{this};
        eventTimer->setInterval(16);
        QObject::connect(eventTimer, &QTimer::timeout, this, &keyLogWindow::drainEvents);
        if (backendStarted)
        {
            eventTimer->start();
        }
        else
        {
            const auto message{backendError.isEmpty() ? QString{"input backend failed to start"} : backendError};
            infoLabel->setText(QString("input: %1").arg(message));
            QMessageBox::critical(this, "Input backend error", message);
        }

        QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                         [this](int index)
                         {
                             const auto data{modeCombo->itemData(index)};
                             if (!data.isValid())
                             {
                                 return;
                             }
                             const auto modeValue{data.toInt()};
                             keyboard->setKeyIdMode(static_cast<keyboardView::keyIdMode>(modeValue));
                             QSettings settings{};
                             settings.setValue("ui/keyIdMode", modeValue);
                         });
        QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::activated), modeCombo, &QComboBox::hidePopup);
        QObject::connect(resetButton, &QPushButton::clicked, this,
                         [this]()
                         { keyboard->resetPressedKeys(); });

        QObject::connect(loadButton, &QPushButton::clicked, this, [this]()
                         {
            const auto geometryPath{QFileDialog::getOpenFileName(this, "Open KLE layout", QString{},
                                                                 "KLE JSON (*.json)")};
            if (geometryPath.isEmpty()) {
                return;
            }
            const auto mappingPath{QFileDialog::getOpenFileName(this, "Open mapping file", QString{},
                                                                "Mapping JSON (*.json)")};
            if (mappingPath.isEmpty()) {
                return;
            }

            QString errorMessage{};
            if (!keyboard->loadLayoutFromFiles(geometryPath, mappingPath, &errorMessage)) {
                QMessageBox::warning(this, "Layout load failed", errorMessage);
                return;
            }

            const QFileInfo info{geometryPath};
            layoutStatus->setText(QString("layout: %1").arg(info.fileName())); });

        static constexpr keyboardView::keyIdMode defaultKeyMode{
#if defined(_WIN32)
            keyboardView::keyIdMode::scanCode
#else
            keyboardView::keyIdMode::virtualKey
#endif
        };
        QSettings settings{};
        int defaultIndex{0};
        for (int index{0}; index < modeCombo->count(); ++index)
        {
            if (modeCombo->itemData(index).toInt() == static_cast<int>(defaultKeyMode))
            {
                defaultIndex = index;
                break;
            }
        }
        const auto storedMode{settings.value("ui/keyIdMode")};
        if (storedMode.isValid())
        {
            const auto storedValue{storedMode.toInt()};
            for (int index{0}; index < modeCombo->count(); ++index)
            {
                if (modeCombo->itemData(index).toInt() == storedValue)
                {
                    defaultIndex = index;
                    break;
                }
            }
        }

        modeCombo->blockSignals(true);
        modeCombo->setCurrentIndex(defaultIndex);
        modeCombo->blockSignals(false);
        keyboard->setKeyIdMode(static_cast<keyboardView::keyIdMode>(
            modeCombo->itemData(defaultIndex).toInt()));
        QTimer::singleShot(0, this, [this]()
                           { loadDefaultLayout(); });
    }

    ~keyLogWindow() override
    {
        if (backend)
        {
            backend->stop();
        }
    }

private:
    static constexpr int textBufferLimit{100};
    static constexpr int textLabelHeight{64};

    void drainEvents()
    {
        inputTester::inputEvent event{};
        while (eventQueue.tryPop(event))
        {
            if (!event.isTextEvent)
            {
                updateInfo(event);
            }
            if (event.kind == inputTester::eventKind::keyDown && event.text != U'\0')
            {
                handleText(event.text);
            }
            keyboard->handleInputEvent(event);
        }
    }

    void updateInfo(const inputTester::inputEvent &event)
    {
        const auto state{event.kind == inputTester::eventKind::keyDown ? "down" : "up"};
        infoLabel->setText(QString("state=%1 dev=%2 vKey=%3 scan=%4 repeat=%5 ext=%6")
                               .arg(state)
                               .arg(event.deviceId)
                               .arg(event.virtualKey)
                               .arg(event.scanCode)
                               .arg(event.repeatCount)
                               .arg(event.isExtended ? 1 : 0));
    }

    void handleText(char32_t text)
    {
        if (text == U'\b')
        {
            if (!textBuffer.isEmpty())
            {
                textBuffer.chop(1);
            }
        }
        else if (text == U'\n')
        {
            textBuffer += QLatin1Char{'\n'};
        }
        else
        {
            textBuffer += QString::fromUcs4(&text, 1);
        }
        if (textBuffer.size() > textBufferLimit)
        {
            textBuffer = textBuffer.right(textBufferLimit);
        }
        textLabel->setPlainText(textBuffer);
    }

    void loadDefaultLayout()
    {
        const auto geometryPath{findLayoutFile("layouts/ansi_full/ansi_full_kle.json")};
        const auto mappingPath{findLayoutFile("layouts/ansi_full/ansi_full_mapping.json")};
        if (geometryPath.isEmpty() || mappingPath.isEmpty())
        {
            layoutStatus->setText("layout: default not found");
            return;
        }

        QString errorMessage{};
        if (!keyboard->loadLayoutFromFiles(geometryPath, mappingPath, &errorMessage))
        {
            layoutStatus->setText(QString("layout: default failed (%1)").arg(errorMessage));
            qWarning().noquote() << "Default layout failed:" << errorMessage
                                 << "geometry:" << geometryPath << "mapping:" << mappingPath;
            return;
        }

        layoutStatus->setText("layout: ansi_full (default)");
    }

    QString findProjectRoot() const
    {
        static constexpr int maxDepth{6};
        const QStringList startPaths{QDir::currentPath(), QCoreApplication::applicationDirPath()};
        for (const auto &startPath : startPaths)
        {
            QDir dir{startPath};
            for (int depth{0}; depth < maxDepth; ++depth)
            {
                if (QFileInfo::exists(dir.filePath("CMakeLists.txt")) ||
                    QFileInfo::exists(dir.filePath(".git")))
                {
                    return dir.absolutePath();
                }
                if (!dir.cdUp())
                {
                    break;
                }
            }
        }
        return {};
    }

    QString findLayoutFile(const QString &relativePath) const
    {
        const auto projectRoot{findProjectRoot()};
        if (!projectRoot.isEmpty())
        {
            const auto candidate{QDir{projectRoot}.filePath(relativePath)};
            if (QFileInfo::exists(candidate))
            {
                return candidate;
            }
        }

        const auto currentCandidate{QDir{QDir::currentPath()}.filePath(relativePath)};
        if (QFileInfo::exists(currentCandidate))
        {
            return currentCandidate;
        }

        QDir searchDir{QCoreApplication::applicationDirPath()};
        for (int depth{0}; depth < 5; ++depth)
        {
            const auto candidate{searchDir.filePath(relativePath)};
            if (QFileInfo::exists(candidate))
            {
                return candidate;
            }
            if (!searchDir.cdUp())
            {
                break;
            }
        }
        return {};
    }

    QLabel *infoLabel{};
    QPlainTextEdit *textLabel{};
    QComboBox *modeCombo{};
    keyboardView *keyboard{};
    QPushButton *loadButton{};
    QPushButton *resetButton{};
    QLabel *layoutStatus{};
    QString textBuffer{};
    QTimer *eventTimer{};
    inputTester::inputEventQueue eventQueue{};
    std::unique_ptr<inputTester::inputBackend> backend{};
};

int main(int argc, char **argv)
{
    QApplication app{argc, argv};
    QCoreApplication::setOrganizationName("InputTester");
    QCoreApplication::setApplicationName("InputTester");
    app.setWindowIcon(QIcon{":/inputtester/icons/InputTester.png"});

    keyLogWindow window{};
    window.show();

    return app.exec();
}
