#include "HostColorSmoothingTest.hpp"
#include "HostColorSmoothing.hpp"
#include <QtTest>

// Note: this suite covers HostColorSmoothing in isolation (deterministic, no real
// timers/clock - callers supply arbitrary "now" values in ms). The device-type guard
// (isHostSmoothingApplicable(): device != Lightpack && duration > 0) and the
// fake-grab/host-timer exclusivity live in GrabManager itself, which is not
// practically instantiable in this test binary (it owns real screen grabbers/widgets)
// - those are covered by code inspection instead, per docs/plans/smoothing-host-side.md.

HostColorSmoothingTest::HostColorSmoothingTest(QObject *parent) :
	QObject(parent)
{
}

void HostColorSmoothingTest::testZeroDurationIsImmediate()
{
	HostColorSmoothing engine;
	engine.reset(2);
	engine.setDurationMs(0);

	const QList<QRgb> target = { qRgb(10, 20, 30), qRgb(40, 50, 60) };
	engine.retarget(target, 12345);

	QVERIFY(!engine.isActive());
	QCOMPARE(engine.displayedColors(), target);
}

void HostColorSmoothingTest::testLinearMathWithKnownValues()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	const QRgb start = qRgb(0, 100, 200);
	const QRgb target = qRgb(100, 0, 0);

	engine.setDisplayedImmediately({ start });
	engine.retarget({ target }, 0);
	QVERIFY(engine.isActive());

	engine.advance(0);
	QCOMPARE(engine.displayedColors()[0], start);
	QVERIFY(engine.isActive());

	engine.advance(100);
	QCOMPARE(engine.displayedColors()[0], qRgb(50, 50, 100));
	QVERIFY(engine.isActive());

	engine.advance(200);
	QCOMPARE(engine.displayedColors()[0], target);
	QVERIFY(!engine.isActive());
}

void HostColorSmoothingTest::testRetargetMidTransitionHasNoDiscontinuity()
{
	HostColorSmoothing engine;
	engine.reset(1); // starts at black
	engine.setDurationMs(200);

	const QRgb red = qRgb(200, 0, 0);
	const QRgb blue = qRgb(0, 0, 200);

	engine.retarget({ red }, 0);
	engine.advance(100);
	QCOMPARE(engine.displayedColors()[0], qRgb(100, 0, 0));

	// Retarget mid-transition: the new transition must start exactly from the color
	// that was just displayed - no jump back to black, no snap to the old target.
	engine.retarget({ blue }, 100);
	QCOMPARE(engine.displayedColors()[0], qRgb(100, 0, 0));
	QVERIFY(engine.isActive());

	engine.advance(300); // another full 200ms from the retarget at t=100
	QCOMPARE(engine.displayedColors()[0], blue);
	QVERIFY(!engine.isActive());
}

void HostColorSmoothingTest::testIdenticalRetargetDoesNotRestartClock()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	const QRgb target = qRgb(200, 0, 0);
	engine.retarget({ target }, 0);
	engine.advance(50);

	// A retarget to the SAME target must not reset the clock.
	engine.retarget({ target }, 50);

	engine.advance(100); // if the clock had restarted at 50, t would be 0.25 here, not 0.5
	QCOMPARE(engine.displayedColors()[0], qRgb(100, 0, 0));
}

void HostColorSmoothingTest::testStopsExactlyAtFinalFrame()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(100);

	const QRgb target = qRgb(50, 60, 70);
	engine.retarget({ target }, 0);

	QVERIFY(engine.advance(100));
	QCOMPARE(engine.displayedColors()[0], target);
	QVERIFY(!engine.isActive());

	// No duplicate emission once finished: advance() past the end is a no-op.
	QVERIFY(!engine.advance(150));
	QCOMPARE(engine.displayedColors()[0], target);
}

void HostColorSmoothingTest::testResizeAndResetCancelsTransition()
{
	HostColorSmoothing engine;
	engine.reset(5);
	engine.setDurationMs(200);
	engine.retarget({ qRgb(1,1,1), qRgb(2,2,2), qRgb(3,3,3), qRgb(4,4,4), qRgb(5,5,5) }, 0);
	engine.advance(50);
	QVERIFY(engine.isActive());

	// A resize/profile switch to a different LED count must cancel the transition and
	// never leave a later tick interpolating between mismatched array sizes.
	engine.reset(2);
	QVERIFY(!engine.isActive());
	QCOMPARE(engine.displayedColors(), QList<QRgb>(2, qRgb(0, 0, 0)));

	// The engine must still work correctly at the new size.
	const QList<QRgb> target = { qRgb(10, 20, 30), qRgb(40, 50, 60) };
	engine.retarget(target, 100);
	engine.advance(300);
	QCOMPARE(engine.displayedColors(), target);
}

void HostColorSmoothingTest::testDurationChangeToZeroFinishesImmediately()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	const QRgb target = qRgb(100, 100, 100);
	engine.retarget({ target }, 0);
	engine.advance(50); // mid-transition, t=0.25

	engine.changeDurationAndRetarget(0, 50);

	QVERIFY(!engine.isActive());
	QCOMPARE(engine.displayedColors()[0], target);
}

void HostColorSmoothingTest::testDurationChangeMidTransitionKeepsContinuity()
{
	HostColorSmoothing engine;
	engine.reset(1);
	engine.setDurationMs(200);

	const QRgb target = qRgb(200, 0, 0);
	engine.retarget({ target }, 0);
	engine.advance(100); // t=0.5 -> (100,0,0)
	QCOMPARE(engine.displayedColors()[0], qRgb(100, 0, 0));

	// Change to a shorter duration mid-transition: no jump, same target, finishes
	// `newDurationMs` after the change instead of the original duration.
	engine.changeDurationAndRetarget(50, 100);
	QCOMPARE(engine.displayedColors()[0], qRgb(100, 0, 0));
	QVERIFY(engine.isActive());

	engine.advance(150);
	QCOMPARE(engine.displayedColors()[0], target);
	QVERIFY(!engine.isActive());
}
