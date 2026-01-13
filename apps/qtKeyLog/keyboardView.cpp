#include "keyboardView.h"

#include <algorithm>

#include <QFile>
#include <QMap>
#include <QPainter>
#include <QTransform>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "layoutParser.h"

namespace
{
constexpr int g_viewMinHeight{ 220 };
constexpr int g_hintWidth{ 900 };
constexpr int g_hintHeight{ 320 };
constexpr qreal g_keyCornerRadius{ 4.0 };
constexpr std::uint32_t g_extendedKeyOffset{ 256 };
constexpr qreal g_outlinePenWidth{ 1.2 };
constexpr qreal g_minFontSize{ 7.0 };
constexpr qreal g_minScaledFontSize{ 6.0 };
constexpr qreal g_twoLineFontFactor{ 0.22 };
constexpr qreal g_oneLineFontFactor{ 0.25 };
constexpr qreal g_smallFontScale{ 0.85 };

constexpr std::uint32_t g_vkBack{ 0x08 };
constexpr std::uint32_t g_vkTab{ 0x09 };
constexpr std::uint32_t g_vkReturn{ 0x0D };
constexpr std::uint32_t g_vkShift{ 0x10 };
constexpr std::uint32_t g_vkControl{ 0x11 };
constexpr std::uint32_t g_vkMenu{ 0x12 }; // ALT
constexpr std::uint32_t g_vkCapital{ 0x14 };
constexpr std::uint32_t g_vkEscape{ 0x1B };
constexpr std::uint32_t g_vkSpace{ 0x20 };
constexpr std::uint32_t g_vkLeft{ 0x25 };
constexpr std::uint32_t g_vkUp{ 0x26 };
constexpr std::uint32_t g_vkRight{ 0x27 };
constexpr std::uint32_t g_vkDown{ 0x28 };
constexpr std::uint32_t g_vkDelete{ 0x2E };
constexpr std::uint32_t g_vkLwin{ 0x5B };
constexpr std::uint32_t g_vkLshift{ 0xA0 };
constexpr std::uint32_t g_vkRshift{ 0xA1 };

std::uint32_t virtualKeyFromLabel(const QString& label)
{
    static const QMap<QString, std::uint32_t> specialKeys{
        { "Esc", g_vkEscape },   { "Escape", g_vkEscape },   { "Tab", g_vkTab },        { "Caps Lock", g_vkCapital },
        { "Caps", g_vkCapital }, { "Shift", g_vkShift },     { "LShift", g_vkLshift },  { "RShift", g_vkRshift },
        { "Ctrl", g_vkControl }, { "Control", g_vkControl }, { "Alt", g_vkMenu },       { "Win", g_vkLwin },
        { "Cmd", g_vkLwin },     { "Super", g_vkLwin },      { "Space", g_vkSpace },    { "", g_vkSpace },
        { "Enter", g_vkReturn }, { "Return", g_vkReturn },   { "Backspace", g_vkBack }, { "Bksp", g_vkBack },
        { "Del", g_vkDelete },   { "Delete", g_vkDelete },   { "Up", g_vkUp },          { "Down", g_vkDown },
        { "Left", g_vkLeft },    { "Right", g_vkRight }
    };

    if (specialKeys.contains(label))
    {
        return specialKeys.value(label);
    }

    const QStringList parts{ label.split('\n') };
    for (const auto& part : parts)
    {
        if (part.isEmpty())
        {
            continue;
        }

        if (specialKeys.contains(part))
        {
            return specialKeys.value(part);
        }

        if (part.size() == 1)
        {
#ifdef _WIN32
            const char ch{ part.at(0).toLatin1() };
            const short vkResult{ VkKeyScanA(ch) };
            if (vkResult != -1)
            {
                return static_cast<std::uint32_t>(vkResult & 0xFF);
            }
#else
            const QChar ch{ part.at(0) };
            if (ch.isLetterOrNumber())
            {
                return ch.toUpper().unicode();
            }
#endif
        }
    }

    return 0; // Unknown
}

QString joinErrors(const std::vector<QString>& errors)
{
    QString combined{};
    for (const auto& error : errors)
    {
        if (!combined.isEmpty())
        {
            combined += QLatin1Char{ '\n' };
        }
        combined += error;
    }
    return combined;
}
} // namespace

KeyboardView::KeyboardView(QWidget* parent) : QWidget{ parent }
{
    setMinimumHeight(g_viewMinHeight);
}

void KeyboardView::setKeyIdMode(KeyIdMode newMode)
{
    if (m_mode == newMode)
    {
        return;
    }
    m_mode = newMode;
    m_pressedKeys.clear();
    update();
}

KeyboardView::KeyIdMode KeyboardView::getKeyIdMode() const
{
    return m_mode;
}

void KeyboardView::resetPressedKeys()
{
    m_pressedKeys.clear();
    update();
}

void KeyboardView::resetTestedKeys()
{
    m_testedKeys.clear();
    update();
}

std::size_t KeyboardView::getPressedKeyCount() const
{
    return m_pressedKeys.size();
}

void KeyboardView::handleInputEvent(const inputTester::inputEvent& event)
{
    if (event.device != inputTester::deviceType::keyboard)
    {
        return;
    }
    if (event.isTextEvent)
    {
        return;
    }
    const auto keyId{ keyIdForEvent(event) };
    if (keyId == 0)
    {
        return;
    }

    if (event.kind == inputTester::eventKind::keyDown)
    {
        m_pressedKeys.insert(keyId);
        m_testedKeys.insert(keyId);
    }
    else if (event.kind == inputTester::eventKind::keyUp)
    {
        m_pressedKeys.erase(keyId);
    }
    update();
}

QSize KeyboardView::sizeHint() const
{
    return QSize{ g_hintWidth, g_hintHeight };
}

void KeyboardView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter{ this };
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor backgroundColor{ 35, 38, 40 };
    painter.fillRect(rect(), backgroundColor);

    if (m_keys.empty() || m_sceneRect.isEmpty())
    {
        return;
    }

    constexpr qreal padding{ 12.0 };

    const qreal availableWidth{ width() - 2.0 * padding };
    const qreal availableHeight{ height() - 2.0 * padding };

    const qreal scaleX{ availableWidth / m_sceneRect.width() };
    const qreal scaleY{ availableHeight / m_sceneRect.height() };
    const qreal scale{ std::min(scaleX, scaleY) };

    const qreal drawnWidth{ m_sceneRect.width() * scale };
    const qreal drawnHeight{ m_sceneRect.height() * scale };

    const qreal startX{ padding + (availableWidth - drawnWidth) / 2.0 };
    const qreal startY{ padding + (availableHeight - drawnHeight) / 2.0 };

    const qreal offsetX{ startX - m_sceneRect.x() * scale };
    const qreal offsetY{ startY - m_sceneRect.y() * scale };

    const qreal gap{ std::max<qreal>(1.5, scale * 0.04) };
    const qreal frameInset{ std::max<qreal>(2.0, scale * 0.03) };
    const qreal faceInset{ std::max<qreal>(3.0, scale * 0.05) };

    const QColor frameColor{ 55, 58, 60 };
    const QColor faceColor{ 25, 28, 30 };
    const QColor framePressed{ 139, 0, 0 };
    const QColor facePressed{ 178, 34, 34 };
    const QColor frameTested{ 40, 60, 60 };
    const QColor faceTested{ 30, 50, 50 };
    const QColor outline{ 160, 150, 130 };
    const QColor labelColor{ 240, 240, 240 };

    for (const auto& key : m_keys)
    {
        painter.save();

        const qreal rxScreen{ offsetX + key.rx * scale };
        const qreal ryScreen{ offsetY + key.ry * scale };

        painter.translate(rxScreen, ryScreen);
        painter.rotate(key.rotation);

        QRectF keyRect{};
        keyRect.setX((key.unitRect.x() - key.rx) * scale);
        keyRect.setY((key.unitRect.y() - key.ry) * scale);
        keyRect.setWidth(key.unitRect.width() * scale);
        keyRect.setHeight(key.unitRect.height() * scale);

        keyRect = keyRect.adjusted(gap, gap, -gap, -gap);

        const bool pressed{ isPressed(key) };
        const auto keyId{ m_mode == KeyIdMode::virtualKey ? key.virtualKey : key.scanCode };
        const bool tested{ !pressed && m_testedKeys.contains(keyId) };

        const qreal radius{ std::min<qreal>(g_keyCornerRadius, std::min(keyRect.width(), keyRect.height()) * 0.18) };

        painter.setPen(QPen{ outline, g_outlinePenWidth });
        painter.setBrush(pressed ? framePressed : (tested ? frameTested : frameColor));
        painter.drawRoundedRect(keyRect, radius, radius);

        QRectF face{ keyRect.adjusted(faceInset, faceInset, -faceInset, -faceInset) };
        const qreal faceRadius{ std::max<qreal>(0.0, radius - 2.0) };

        painter.setPen(Qt::NoPen);
        painter.setBrush(pressed ? facePressed : (tested ? faceTested : faceColor));
        painter.drawRoundedRect(face, faceRadius, faceRadius);

        painter.setPen(labelColor);

        const QStringList lines{ key.label.split('\n') };
        QRectF inner{ face.adjusted(frameInset, frameInset, -frameInset, -frameInset) };

        if (lines.size() == 2)
        {
            QFont base{ painter.font() };
            base.setPointSizeF(std::max<qreal>(g_minFontSize, inner.height() * g_twoLineFontFactor));

            QFont smallFont{ base };
            smallFont.setPointSizeF(base.pointSizeF() * g_smallFontScale);

            painter.setFont(smallFont);
            painter.drawText(inner, Qt::AlignLeft | Qt::AlignTop, lines[0]);

            painter.setFont(base);
            painter.drawText(inner, Qt::AlignLeft | Qt::AlignBottom, lines[1]);
        }
        else
        {
            QFont f{ painter.font() };
            qreal fontSize{ std::max<qreal>(g_minFontSize, inner.height() * g_oneLineFontFactor) };
            f.setPointSizeF(fontSize);

            const QFontMetricsF fm{ f };
            const qreal textWidth{ fm.horizontalAdvance(key.label) };
            const qreal maxWidth{ inner.width() * 0.9 };

            if (textWidth > maxWidth && textWidth > 0.0)
            {
                fontSize = std::max<qreal>(g_minScaledFontSize, fontSize * (maxWidth / textWidth));
                f.setPointSizeF(fontSize);
            }

            painter.setFont(f);
            painter.drawText(inner, Qt::AlignCenter, key.label);
        }

        painter.restore();
    }
}

void KeyboardView::addKeyAt(qreal x, qreal y, qreal widthUnits, qreal heightUnits, const QString& label,
                            std::uint32_t virtualKey, std::uint32_t scanCode)
{
    KeyDefinition key{};
    key.label = label;
    key.unitRect = QRectF{ x, y, widthUnits, heightUnits };
    key.virtualKey = virtualKey;
    key.scanCode = scanCode;
    m_keys.push_back(key);
}

bool KeyboardView::loadLayoutFromFiles(const QString& geometryPath, const QString& mappingPath, QString* errorMessage)
{
    if (!loadKleGeometry(geometryPath, errorMessage))
    {
        return false;
    }

    if (!mappingPath.isEmpty())
    {
        if (!applyMapping(mappingPath, errorMessage))
        {
            return false;
        }
    }

    for (auto& key : m_keys)
    {
        if (key.virtualKey == 0)
        {
            key.virtualKey = virtualKeyFromLabel(key.label);
        }
#ifdef _WIN32
        if (key.scanCode == 0 && key.virtualKey != 0)
        {
            key.scanCode = static_cast<std::uint32_t>(MapVirtualKeyA(key.virtualKey, MAPVK_VK_TO_VSC));
        }
#endif
    }

    m_pressedKeys.clear();
    m_testedKeys.clear();
    update();
    return true;
}

bool KeyboardView::loadKleGeometry(const QString& geometryPath, QString* errorMessage)
{
    QFile file{ geometryPath };
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "geometry: unable to open geometry file";
        }
        return false;
    }

    const auto data{ file.readAll() };
    std::vector<LayoutParser::GeometryKey> parsedKeys{};
    std::vector<QString> errors{};
    if (!LayoutParser::parseKleGeometry(data, &parsedKeys, &errors))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = joinErrors(errors);
        }
        return false;
    }

    m_keys.clear();
    m_keys.reserve(parsedKeys.size());
    for (const auto& parsedKey : parsedKeys)
    {
        KeyDefinition key{};
        key.label = parsedKey.label;
        key.unitRect = parsedKey.rect;
        key.rotation = parsedKey.rotation;
        key.rx = parsedKey.rx;
        key.ry = parsedKey.ry;
        m_keys.push_back(key);
    }

    recalculateBounds();
    return true;
}

bool KeyboardView::applyMapping(const QString& mappingPath, QString* errorMessage)
{
    QFile file{ mappingPath };
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "mapping: unable to open mapping file";
        }
        return false;
    }

    const auto data{ file.readAll() };
    std::vector<LayoutParser::MappingEntry> entries{};
    std::vector<QString> errors{};
    if (!LayoutParser::parseMapping(data, m_keys.size(), &entries, &errors))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = joinErrors(errors);
        }
        return false;
    }

    for (std::size_t index{ 0 }; index < entries.size(); ++index)
    {
        m_keys[index].virtualKey = entries[index].virtualKey;
        m_keys[index].scanCode = entries[index].scanCode;
    }

    return true;
}

void KeyboardView::recalculateBounds()
{
    if (m_keys.empty())
    {
        m_sceneRect = QRectF{};
        return;
    }

    qreal minX{ std::numeric_limits<qreal>::max() };
    qreal minY{ std::numeric_limits<qreal>::max() };
    qreal maxX{ std::numeric_limits<qreal>::lowest() };
    qreal maxY{ std::numeric_limits<qreal>::lowest() };

    for (const auto& key : m_keys)
    {
        QTransform t{};
        t.translate(key.rx, key.ry);
        t.rotate(key.rotation);
        t.translate(-key.rx, -key.ry);

        const QRectF mappedRect{ t.mapRect(key.unitRect) };

        minX = std::min(minX, mappedRect.left());
        minY = std::min(minY, mappedRect.top());
        maxX = std::max(maxX, mappedRect.right());
        maxY = std::max(maxY, mappedRect.bottom());
    }
    m_sceneRect = QRectF(minX, minY, maxX - minX, maxY - minY);
}

bool KeyboardView::isPressed(const KeyDefinition& key) const
{
    const auto keyId{ m_mode == KeyIdMode::virtualKey ? key.virtualKey : key.scanCode };
    return m_pressedKeys.find(keyId) != m_pressedKeys.end();
}

std::uint32_t KeyboardView::keyIdForEvent(const inputTester::inputEvent& event) const
{
    if (m_mode == KeyIdMode::virtualKey)
    {
        return event.virtualKey;
    }
    if (event.isExtended)
    {
        return event.scanCode + g_extendedKeyOffset;
    }
    return event.scanCode;
}
