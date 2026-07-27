#include "LayoutRecipeGeneratorTest.hpp"
#include "wizard/LayoutRecipeGenerator.hpp"
#include "wizard/CustomDistributor.hpp"
#include <QtTest>

LayoutRecipeGeneratorTest::LayoutRecipeGeneratorTest(QObject *parent) :
	QObject(parent)
{
}

void LayoutRecipeGeneratorTest::testGeneratedRectsAreContainedInOffsetContentRect()
{
	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.topLeds = 4;
	recipe.sideLeds = 2;
	recipe.bottomLeds = 4;
	recipe.thicknessPercent = 15;
	recipe.standWidthPercent = 0;
	recipe.skipCorners = false;
	recipe.startingLed = 0;

	const QRect contentRect(437, 202, 2560, 1440); // pillarboxed 16:9 offset, as ContentAspectPreset would produce
	const QMap<int, QRect> rects = LayoutRecipeGenerator::generate(recipe, contentRect);

	QCOMPARE(rects.count(), 12); // 4 + 2*2 + 4
	for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
		QVERIFY2(contentRect.contains(it.value()), qPrintable(QStringLiteral("id %1 rect %2 not contained in content rect").arg(it.key()).arg(QString(QTest::toString(it.value())))));
		QVERIFY(it.value().width() > 0);
		QVERIFY(it.value().height() > 0);
	}
}

void LayoutRecipeGeneratorTest::testLedCountMatchesTopSideBottomSum()
{
	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.topLeds = 3;
	recipe.sideLeds = 5;
	recipe.bottomLeds = 2;
	recipe.startingLed = 100; // ids should shift by this

	const QRect contentRect(0, 0, 1920, 1080);
	const QMap<int, QRect> rects = LayoutRecipeGenerator::generate(recipe, contentRect);

	QCOMPARE(rects.count(), 3 + 2 * 5 + 2);
	QVERIFY(rects.contains(100));
	QVERIFY(rects.contains(100 + rects.count() - 1));
}

void LayoutRecipeGeneratorTest::testSkipCornersPreservesLedCount()
{
	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.topLeds = 4;
	recipe.sideLeds = 3;
	recipe.bottomLeds = 4;
	const QRect contentRect(0, 0, 1920, 1080);

	recipe.skipCorners = false;
	const QMap<int, QRect> withoutSkip = LayoutRecipeGenerator::generate(recipe, contentRect);
	recipe.skipCorners = true;
	const QMap<int, QRect> withSkip = LayoutRecipeGenerator::generate(recipe, contentRect);

	// skipCorners changes spacing/gaps at the corners, not how many LEDs exist.
	QCOMPARE(withSkip.count(), withoutSkip.count());
}

void LayoutRecipeGeneratorTest::testInvalidRecipeReturnsEmpty()
{
	const QRect contentRect(0, 0, 1920, 1080);

	LayoutRecipeGenerator::MonitorRecipe negativeCount;
	negativeCount.topLeds = -1;
	QVERIFY(LayoutRecipeGenerator::generate(negativeCount, contentRect).isEmpty());

	LayoutRecipeGenerator::MonitorRecipe zeroTotal;
	zeroTotal.topLeds = 0;
	zeroTotal.sideLeds = 0;
	zeroTotal.bottomLeds = 0;
	QVERIFY(LayoutRecipeGenerator::generate(zeroTotal, contentRect).isEmpty());

	LayoutRecipeGenerator::MonitorRecipe badThickness;
	badThickness.topLeds = 4;
	badThickness.thicknessPercent = 150; // out of [0,100]
	QVERIFY(LayoutRecipeGenerator::generate(badThickness, contentRect).isEmpty());
}

void LayoutRecipeGeneratorTest::testNonPositiveContentRectReturnsEmpty()
{
	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.topLeds = 4;

	QVERIFY(LayoutRecipeGenerator::generate(recipe, QRect(0, 0, 0, 500)).isEmpty());
	QVERIFY(LayoutRecipeGenerator::generate(recipe, QRect(0, 0, 500, 0)).isEmpty());
	QVERIFY(LayoutRecipeGenerator::generate(recipe, QRect()).isEmpty());
}

void LayoutRecipeGeneratorTest::testJsonRoundTrip()
{
	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.startingLed = 7;
	recipe.topLeds = 4;
	recipe.sideLeds = 3;
	recipe.bottomLeds = 2;
	recipe.thicknessPercent = 20;
	recipe.standWidthPercent = 10;
	recipe.skipCorners = true;
	recipe.invertOrder = true;
	recipe.numberingOffset = 5;
	recipe.baseRect = QRect(12, 34, 1920, 1080);

	const LayoutRecipeGenerator::MonitorRecipe roundTripped = LayoutRecipeGenerator::fromJson(LayoutRecipeGenerator::toJson(recipe));

	QCOMPARE(roundTripped.startingLed, recipe.startingLed);
	QCOMPARE(roundTripped.topLeds, recipe.topLeds);
	QCOMPARE(roundTripped.sideLeds, recipe.sideLeds);
	QCOMPARE(roundTripped.bottomLeds, recipe.bottomLeds);
	QCOMPARE(roundTripped.thicknessPercent, recipe.thicknessPercent);
	QCOMPARE(roundTripped.standWidthPercent, recipe.standWidthPercent);
	QCOMPARE(roundTripped.skipCorners, recipe.skipCorners);
	QCOMPARE(roundTripped.invertOrder, recipe.invertOrder);
	QCOMPARE(roundTripped.numberingOffset, recipe.numberingOffset);
	QCOMPARE(roundTripped.baseRect, recipe.baseRect);
}

void LayoutRecipeGeneratorTest::testBottomLedsOneWithStandWidthDoesNotDivideByZero()
{
	// Regression test for a real division-by-zero in CustomDistributor: with
	// bottomLeds == 1, standWidth != 0 and skipCorners == false, both
	// "bLeds - hLeds" and "hLeds" used to reduce to 0. See CustomDistributor.cpp.
	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.topLeds = 0;
	recipe.sideLeds = 0;
	recipe.bottomLeds = 1;
	recipe.standWidthPercent = 50;
	recipe.skipCorners = false;

	const QMap<int, QRect> rects = LayoutRecipeGenerator::generate(recipe, QRect(0, 0, 1920, 1080));
	QCOMPARE(rects.count(), 1);
	QVERIFY(rects.first().width() > 0);
	QVERIFY(rects.first().height() > 0);
}

void LayoutRecipeGeneratorTest::testDistributorAndRecipeEntryPointsAgree()
{
	// The wizard's live preview goes through generate(AreaDistributor&, ...);
	// ZoneLayoutRuntime goes through generate(MonitorRecipe, QRect). Both must
	// produce byte-for-byte identical results for equivalent inputs, since
	// there is supposed to be exactly one implementation of this math.
	const QRect contentRect(50, 60, 1920, 1080);
	const int topLeds = 5, sideLeds = 3, bottomLeds = 4;
	const double thickness = 0.15, standWidth = 0.0;
	const bool skipCorners = false, invertOrder = true;
	const int numberingOffset = 2, startingLed = 10;

	CustomDistributor distributor(contentRect, topLeds, sideLeds, bottomLeds, thickness, standWidth, skipCorners);
	const QMap<int, QRect> viaDistributor = LayoutRecipeGenerator::generate(distributor, invertOrder, numberingOffset, startingLed);

	LayoutRecipeGenerator::MonitorRecipe recipe;
	recipe.startingLed = startingLed;
	recipe.topLeds = topLeds;
	recipe.sideLeds = sideLeds;
	recipe.bottomLeds = bottomLeds;
	recipe.thicknessPercent = 15;
	recipe.standWidthPercent = 0;
	recipe.skipCorners = skipCorners;
	recipe.invertOrder = invertOrder;
	recipe.numberingOffset = numberingOffset;
	const QMap<int, QRect> viaRecipe = LayoutRecipeGenerator::generate(recipe, contentRect);

	QCOMPARE(viaRecipe, viaDistributor);
}
