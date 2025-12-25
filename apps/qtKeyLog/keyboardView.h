#ifndef inputTesterAppsKeyboardViewH
#define inputTesterAppsKeyboardViewH

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <QRectF>
#include <QString>
#include <QWidget>

#include "inputtester/core/inputEvent.h"

class keyboardView final : public QWidget {
public:
    enum class keyIdMode {
        virtualKey,
        scanCode,
    };

    explicit keyboardView(QWidget* parent = nullptr);

    void setKeyIdMode(keyIdMode mode);
    keyIdMode getKeyIdMode() const;

    bool loadLayoutFromFiles(const QString& geometryPath, const QString& mappingPath, QString* errorMessage);

    void handleInputEvent(const inputTester::inputEvent& event);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct keyDefinition {
        QString label{};
        QRectF unitRect{};
        std::uint32_t virtualKey{};
        std::uint32_t scanCode{};
    };

    void recalculateBounds();
    void addKeyAt(qreal x, qreal y, qreal widthUnits, qreal heightUnits,
                  const QString& label, std::uint32_t virtualKey, std::uint32_t scanCode);
    bool loadKleGeometry(const QString& geometryPath, QString* errorMessage);
    bool applyMapping(const QString& mappingPath, QString* errorMessage);

    bool isPressed(const keyDefinition& key) const;
    std::uint32_t keyIdForEvent(const inputTester::inputEvent& event) const;

    std::vector<keyDefinition> keys{};
    std::unordered_set<std::uint32_t> pressedKeys{};

    keyIdMode mode{keyIdMode::scanCode};
    qreal totalWidthUnits{0.0};
    qreal totalHeightUnits{0.0};
};

#endif // inputTesterAppsKeyboardViewH
