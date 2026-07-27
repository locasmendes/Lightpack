#include "lightpackmathtest.hpp"
#include "PrismatikMath.hpp"
#include <QtTest>

LightpackMathTest::LightpackMathTest(QObject *parent) :
	QObject(parent)
{
}
void LightpackMathTest::testCase1()
{
	QVERIFY2( PrismatikMath::getValueHSV(qRgb(215,122,0)) == 215, "getValueHSV() is incorrect");
	QRgb testRgb = qRgb(200,100,0);
	QVERIFY2( PrismatikMath::withChromaHSV(testRgb, 100) == qRgb(200, 150, 100), "getChromaHSV() is incorrect");
	QVERIFY2( PrismatikMath::withChromaHSV(testRgb, 250) == qRgb(200, 75, 0), "getChromaHSV() is incorrect");

	QVERIFY2( PrismatikMath::withChromaHSV(testRgb, PrismatikMath::getChromaHSV(testRgb)) == testRgb, "getChromaHSV() is incorrect");
}

void LightpackMathTest::testColorWheel()
{
	const double radius = 100.0;

	// Cardinal angles round-trip through hueSatToPoint -> pointToHueSat.
	for (int hue : {0, 90, 180, 270}) {
		const QPointF p = PrismatikMath::hueSatToPoint(hue, 100, radius);
		int outHue, outSat;
		QVERIFY(PrismatikMath::pointToHueSat(p, radius, outHue, outSat));
		QCOMPARE(outHue, hue);
		QCOMPARE(outSat, 100);
	}

	// Center of the wheel is saturation 0.
	int hue, sat;
	QVERIFY(PrismatikMath::pointToHueSat(QPointF(0, 0), radius, hue, sat));
	QCOMPARE(sat, 0);

	// A point exactly on the edge is saturation 100 and still considered inside.
	QVERIFY(PrismatikMath::pointToHueSat(QPointF(radius, 0), radius, hue, sat));
	QCOMPARE(sat, 100);
	QCOMPARE(hue, 0);

	// A point outside the circle is reported as outside and clamped to saturation 100.
	QVERIFY(!PrismatikMath::pointToHueSat(QPointF(radius * 2, 0), radius, hue, sat));
	QCOMPARE(sat, 100);
}

void LightpackMathTest::testBloom()
{
	// Pure white qualifies strongly at a fairly strict (70%) threshold.
	QVERIFY(PrismatikMath::bloomFactor(qRgb(255, 255, 255), 70) > 200);

	// A near-white (slightly desaturated) pixel qualifies at a lenient threshold.
	QVERIFY(PrismatikMath::bloomFactor(qRgb(240, 245, 250), 50) > 0);

	// A fully saturated color never qualifies, regardless of threshold.
	QCOMPARE(PrismatikMath::bloomFactor(qRgb(255, 0, 0), 0), 0);

	// Black never qualifies either (value is 0, even though chroma is too).
	QCOMPARE(PrismatikMath::bloomFactor(qRgb(0, 0, 0), 50), 0);

	// intensity<=0 is always identity, regardless of how bloom-worthy the pixel is.
	QCOMPARE(PrismatikMath::applyBloom(qRgb(255, 255, 255), 0, 50), qRgb(255, 255, 255));

	// A non-bloom-worthy (saturated) pixel is unchanged even at full intensity.
	QCOMPARE(PrismatikMath::applyBloom(qRgb(255, 0, 0), 100, 50), qRgb(255, 0, 0));

	// Never overflows past 255 per channel.
	const QRgb boosted = PrismatikMath::applyBloom(qRgb(255, 255, 255), 100, 50);
	QVERIFY(qRed(boosted) <= 255 && qGreen(boosted) <= 255 && qBlue(boosted) <= 255);

	// A moderately bright near-white pixel actually gets boosted at full intensity.
	const QRgb dim = qRgb(180, 180, 180);
	const QRgb boostedDim = PrismatikMath::applyBloom(dim, 100, 50);
	QVERIFY(qRed(boostedDim) > qRed(dim));
}

void LightpackMathTest::testColorAdjustments()
{
	const QRgb testRgb = qRgb(200, 100, 50);

	// Neutral factor (1.0) is identity for all three adjustments.
	QCOMPARE(PrismatikMath::adjustSaturation(testRgb, 1.0), testRgb);
	QCOMPARE(PrismatikMath::adjustContrast(testRgb, 1.0), testRgb);
	QCOMPARE(PrismatikMath::adjustVibrance(testRgb, 1.0, 0.6), testRgb);

	// Saturation factor 0 fully desaturates (grayscale: R==G==B).
	const QRgb desaturated = PrismatikMath::adjustSaturation(testRgb, 0.0);
	QCOMPARE(qRed(desaturated), qGreen(desaturated));
	QCOMPARE(qGreen(desaturated), qBlue(desaturated));

	// Contrast pushes channels away from the pivot, clamped to [0,255].
	const QRgb highContrast = PrismatikMath::adjustContrast(qRgb(200, 50, 0), 3.0, 128);
	QCOMPARE(qRed(highContrast), 255);
	QCOMPARE(qBlue(highContrast), 0);

	// Vibrance protects already-saturated pixels: with full protection, boosting a
	// partially-saturated pixel's chroma moves it less than plain saturation would.
	const QRgb partiallySaturated = qRgb(200, 100, 100); // chroma = 100, not maxed out
	const int chromaFromSaturation = PrismatikMath::getChromaHSV(PrismatikMath::adjustSaturation(partiallySaturated, 1.5));
	const int chromaFromVibrance = PrismatikMath::getChromaHSV(PrismatikMath::adjustVibrance(partiallySaturated, 1.5, 1.0));
	QVERIFY(chromaFromVibrance < chromaFromSaturation);
}
