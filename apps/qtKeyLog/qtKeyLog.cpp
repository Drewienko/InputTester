#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "inputtester/core/inputEvent.h"
#include "inputtester/core/inputEventQueue.h"
#include "inputtester/platform/inputBackend.h"
#include "keyboardView.h"

class keyLogWindow final : public QWidget {
public:
    keyLogWindow() {
        setWindowTitle("InputTester");
        resize(980, 520);
        setFocusPolicy(Qt::StrongFocus);

        auto* layout = new QVBoxLayout{this};
        auto* modeLayout = new QHBoxLayout{};
        auto* modeLabel = new QLabel{"key id mode:"};
        modeCombo = new QComboBox{};
        modeCombo->addItem("virtualKey");
        modeCombo->addItem("scanCode");
        loadButton = new QPushButton{"load layout"};
        layoutStatus = new QLabel{"layout: none (load KLE json)"};
        modeLayout->addWidget(modeLabel);
        modeLayout->addWidget(modeCombo);
        modeLayout->addWidget(loadButton);
        modeLayout->addWidget(layoutStatus);
        modeLayout->addStretch(1);

        infoLabel = new QLabel{"state=none dev=0 vKey=0 scan=0 repeat=0 ext=0"};
        textLabel = new QLabel{};
        textLabel->setWordWrap(true);
        keyboard = new keyboardView{this};

        layout->addLayout(modeLayout);
        layout->addWidget(infoLabel);
        layout->addWidget(textLabel);
        layout->addWidget(keyboard, 1);

        backend = inputTester::createInputBackend();
        backend->setSink(&eventQueue);
        backend->start(this);

        eventTimer = new QTimer{this};
        eventTimer->setInterval(16);
        QObject::connect(eventTimer, &QTimer::timeout, this, &keyLogWindow::drainEvents);
        eventTimer->start();

        QObject::connect(modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            const auto mode{index == 0 ? keyboardView::keyIdMode::virtualKey
                                       : keyboardView::keyIdMode::scanCode};
            keyboard->setKeyIdMode(mode);
        });

        QObject::connect(loadButton, &QPushButton::clicked, this, [this]() {
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
            layoutStatus->setText(QString("layout: %1").arg(info.fileName()));
        });
    }

    ~keyLogWindow() override {
        if (backend) {
            backend->stop();
        }
    }

private:
    void drainEvents() {
        inputTester::inputEvent event{};
        while (eventQueue.tryPop(event)) {
            if (!event.isTextEvent) {
                updateInfo(event);
            }
            if (event.kind == inputTester::eventKind::keyDown && event.text != U'\0') {
                handleText(event.text);
            }
            keyboard->handleInputEvent(event);
        }
    }

    void updateInfo(const inputTester::inputEvent& event) {
        const auto state{event.kind == inputTester::eventKind::keyDown ? "down" : "up"};
        infoLabel->setText(QString("state=%1 dev=%2 vKey=%3 scan=%4 repeat=%5 ext=%6")
                               .arg(state)
                               .arg(event.deviceId)
                               .arg(event.virtualKey)
                               .arg(event.scanCode)
                               .arg(event.repeatCount)
                               .arg(event.isExtended ? 1 : 0));
    }

    void handleText(char32_t text) {
        if (text == U'\b') {
            if (!textBuffer.isEmpty()) {
                textBuffer.chop(1);
            }
        } else if (text == U'\n') {
            textBuffer += QLatin1Char{'\n'};
        } else {
            textBuffer += QString::fromUcs4(&text, 1);
        }
        textLabel->setText(textBuffer);
    }

    QLabel* infoLabel{};
    QLabel* textLabel{};
    QComboBox* modeCombo{};
    keyboardView* keyboard{};
    QPushButton* loadButton{};
    QLabel* layoutStatus{};
    QString textBuffer{};
    QTimer* eventTimer{};
    inputTester::inputEventQueue eventQueue{};
    std::unique_ptr<inputTester::inputBackend> backend{};
};

int main(int argc, char** argv) {
    QApplication app{argc, argv};

    keyLogWindow window{};
    window.show();

    return app.exec();
}
