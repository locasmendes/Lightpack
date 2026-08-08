#include "HostColorSmoothingTest.hpp"
#include "HostColorSmoothing.hpp"
#include "ColorOps.hpp"
#include <QtTest>
#include <cmath>

namespace {

LinearRgbF from8(int r, int g, int b)
{
	return ColorOps::srgbDecode(qRgb(r, g, b));
}

QRgb to8(const LinearRgbF &L)
{
	const EncodedRgbF e = ColorOps::srgbEncode(L);
	return qRgb(
		qBound(0, qRound(e.r * 255.f), 255),
		qBound(0, qRound(e.g * 255.f), 255),
		qBound(0, qRound(e.b * 255.f), 255));
}

bool nearlyEqual8(QRgb a, QRgb b, int tol = 3)
{
	return std::abs(qRed(a) - qRed(b)) <= tol
		&& std::abs(qGreen(a) - qGreen(b)) <= tol
		&& std::abs(qBlue(a) - qBlue(b)) <= tol;
}

QList<LinearRgbF> from8List(const QList<QRgb> &colors)
{
	QList<LinearRgbF> out;
	out.reserve(colors.size());
	for (QRgb c : colors)
		out.append(ColorOps::srgbDecode(c));
	return out;
}

} // namespace

HostColorSmoothingTest::HostColorSmoothingTest(QObject *parent) :
	QObject(parent)
{
}

void HostColorSmoothingTest::testZeroDurationIsImmediate()
{
	HostColorSmoothing engine;
	engine.reset(2);
	engine.setDurationMs(0);

	const QList<LinearRgbF> target = from8List({ qRgb(10, 20, 30), qRgb(40, 50, 60) });
	engine.retarget(target, 12345);

	QVERIFY(!engine.isActive());
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(10, 20, 30)));
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[1]), qRgb(40, 50, 60)));
}

void HostColorSmoothingTest::testLinearMathWithKnownValues()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	engine.setDisplayedImmediately({ from8(0, 100, 200) });
	engine.retarget({ from8(100, 0, 0) }, 0);
	QVERIFY(engine.isActive());

	engine.advance(0);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(0, 100, 200)));
	QVERIFY(engine.isActive());

	engine.advance(100);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(50, 50, 100)));
	QVERIFY(engine.isActive());

	engine.advance(200);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 0, 0)));
	QVERIFY(!engine.isActive());
}

void HostColorSmoothingTest::testRetargetMidTransitionHasNoDiscontinuity()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	engine.retarget({ from8(200, 0, 0) }, 0);
	engine.advance(100);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 0, 0)));

	engine.retarget({ from8(0, 0, 200) }, 100);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 0, 0)));
	QVERIFY(engine.isActive());

	engine.advance(300);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(0, 0, 200)));
	QVERIFY(!engine.isActive());
}

void HostColorSmoothingTest::testIdenticalRetargetDoesNotRestartClock()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	engine.retarget({ from8(200, 0, 0) }, 0);
	engine.advance(50);
	engine.retarget({ from8(200, 0, 0) }, 50);
	engine.advance(100);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 0, 0)));
}

void HostColorSmoothingTest::testStopsExactlyAtFinalFrame()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(100);

	engine.retarget({ from8(50, 60, 70) }, 0);
	QVERIFY(engine.advance(100));
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(50, 60, 70)));
	QVERIFY(!engine.isActive());
	QVERIFY(!engine.advance(150));
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(50, 60, 70)));
}

void HostColorSmoothingTest::testResizeAndResetCancelsTransition()
{
	HostColorSmoothing engine;
	engine.reset(5);
	engine.setDurationMs(200);
	engine.retarget(from8List({ qRgb(1,1,1), qRgb(2,2,2), qRgb(3,3,3), qRgb(4,4,4), qRgb(5,5,5) }), 0);
	engine.advance(50);
	QVERIFY(engine.isActive());

	engine.reset(2);
	QVERIFY(!engine.isActive());
	QCOMPARE(engine.displayedColors().size(), 2);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(0, 0, 0)));

	const QList<LinearRgbF> target = from8List({ qRgb(10, 20, 30), qRgb(40, 50, 60) });
	engine.retarget(target, 100);
	engine.advance(300);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(10, 20, 30)));
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[1]), qRgb(40, 50, 60)));
}

void HostColorSmoothingTest::testDurationChangeToZeroFinishesImmediately()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	engine.retarget({ from8(100, 100, 100) }, 0);
	engine.advance(50);
	engine.changeDurationAndRetarget(0, 50);

	QVERIFY(!engine.isActive());
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 100, 100)));
}

void HostColorSmoothingTest::testDurationChangeMidTransitionKeepsContinuity()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	engine.retarget({ from8(200, 0, 0) }, 0);
	engine.advance(100);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 0, 0)));

	engine.changeDurationAndRetarget(50, 100);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(100, 0, 0)));
	QVERIFY(engine.isActive());

	engine.advance(150);
	QVERIFY(nearlyEqual8(to8(engine.displayedColors()[0]), qRgb(200, 0, 0)));
	QVERIFY(!engine.isActive());
}
