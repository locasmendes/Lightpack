/*
 * CalibrationTest.hpp — Phase 3 unit tests for pattern generators + ΔE solver.
 */

#ifndef CALIBRATIONTEST_HPP
#define CALIBRATIONTEST_HPP

#include <QObject>

class CalibrationTest : public QObject
{
	Q_OBJECT
public:
	explicit CalibrationTest(QObject *parent = nullptr);

private slots:
	void testWhiteWindows();
	void testPrimaryAndSecondaryColors();
	void testGrayRampSteps();
	void testColorBarsCycle();
	void testChaseIdentify();
	void testGenerateFillsAllLeds();

	void testXyToXyzPreservesChromaticity();
	void testSolveGainsIdentityWhenMeasuredMatchesTarget();
	void testSolveGainsCorrectsTintTowardTarget();
	void testDeltaEMonotonicallyNonIncreasing();
	void testCoefsDeviateFromNeutral();
};
#endif
