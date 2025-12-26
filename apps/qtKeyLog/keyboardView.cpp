#include "keyboardView.h"

#include <algorithm>

#include <QFile>
#include <QPainter>

#include "layoutParser.h"

namespace
{
    QString joinErrors(const std::vector<QString> &errors)
    {
        QString combined{};
        for (const auto &error : errors)
        {
            if (!combined.isEmpty())
            {
                combined += QLatin1Char{'\n'};
            }
            combined += error;
        }
        return combined;
    }
} // namespace

keyboardView::keyboardView(QWidget *parent) : QWidget{parent}
{
    setMinimumHeight(220);
}

void keyboardView::setKeyIdMode(keyIdMode newMode)
{
    if (mode == newMode)
    {
        return;
    }
    mode = newMode;
    pressedKeys.clear();
    update();
}

keyboardView::keyIdMode keyboardView::getKeyIdMode() const
{
    return mode;
}

void keyboardView::resetPressedKeys()
{
    pressedKeys.clear();
    update();
}

void keyboardView::handleInputEvent(const inputTester::inputEvent &event)
{
    if (event.device != inputTester::deviceType::keyboard)
    {
        return;
    }
    if (event.isTextEvent)
    {
        return;
    }
    const auto keyId{keyIdForEvent(event)};
    if (keyId == 0)
    {
        return;
    }

    if (event.kind == inputTester::eventKind::keyDown)
    {
        pressedKeys.insert(keyId);
    }
    else if (event.kind == inputTester::eventKind::keyUp)
    {
        pressedKeys.erase(keyId);
    }
    update();
}

QSize keyboardView::sizeHint() const
{
    return QSize{900, 320};
}

void keyboardView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter{this};
    painter.fillRect(rect(), Qt::white);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (keys.empty())
    {
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

    for (const auto &key : keys)
    {
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

void keyboardView::addKeyAt(qreal x, qreal y, qreal widthUnits, qreal heightUnits, const QString &label,
                            std::uint32_t virtualKey, std::uint32_t scanCode)
{
    keyDefinition key{};
    key.label = label;
    key.unitRect = QRectF{x, y, widthUnits, heightUnits};
    key.virtualKey = virtualKey;
    key.scanCode = scanCode;
    keys.push_back(key);
}

bool keyboardView::loadLayoutFromFiles(const QString &geometryPath, const QString &mappingPath,
                                       QString *errorMessage)
{
    if (!loadKleGeometry(geometryPath, errorMessage))
    {
        return false;
    }
    if (!applyMapping(mappingPath, errorMessage))
    {
        return false;
    }
    pressedKeys.clear();
    update();
    return true;
}

bool keyboardView::loadKleGeometry(const QString &geometryPath, QString *errorMessage)
{
    QFile file{geometryPath};
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "geometry: unable to open geometry file";
        }
        return false;
    }

    const auto data{file.readAll()};
    std::vector<layoutParser::geometryKey> parsedKeys{};
    std::vector<QString> errors{};
    if (!layoutParser::parseKleGeometry(data, &parsedKeys, &errors))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = joinErrors(errors);
        }
        return false;
    }

    keys.clear();
    keys.reserve(parsedKeys.size());
    for (const auto &parsedKey : parsedKeys)
    {
        keyDefinition key{};
        key.label = parsedKey.label;
        key.unitRect = parsedKey.rect;
        keys.push_back(key);
    }

    recalculateBounds();
    return true;
}

bool keyboardView::applyMapping(const QString &mappingPath, QString *errorMessage)
{
    QFile file{mappingPath};
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "mapping: unable to open mapping file";
        }
        return false;
    }

    const auto data{file.readAll()};
    std::vector<layoutParser::mappingEntry> entries{};
    std::vector<QString> errors{};
    if (!layoutParser::parseMapping(data, keys.size(), &entries, &errors))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = joinErrors(errors);
        }
        return false;
    }

    for (std::size_t index{0}; index < entries.size(); ++index)
    {
        keys[index].virtualKey = entries[index].virtualKey;
        keys[index].scanCode = entries[index].scanCode;
    }

    return true;
}

void keyboardView::recalculateBounds()
{
    totalWidthUnits = 0.0;
    totalHeightUnits = 0.0;
    for (const auto &key : keys)
    {
        totalWidthUnits = std::max(totalWidthUnits, key.unitRect.x() + key.unitRect.width());
        totalHeightUnits = std::max(totalHeightUnits, key.unitRect.y() + key.unitRect.height());
    }
}

bool keyboardView::isPressed(const keyDefinition &key) const
{
    const auto keyId{mode == keyIdMode::virtualKey ? key.virtualKey : key.scanCode};
    return pressedKeys.find(keyId) != pressedKeys.end();
}

std::uint32_t keyboardView::keyIdForEvent(const inputTester::inputEvent &event) const
{
    if (mode == keyIdMode::virtualKey)
    {
        return event.virtualKey;
    }
    if (event.isExtended)
    {
        return event.scanCode + 256;
    }
    return event.scanCode;
}
