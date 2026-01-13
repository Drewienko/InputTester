#ifndef inputTesterAppsKeyboardViewH
#define inputTesterAppsKeyboardViewH

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <QRectF>
#include <QString>
#include <QWidget>

#include "inputtester/core/inputEvent.h"

class KeyboardView final : public QWidget
{
public:
    enum class KeyIdMode
    {
        virtualKey,
        scanCode,
    };

    explicit KeyboardView(QWidget* parent = nullptr);

    void setKeyIdMode(KeyIdMode mode);
    KeyIdMode getKeyIdMode() const;
    void resetPressedKeys();
    void resetTestedKeys();
    std::size_t getPressedKeyCount() const;

    bool loadLayoutFromFiles(const QString& geometryPath, const QString& mappingPath, QString* errorMessage);

    void handleInputEvent(const inputTester::inputEvent& event);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct KeyDefinition
    {
        QString label;
        QRectF unitRect;
        std::uint32_t virtualKey{};
        std::uint32_t scanCode{};
        qreal rotation{};
        qreal rx{};
        qreal ry{};
    };

    void recalculateBounds();
    void addKeyAt(qreal x, qreal y, qreal widthUnits, qreal heightUnits, const QString& label, std::uint32_t virtualKey,
                  std::uint32_t scanCode);
    bool loadKleGeometry(const QString& geometryPath, QString* errorMessage);
    bool applyMapping(const QString& mappingPath, QString* errorMessage);

    bool isPressed(const KeyDefinition& key) const;
    std::uint32_t keyIdForEvent(const inputTester::inputEvent& event) const;
    std::vector<KeyDefinition> m_keys;
    std::unordered_set<std::uint32_t> m_pressedKeys;
    std::unordered_set<std::uint32_t> m_testedKeys;

    KeyIdMode m_mode{ KeyIdMode::virtualKey };
    QRectF m_sceneRect;
};

#endif // inputTesterAppsKeyboardViewH
