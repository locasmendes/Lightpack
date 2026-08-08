/*
 * ColorPipelineHysteresisTest.cpp — §2.11 / R14
 *
 * Encoded-domain hysteresis: enter at 1.5/255, exit at 0.5/255.
 * A 1 LSB oscillation must not spam emits once latched then settled.
 */

#include "ColorPipelineHysteresisTest.hpp"
#include "ColorPipeline.hpp"
#include <QtTest>

void ColorPipelineHysteresisTest::testEnterExitThresholds()
{
	const EncodedRgbF a{ 0.5f, 0.5f, 0.5f };
	EncodedRgbF tiny = a;
	tiny.r += 0.4f / 255.f; // below exit and enter
	QVERIFY(!ColorPipeline::colorsChangedEncoded(a, tiny));
	QVERIFY(!ColorPipeline::updateChangeHysteresis(false, a, tiny));

	EncodedRgbF mid = a;
	mid.r += 1.0f / 255.f; // between exit and enter
	QVERIFY(!ColorPipeline::updateChangeHysteresis(false, a, mid));
	QVERIFY(ColorPipeline::updateChangeHysteresis(true, a, mid)); // stay latched until below exit

	EncodedRgbF big = a;
	big.r += 2.0f / 255.f; // above enter
	QVERIFY(ColorPipeline::colorsChangedEncoded(a, big));
	QVERIFY(ColorPipeline::updateChangeHysteresis(false, a, big));
}

void ColorPipelineHysteresisTest::testOneLsbOscillationProducesSingleLatch()
{
	EncodedRgbF prev{ 100.f / 255.f, 100.f / 255.f, 100.f / 255.f };
	bool latched = false;
	int enterCount = 0;

	// Simulate 60 frames oscillating by 1 LSB around the same base.
	for (int i = 0; i < 60; ++i) {
		EncodedRgbF next = prev;
		next.r = (100.f + ((i % 2) ? 1.f : 0.f)) / 255.f;
		const bool was = latched;
		latched = ColorPipeline::updateChangeHysteresis(latched, prev, next);
		if (!was && latched)
			++enterCount;
		if (latched)
			prev = next;
	}

	// 1 LSB (=1/255) is below enter (1.5/255), so never latches → zero emits from enter.
	QCOMPARE(enterCount, 0);

	// A step of 2 LSB should latch once, then 1 LSB keep-alive should not re-enter after exit.
	prev = EncodedRgbF{ 100.f / 255.f, 100.f / 255.f, 100.f / 255.f };
	latched = false;
	enterCount = 0;
	EncodedRgbF step = prev;
	step.r += 2.f / 255.f;
	latched = ColorPipeline::updateChangeHysteresis(latched, prev, step);
	QVERIFY(latched);
	++enterCount;
	prev = step;

	for (int i = 0; i < 60; ++i) {
		EncodedRgbF next = prev;
		next.r += ((i % 2) ? 0.25f : -0.25f) / 255.f; // sub-exit wobble
		const bool was = latched;
		latched = ColorPipeline::updateChangeHysteresis(latched, prev, next);
		if (!was && latched)
			++enterCount;
		if (latched && ColorPipeline::colorsChangedEncoded(prev, next, ColorPipeline::kChangeExitEncoded))
			prev = next;
	}
	QCOMPARE(enterCount, 1);
}
