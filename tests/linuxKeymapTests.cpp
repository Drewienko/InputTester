#include <vector>

#include <QDir>
#include <QFile>
#include <QString>
#include <QtTest/QTest>

#include "linuxKeymapParser.h"

namespace
{

    bool containsError(const std::vector<QString> &errors, const QString &needle)
    {
        for (const auto &error : errors)
        {
            if (error.contains(needle))
            {
                return true;
            }
        }
        return false;
    }

} // namespace

class linuxKeymapTests final : public QObject
{
    Q_OBJECT

private slots:
    void linuxKeymapJsonIsValid();
    void linuxKeymapJsonRejectsInvalid();
};

void linuxKeymapTests::linuxKeymapJsonIsValid()
{
    const QString root{QString::fromUtf8(INPUTTESTER_SOURCE_DIR)};
    const auto path{QDir{root}.filePath("resources/linux_keymap.json")};

    QFile file{path};
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(QString("unable to open %1").arg(path)));

    std::vector<QString> errors{};
    linuxKeymapParser::linuxKeyMap map{};
    const bool ok{linuxKeymapParser::parseLinuxKeyMap(file.readAll(), &map, &errors)};
    QVERIFY2(ok, qPrintable(linuxKeymapParser::formatErrors(errors)));
}

void linuxKeymapTests::linuxKeymapJsonRejectsInvalid()
{
    const QByteArray invalidData{
        R"json({ "nativeScanCodeOffset": 8, "qtKeyToVirtualKey": [], "linuxScanToWinScan": [] })json"};

    std::vector<QString> errors{};
    linuxKeymapParser::linuxKeyMap map{};
    const bool ok{linuxKeymapParser::parseLinuxKeyMap(invalidData, &map, &errors)};
    QVERIFY(!ok);
    QVERIFY(!errors.empty());
    QVERIFY(containsError(errors, "qtKeyToVirtualKey"));
    QVERIFY(containsError(errors, "linuxScanToWinScan"));
}

QTEST_MAIN(linuxKeymapTests)

#include "linuxKeymapTests.moc"
