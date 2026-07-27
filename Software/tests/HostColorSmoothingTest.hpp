#ifndef HOSTCOLORSMOOTHINGTEST_HPP
#define HOSTCOLORSMOOTHINGTEST_HPP

#include <QObject>

class HostColorSmoothingTest : public QObject
{
	Q_OBJECT
public:
	explicit HostColorSmoothingTest(QObject *parent = 0);

private slots:
	void testZeroDurationIsImmediate();
	void testLinearMathWithKnownValues();
	void testRetargetMidTransitionHasNoDiscontinuity();
	void testIdenticalRetargetDoesNotRestartClock();
	void testStopsExactlyAtFinalFrame();
	void testResizeAndResetCancelsTransition();
	void testDurationChangeToZeroFinishesImmediately();
	void testDurationChangeMidTransitionKeepsContinuity();
};

#endif // HOSTCOLORSMOOTHINGTEST_HPP
