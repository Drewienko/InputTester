#include <vector>

#include <QFile>
#include <QRectF>
#include <QString>
#include <QDir>
#include <QtTest/QTest>

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

class layoutParserTests final : public QObject
{
  Q_OBJECT

private slots:
  void parseKleGeometryParsesMetaAndLabels();
  void parseKleGeometryParsesLabelArray();
  void parseKleGeometryFailsOnEmpty();
  void parseMappingParsesHexAndDecimal();
  void parseMappingFailsOnMissingEntries();
  void parseLayoutsFromDiskAnsiFull();
  void parseLayoutsFromDiskAnsiTkl();
};

void layoutParserTests::parseKleGeometryParsesMetaAndLabels()
{
  const QByteArray data{R"([
  { "name": "test" },
  [ "A", { "w": 2.0 }, "B" ],
  [ { "x": 0.5 }, "C\nD", { "h": 2.0 }, "E" ]
])"};

  std::vector<layoutParser::geometryKey> keys{};
  std::vector<QString> errors{};
  const bool ok{layoutParser::parseKleGeometry(data, &keys, &errors)};
  QVERIFY2(ok, qPrintable(joinErrors(errors)));
  QCOMPARE(keys.size(), static_cast<std::size_t>(4));
  QCOMPARE(keys[0].label, QString{"A"});
  QCOMPARE(keys[1].label, QString{"B"});
  QCOMPARE(keys[2].label, QString{"C\nD"});
  QCOMPARE(keys[3].label, QString{"E"});
  QCOMPARE(keys[0].rect, (QRectF{0.0, 0.0, 1.0, 1.0}));
  QCOMPARE(keys[1].rect, (QRectF{1.0, 0.0, 2.0, 1.0}));
  QCOMPARE(keys[2].rect, (QRectF{0.5, 1.0, 1.0, 1.0}));
  QCOMPARE(keys[3].rect, (QRectF{1.5, 1.0, 1.0, 2.0}));
}

void layoutParserTests::parseKleGeometryParsesLabelArray()
{
  const QByteArray data{R"([
  [ "A", [ "B", "C" ], "D" ]
])"};

  std::vector<layoutParser::geometryKey> keys{};
  std::vector<QString> errors{};
  const bool ok{layoutParser::parseKleGeometry(data, &keys, &errors)};
  QVERIFY2(ok, qPrintable(joinErrors(errors)));
  QCOMPARE(keys.size(), static_cast<std::size_t>(3));
  QCOMPARE(keys[0].label, QString{"A"});
  QCOMPARE(keys[1].label, QString{"B\nC"});
  QCOMPARE(keys[2].label, QString{"D"});
  QCOMPARE(keys[0].rect, (QRectF{0.0, 0.0, 1.0, 1.0}));
  QCOMPARE(keys[1].rect, (QRectF{1.0, 0.0, 1.0, 1.0}));
  QCOMPARE(keys[2].rect, (QRectF{2.0, 0.0, 1.0, 1.0}));
}

void layoutParserTests::parseKleGeometryFailsOnEmpty()
{
  const QByteArray data{"[]"};

  std::vector<layoutParser::geometryKey> keys{};
  std::vector<QString> errors{};
  const bool ok{layoutParser::parseKleGeometry(data, &keys, &errors)};
  QVERIFY(!ok);
  QVERIFY(!errors.empty());
}

void layoutParserTests::parseMappingParsesHexAndDecimal()
{
  const QByteArray data{R"({
  "keys": [
    { "index": 0, "virtualKey": "0x41", "scanCode": 30 },
    { "index": 1, "virtualKey": 66, "scanCode": "0x31" }
  ]
})"};

  std::vector<layoutParser::mappingEntry> entries{};
  std::vector<QString> errors{};
  const bool ok{layoutParser::parseMapping(data, 2, &entries, &errors)};
  QVERIFY(ok);
  QCOMPARE(entries.size(), static_cast<std::size_t>(2));
  QCOMPARE(entries[0].virtualKey, static_cast<std::uint32_t>(0x41));
  QCOMPARE(entries[0].scanCode, static_cast<std::uint32_t>(30));
  QCOMPARE(entries[1].virtualKey, static_cast<std::uint32_t>(66));
  QCOMPARE(entries[1].scanCode, static_cast<std::uint32_t>(0x31));
}

void layoutParserTests::parseMappingFailsOnMissingEntries()
{
  const QByteArray data{R"({
  "keys": [
    { "index": 0, "virtualKey": 65, "scanCode": 30 }
  ]
})"};

  std::vector<layoutParser::mappingEntry> entries{};
  std::vector<QString> errors{};
  const bool ok{layoutParser::parseMapping(data, 2, &entries, &errors)};
  QVERIFY(!ok);
  QVERIFY(!errors.empty());
}

void layoutParserTests::parseLayoutsFromDiskAnsiFull()
{
  const QString root{QString::fromUtf8(INPUTTESTER_SOURCE_DIR)};
  const auto geometryPath{QDir{root}.filePath("layouts/ansi_full/ansi_full_kle.json")};
  const auto mappingPath{QDir{root}.filePath("layouts/ansi_full/ansi_full_mapping.json")};

  QFile geometryFile{geometryPath};
  QVERIFY(geometryFile.open(QIODevice::ReadOnly));
  QFile mappingFile{mappingPath};
  QVERIFY(mappingFile.open(QIODevice::ReadOnly));

  std::vector<layoutParser::geometryKey> keys{};
  std::vector<QString> errors{};
  const auto geometryData{geometryFile.readAll()};
  const bool geometryOk{layoutParser::parseKleGeometry(geometryData, &keys, &errors)};
  QVERIFY2(geometryOk, qPrintable(joinErrors(errors)));
  QCOMPARE(keys.size(), static_cast<std::size_t>(104));

  std::vector<layoutParser::mappingEntry> mapping{};
  const auto mappingData{mappingFile.readAll()};
  const bool mappingOk{layoutParser::parseMapping(mappingData, keys.size(), &mapping, &errors)};
  QVERIFY2(mappingOk, qPrintable(joinErrors(errors)));
  QCOMPARE(mapping.size(), keys.size());
}

void layoutParserTests::parseLayoutsFromDiskAnsiTkl()
{
  const QString root{QString::fromUtf8(INPUTTESTER_SOURCE_DIR)};
  const auto geometryPath{QDir{root}.filePath("layouts/ansi_tkl/ansi_tkl_kle.json")};
  const auto mappingPath{QDir{root}.filePath("layouts/ansi_tkl/ansi_tkl_mapping.json")};

  QFile geometryFile{geometryPath};
  QVERIFY(geometryFile.open(QIODevice::ReadOnly));
  QFile mappingFile{mappingPath};
  QVERIFY(mappingFile.open(QIODevice::ReadOnly));

  std::vector<layoutParser::geometryKey> keys{};
  std::vector<QString> errors{};
  const auto geometryData{geometryFile.readAll()};
  const bool geometryOk{layoutParser::parseKleGeometry(geometryData, &keys, &errors)};
  QVERIFY2(geometryOk, qPrintable(joinErrors(errors)));
  QCOMPARE(keys.size(), static_cast<std::size_t>(87));

  std::vector<layoutParser::mappingEntry> mapping{};
  const auto mappingData{mappingFile.readAll()};
  const bool mappingOk{layoutParser::parseMapping(mappingData, keys.size(), &mapping, &errors)};
  QVERIFY2(mappingOk, qPrintable(joinErrors(errors)));
  QCOMPARE(mapping.size(), keys.size());
}

QTEST_MAIN(layoutParserTests)

#include "layoutParserTests.moc"
