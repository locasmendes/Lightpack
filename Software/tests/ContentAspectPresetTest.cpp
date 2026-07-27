#include "ContentAspectPresetTest.hpp"
#include "wizard/ContentAspectPreset.hpp"
#include <QtTest>

ContentAspectPresetTest::ContentAspectPresetTest(QObject *parent) :
	QObject(parent)
{
}

void ContentAspectPresetTest::testFillReturnsMonitorUnchanged()
{
	const QRect monitor(0, 0, 3440, 1440);
	QCOMPARE(ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Fill), monitor);
}

void ContentAspectPresetTest::test16x9OnWiderMonitor()
{
	const QRect monitor(0, 0, 3440, 1440);
	const QRect result = ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Ratio16x9);
	QCOMPARE(result, QRect(440, 0, 2560, 1440));
}

void ContentAspectPresetTest::test4x3OnWiderMonitor()
{
	const QRect monitor(0, 0, 3440, 1440);
	const QRect result = ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Ratio4x3);
	QCOMPARE(result, QRect(760, 0, 1920, 1440));
}

void ContentAspectPresetTest::testExactRatioMatchIsUnchanged()
{
	const QRect monitor(0, 0, 1920, 1080); // exactly 16:9
	const QRect result = ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Ratio16x9);
	QCOMPARE(result, monitor);
}

void ContentAspectPresetTest::testNarrowerMonitorLetterboxes()
{
	const QRect monitor(100, 50, 1280, 1024); // non-zero origin, narrower than 16:9
	const QRect result = ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Ratio16x9);
	QCOMPARE(result, QRect(100, 202, 1280, 720));
}

void ContentAspectPresetTest::testNonZeroMonitorOrigin()
{
	// Second monitor to the right, as on a multi-monitor desktop.
	const QRect monitor(1920, 0, 3440, 1440);
	const QRect result = ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Ratio16x9);
	QCOMPARE(result, QRect(2360, 0, 2560, 1440));
}

void ContentAspectPresetTest::testRotatedResolutionStaysCenteredWithinOnePixel()
{
	const QRect monitor(0, 0, 1080, 1920); // portrait/rotated
	const QRect result = ContentAspectPreset::contentRect(monitor, ContentAspectPreset::Ratio16x9);

	QVERIFY(result.width() > 0);
	QVERIFY(result.height() > 0);
	QVERIFY(monitor.contains(result));

	const int topGap = result.top() - monitor.top();
	const int bottomGap = monitor.bottom() - result.bottom();
	QVERIFY(qAbs(topGap - bottomGap) <= 1);
}

void ContentAspectPresetTest::testNormalizeDefaultsUnknownToFill()
{
	QCOMPARE(ContentAspectPreset::normalize(QStringLiteral("bogus")), ContentAspectPreset::Fill);
	QCOMPARE(ContentAspectPreset::normalize(QString()), ContentAspectPreset::Fill);
	QCOMPARE(ContentAspectPreset::normalize(ContentAspectPreset::Ratio16x9), ContentAspectPreset::Ratio16x9);
}
