/*
 * CalibrationTest.cpp — Phase 3 (§3.5): pattern generators + calibration solver.
 */

#include "CalibrationTest.hpp"
#include "CalibrationPatterns.hpp"
#include "CalibrationSolver.hpp"
#include <QtTest>
#include <cmath>

CalibrationTest::CalibrationTest(QObject *parent)
	: QObject(parent)
{
}

void CalibrationTest::testWhiteWindows()
{
	using namespace CalibrationPatterns;
	QCOMPARE(colorForLed(PatternId::White100, 0, 10), qRgb(255, 255, 255));
	QCOMPARE(colorForLed(PatternId::White75, 0, 10), qRgb(191, 191, 191));
	QCOMPARE(colorForLed(PatternId::White50, 0, 10), qRgb(128, 128, 128));
	QCOMPARE(colorForLed(PatternId::White25, 0, 10), qRgb(64, 64, 64));
}

void CalibrationTest::testPrimaryAndSecondaryColors()
{
	using namespace CalibrationPatterns;
	QCOMPARE(colorForLed(PatternId::Red, 3, 8), qRgb(255, 0, 0));
	QCOMPARE(colorForLed(PatternId::Green, 3, 8), qRgb(0, 255, 0));
	QCOMPARE(colorForLed(PatternId::Blue, 3, 8), qRgb(0, 0, 255));
	QCOMPARE(colorForLed(PatternId::Cyan, 3, 8), qRgb(0, 255, 255));
	QCOMPARE(colorForLed(PatternId::Magenta, 3, 8), qRgb(255, 0, 255));
	QCOMPARE(colorForLed(PatternId::Yellow, 3, 8), qRgb(255, 255, 0));
}

void CalibrationTest::testGrayRampSteps()
{
	using namespace CalibrationPatterns;
	const int n = 5;
	QRgb prev = colorForLed(PatternId::GrayRamp, 0, n);
	QVERIFY(qRed(prev) == qGreen(prev) && qGreen(prev) == qBlue(prev));
	for (int i = 1; i < n; ++i) {
		const QRgb c = colorForLed(PatternId::GrayRamp, i, n);
		QVERIFY(qRed(c) == qGreen(c) && qGreen(c) == qBlue(c));
		QVERIFY2(qRed(c) >= qRed(prev), "Gray ramp must be non-decreasing along the strip");
		prev = c;
	}
	QCOMPARE(colorForLed(PatternId::GrayRamp, n - 1, n), qRgb(255, 255, 255));
}

void CalibrationTest::testColorBarsCycle()
{
	using namespace CalibrationPatterns;
	QCOMPARE(colorForLed(PatternId::ColorBars, 0, 12), qRgb(255, 0, 0));
	QCOMPARE(colorForLed(PatternId::ColorBars, 1, 12), qRgb(0, 255, 0));
	QCOMPARE(colorForLed(PatternId::ColorBars, 2, 12), qRgb(0, 0, 255));
	QCOMPARE(colorForLed(PatternId::ColorBars, 3, 12), qRgb(0, 255, 255));
	QCOMPARE(colorForLed(PatternId::ColorBars, 4, 12), qRgb(255, 0, 255));
	QCOMPARE(colorForLed(PatternId::ColorBars, 5, 12), qRgb(255, 255, 0));
	QCOMPARE(colorForLed(PatternId::ColorBars, 6, 12), qRgb(255, 0, 0)); // wraps
}

void CalibrationTest::testChaseIdentify()
{
	using namespace CalibrationPatterns;
	const int n = 7;
	const int active = 3;
	for (int i = 0; i < n; ++i) {
		const QRgb c = colorForLed(PatternId::ChaseIdentify, i, n, active);
		if (i == active)
			QCOMPARE(c, qRgb(255, 255, 255));
		else
			QCOMPARE(c, qRgb(0, 0, 0));
	}
}

void CalibrationTest::testGenerateFillsAllLeds()
{
	using namespace CalibrationPatterns;
	const QList<QRgb> bars = generate(PatternId::ColorBars, 10);
	QCOMPARE(bars.size(), 10);
	const QList<QRgb> chase = generate(PatternId::ChaseIdentify, 10, 4);
	QCOMPARE(chase.size(), 10);
	QCOMPARE(chase[4], qRgb(255, 255, 255));
}

void CalibrationTest::testXyToXyzPreservesChromaticity()
{
	const StructXyz xyz = CalibrationSolver::xyToXyz(0.3127, 0.3290, 1.0); // D65 approx
	const double sum = xyz.x + xyz.y + xyz.z;
	QVERIFY(sum > 0.0);
	QCOMPARE(xyz.x / sum, 0.3127);
	QCOMPARE(xyz.y / sum, 0.3290);
	QCOMPARE(xyz.y, 1.0);
}

void CalibrationTest::testSolveGainsIdentityWhenMeasuredMatchesTarget()
{
	// Matching linear RGB → identity gains after peak-normalize.
	const CalibrationSolver::Gains id =
		CalibrationSolver::gainsFromLinear(0.8, 0.9, 1.0, 0.8, 0.9, 1.0);
	QVERIFY(std::fabs(id.r - 1.0) < 1e-9);
	QVERIFY(std::fabs(id.g - 1.0) < 1e-9);
	QVERIFY(std::fabs(id.b - 1.0) < 1e-9);

	// Pure white measured vs Kelvin target: gains equal peak-norm white point (≤1).
	const CalibrationSolver::SolveResult r =
		CalibrationSolver::solveFromMeasuredRgb(qRgb(255, 255, 255), 6500);
	QVERIFY(r.gains.r > 0.5 && r.gains.g > 0.5 && r.gains.b > 0.5);
	QVERIFY(r.deltaEAfter <= r.deltaEBefore + 1e-6);
	QVERIFY(r.deltaEAfter < 1e-6);
}

void CalibrationTest::testSolveGainsCorrectsTintTowardTarget()
{
	// Measured too warm (more red) → expect red gain < green/blue relative after normalize.
	const CalibrationSolver::SolveResult r =
		CalibrationSolver::solveFromMeasuredRgb(qRgb(255, 200, 180), 6500);
	QVERIFY(r.gains.r > 0.0 && r.gains.g > 0.0 && r.gains.b > 0.0);
	const double maxG = std::max({r.gains.r, r.gains.g, r.gains.b});
	QCOMPARE(maxG, 1.0);
	// Warm measurement needs less red relative to blue to hit D65-ish target.
	QVERIFY2(r.gains.r <= r.gains.b + 1e-9,
		"Warm measured white should reduce red gain relative to blue");
}

void CalibrationTest::testDeltaEMonotonicallyNonIncreasing()
{
	const QList<QRgb> samples = {
		qRgb(255, 240, 220),
		qRgb(200, 220, 255),
		qRgb(255, 180, 160),
		qRgb(180, 255, 200),
		qRgb(128, 128, 140),
	};
	for (QRgb m : samples) {
		const CalibrationSolver::SolveResult r =
			CalibrationSolver::solveFromMeasuredRgb(m, 6500);
		QVERIFY2(r.deltaEAfter <= r.deltaEBefore + 1e-4,
			qPrintable(QStringLiteral("ΔE grew for measured #%1: before=%2 after=%3")
				.arg(m, 0, 16).arg(r.deltaEBefore).arg(r.deltaEAfter)));
	}
}

void CalibrationTest::testCoefsDeviateFromNeutral()
{
	CalibrationSolver::Gains neutral{1.0, 1.0, 1.0};
	QVERIFY(!CalibrationSolver::coefsDeviateFromNeutral(neutral, 0.05));
	CalibrationSolver::Gains mild{1.04, 1.0, 0.97};
	QVERIFY(!CalibrationSolver::coefsDeviateFromNeutral(mild, 0.05));
	CalibrationSolver::Gains bad{1.10, 1.0, 1.0};
	QVERIFY(CalibrationSolver::coefsDeviateFromNeutral(bad, 0.05));
}
