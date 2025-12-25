#include "keyboardView.h"

#include <algorithm>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

keyboardView::keyboardView(QWidget* parent) : QWidget{parent} {
    setMinimumHeight(220);
}

void keyboardView::setKeyIdMode(keyIdMode newMode) {
    if (mode == newMode) {
        return;
    }
    mode = newMode;
    pressedKeys.clear();
    update();
}

keyboardView::keyIdMode keyboardView::getKeyIdMode() const {
    return mode;
}

void keyboardView::handleInputEvent(const inputTester::inputEvent& event) {
    if (event.device != inputTester::deviceType::keyboard) {
        return;
    }
    if (event.isTextEvent) {
        return;
    }
    const auto keyId{keyIdForEvent(event)};
    if (keyId == 0) {
        return;
    }

    if (event.kind == inputTester::eventKind::keyDown) {
        pressedKeys.insert(keyId);
    } else if (event.kind == inputTester::eventKind::keyUp) {
        pressedKeys.erase(keyId);
    }
    update();
}

QSize keyboardView::sizeHint() const {
    return QSize{900, 320};
}

void keyboardView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter{this};
    painter.fillRect(rect(), Qt::white);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (keys.empty()) {
        return;
    }

    const qreal padding{12.0};
    const qreal availableWidth{width() - 2.0 * padding};
    const qreal availableHeight{height() - 2.0 * padding};
    const qreal scaleX{availableWidth / totalWidthUnits};
    const qreal scaleY{availableHeight / totalHeightUnits};
    const qreal scale{std::min(scaleX, scaleY)};
    const qreal offsetX{(width() - totalWidthUnits * scale) / 2.0};
    const qreal offsetY{(height() - totalHeightUnits * scale) / 2.0};

    for (const auto& key : keys) {
        QRectF keyRect{};
        keyRect.setX(offsetX + key.unitRect.x() * scale);
        keyRect.setY(offsetY + key.unitRect.y() * scale);
        keyRect.setWidth(key.unitRect.width() * scale);
        keyRect.setHeight(key.unitRect.height() * scale);

        const bool pressed{isPressed(key)};
        painter.setPen(Qt::black);
        painter.setBrush(pressed ? Qt::black : Qt::white);
        painter.drawRoundedRect(keyRect, 4.0, 4.0);

        painter.setPen(pressed ? Qt::white : Qt::black);
        painter.drawText(keyRect, Qt::AlignCenter, key.label);
    }
}

void keyboardView::addKeyAt(qreal x, qreal y, qreal widthUnits, qreal heightUnits, const QString& label,
                            std::uint32_t virtualKey, std::uint32_t scanCode) {
    keyDefinition key{};
    key.label = label;
    key.unitRect = QRectF{x, y, widthUnits, heightUnits};
    key.virtualKey = virtualKey;
    key.scanCode = scanCode;
    keys.push_back(key);
}

bool keyboardView::loadLayoutFromFiles(const QString& geometryPath, const QString& mappingPath,
                                       QString* errorMessage) {
    if (!loadKleGeometry(geometryPath, errorMessage)) {
        return false;
    }
    if (!applyMapping(mappingPath, errorMessage)) {
        return false;
    }
    pressedKeys.clear();
    update();
    return true;
}

bool keyboardView::loadKleGeometry(const QString& geometryPath, QString* errorMessage) {
    QFile file{geometryPath};
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = "unable to open geometry file";
        }
        return false;
    }

    const auto data{file.readAll()};
    QJsonParseError parseError{};
    const auto doc{QJsonDocument::fromJson(data, &parseError)};
    if (doc.isNull() || !doc.isArray()) {
        if (errorMessage != nullptr) {
            *errorMessage = "invalid KLE json";
        }
        return false;
    }

    keys.clear();

    qreal currentY{0.0};
    const auto rows{doc.array()};
    for (const auto& rowValue : rows) {
        if (!rowValue.isArray()) {
            continue;
        }

        const auto row{rowValue.toArray()};
        qreal currentX{0.0};
        qreal currentWidth{1.0};
        qreal currentHeight{1.0};
        qreal rowHeight{1.0};

        for (const auto& item : row) {
            if (item.isObject()) {
                const auto obj{item.toObject()};
                if (obj.contains("x")) {
                    currentX += obj.value("x").toDouble();
                }
                if (obj.contains("y")) {
                    currentY += obj.value("y").toDouble();
                }
                if (obj.contains("w")) {
                    currentWidth = obj.value("w").toDouble();
                }
                if (obj.contains("h")) {
                    currentHeight = obj.value("h").toDouble();
                }
                continue;
            }

            if (!item.isString()) {
                continue;
            }

            const auto label{item.toString()};
            addKeyAt(currentX, currentY, currentWidth, currentHeight, label, 0, 0);
            rowHeight = std::max(rowHeight, currentHeight);
            currentX += currentWidth;
            currentWidth = 1.0;
            currentHeight = 1.0;
        }

        currentY += rowHeight;
    }

    recalculateBounds();
    return true;
}

bool keyboardView::applyMapping(const QString& mappingPath, QString* errorMessage) {
    QFile file{mappingPath};
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = "unable to open mapping file";
        }
        return false;
    }

    const auto data{file.readAll()};
    QJsonParseError parseError{};
    const auto doc{QJsonDocument::fromJson(data, &parseError)};
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = "invalid mapping json";
        }
        return false;
    }

    const auto root{doc.object()};
    const auto keysValue{root.value("keys")};
    if (!keysValue.isArray()) {
        if (errorMessage != nullptr) {
            *errorMessage = "mapping json must contain 'keys' array";
        }
        return false;
    }

    const auto keyArray{keysValue.toArray()};
    bool hasAnyMapping{false};
    for (const auto& item : keyArray) {
        if (!item.isObject()) {
            continue;
        }

        const auto obj{item.toObject()};
        const auto indexValue{obj.value("index")};
        if (!indexValue.isDouble()) {
            continue;
        }

        const auto index{indexValue.toInt(-1)};
        if (index < 0 || index >= static_cast<int>(keys.size())) {
            if (errorMessage != nullptr) {
                *errorMessage = "mapping index out of range";
            }
            return false;
        }

        if (obj.contains("virtualKey") && obj.value("virtualKey").isDouble()) {
            keys[static_cast<std::size_t>(index)].virtualKey =
                static_cast<std::uint32_t>(obj.value("virtualKey").toInt());
            hasAnyMapping = true;
        }
        if (obj.contains("scanCode") && obj.value("scanCode").isDouble()) {
            keys[static_cast<std::size_t>(index)].scanCode =
                static_cast<std::uint32_t>(obj.value("scanCode").toInt());
            hasAnyMapping = true;
        }
    }

    if (!hasAnyMapping) {
        if (errorMessage != nullptr) {
            *errorMessage = "mapping json has no key codes";
        }
        return false;
    }

    return true;
}

void keyboardView::recalculateBounds() {
    totalWidthUnits = 0.0;
    totalHeightUnits = 0.0;
    for (const auto& key : keys) {
        totalWidthUnits = std::max(totalWidthUnits, key.unitRect.x() + key.unitRect.width());
        totalHeightUnits = std::max(totalHeightUnits, key.unitRect.y() + key.unitRect.height());
    }
}

bool keyboardView::isPressed(const keyDefinition& key) const {
    const auto keyId{mode == keyIdMode::virtualKey ? key.virtualKey : key.scanCode};
    return pressedKeys.find(keyId) != pressedKeys.end();
}

std::uint32_t keyboardView::keyIdForEvent(const inputTester::inputEvent& event) const {
    if (mode == keyIdMode::virtualKey) {
        return event.virtualKey;
    }
    if (event.isExtended) {
        return event.scanCode + 256;
    }
    return event.scanCode;
}
