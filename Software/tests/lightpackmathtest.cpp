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
