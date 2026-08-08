/*
 * ColorOpsTest.cpp — unit tests for ColorOps (§2.11).
 */

#include "ColorOpsTest.hpp"
#include "ColorOps.hpp"
#include <QtTest>
#include <cmath>

ColorOpsTest::ColorOpsTest(QObject *parent)
	: QObject(parent)
{
}

void ColorOpsTest::testSrgbRoundTripAndLut()
{
	for (int i = 0; i < 256; ++i) {
		const QRgb in = qRgb(i, i, i);
		const LinearRgbF lin = ColorOps::srgbDecode(in);
		const EncodedRgbF enc = ColorOps::srgbEncode(lin);
		QVERIFY2(std::fabs(enc.r - i / 255.f) < 1e-6f, "round-trip R");
		QVERIFY2(std::fabs(enc.g - i / 255.f) < 1e-6f, "round-trip G");
		QVERIFY2(std::fabs(enc.b - i / 255.f) < 1e-6f, "round-trip B");

		// LUT matches analytical EOTF.
		const LinearRgbF analytical = ColorOps::srgbDecode(EncodedRgbF{ i / 255.f, i / 255.f, i / 255.f });
		QVERIFY(std::fabs(lin.r - analytical.r) < 1e-7f);
	}
}

void ColorOpsTest::testSrgbKnownPoints()
{
	QCOMPARE(ColorOps::srgbDecode(qRgb(0, 0, 0)).r, 0.f);
	QCOMPARE(ColorOps::srgbDecode(qRgb(255, 255, 255)).r, 1.f);

	const float mid = ColorOps::srgbDecode(qRgb(128, 128, 128)).r;
	QVERIFY2(std::fabs(mid - 0.2158605f) < 1e-4f, "decode(128) ≈ 0.2158");

	// Breakpoints from both sides.
	const float justBelow = ColorOps::srgbDecode(EncodedRgbF{ 0.04045f, 0, 0 }).r;
	const float justAbove = ColorOps::srgbDecode(EncodedRgbF{ 0.04046f, 0, 0 }).r;
	QVERIFY(justBelow < justAbove);

	const float encBelow = ColorOps::srgbEncode(LinearRgbF{ 0.0031308f, 0, 0 }).r;
	const float encAbove = ColorOps::srgbEncode(LinearRgbF{ 0.0031309f, 0, 0 }).r;
	QVERIFY(encBelow < encAbove);
}

void ColorOpsTest::testRenderToWire()
{
	const LinearRgbF L{ 0.5f, 0.25f, 0.1f };
	const WireRgbF identity = ColorOps::renderToWire(L, 1.0f);
	QCOMPARE(identity.r, L.r);
	QCOMPARE(identity.g, L.g);
	QCOMPARE(identity.b, L.b);

	// Monotonic in L for fixed gamma.
	const WireRgbF a = ColorOps::renderToWire(LinearRgbF{ 0.2f, 0, 0 }, 1.32f);
	const WireRgbF b = ColorOps::renderToWire(LinearRgbF{ 0.8f, 0, 0 }, 1.32f);
	QVERIFY(a.r < b.r);

	// γ=1.32 ⇒ L^(1/1.32) ≈ L^0.7576
	const float expected = std::pow(0.216f, 1.f / 1.32f);
	const float got = ColorOps::renderToWire(LinearRgbF{ 0.216f, 0, 0 }, 1.32f).r;
	QVERIFY2(std::fabs(got - expected) < 1e-5f, "γ=1.32 power");

	QCOMPARE(ColorOps::renderToWire(LinearRgbF{ 2.f, -1.f, 0.5f }, 1.0f).r, 1.f);
	QCOMPARE(ColorOps::renderToWire(LinearRgbF{ 2.f, -1.f, 0.5f }, 1.0f).g, 0.f);
}

void ColorOpsTest::testLinearWhitePoint()
{
	const LinearRgbF d65 = ColorOps::linearWhitePoint(6500);
	const float peak = std::max(d65.r, std::max(d65.g, d65.b));
	QVERIFY2(std::fabs(peak - 1.f) < 1e-5f, "peak-normalized at 6500");

	const LinearRgbF warm = ColorOps::linearWhitePoint(2500);
	const LinearRgbF cool = ColorOps::linearWhitePoint(9000);
	QVERIFY(cool.b > warm.b);
	QVERIFY(warm.r > cool.r);

	for (int k : { 1000, 2000, 6600, 40000 }) {
		const LinearRgbF wp = ColorOps::linearWhitePoint(static_cast<quint16>(k));
		QVERIFY2(wp.r >= 0.f && wp.r <= 1.f + 1e-5f, "r in range");
		QVERIFY2(wp.g >= 0.f && wp.g <= 1.f + 1e-5f, "g in range");
		QVERIFY2(wp.b >= 0.f && wp.b <= 1.f + 1e-5f, "b in range");
	}
}

void ColorOpsTest::testPerceptualOpsIdentityAtDefault()
{
	const EncodedRgbF c{ 200.f / 255.f, 100.f / 255.f, 50.f / 255.f };
	const EncodedRgbF sat = ColorOps::adjustSaturation(c, 1.f);
	const EncodedRgbF con = ColorOps::adjustContrast(c, 1.f);
	const EncodedRgbF vib = ColorOps::adjustVibrance(c, 1.f, 0.6f);
	QCOMPARE(sat.r, c.r); QCOMPARE(sat.g, c.g); QCOMPARE(sat.b, c.b);
	QCOMPARE(con.r, c.r); QCOMPARE(con.g, c.g); QCOMPARE(con.b, c.b);
	QCOMPARE(vib.r, c.r); QCOMPARE(vib.g, c.g); QCOMPARE(vib.b, c.b);
}

void ColorOpsTest::testPerceptualOpsNoIntermediateClamp()
{
	// Float path: sat 2.0 then sat 0.5 recovers original when chroma*2 stays in [0,1]
	// (no intermediate 8-bit requantize). Pick a mildly saturated color so the boost
	// does not hit the chroma clamp at 1.0.
	const EncodedRgbF c{ 0.60f, 0.40f, 0.30f }; // chroma = 0.30
	const EncodedRgbF boosted = ColorOps::adjustSaturation(c, 2.f);
	const EncodedRgbF restored = ColorOps::adjustSaturation(boosted, 0.5f);
	QVERIFY2(std::fabs(restored.r - c.r) < 1e-4f, "R restored");
	QVERIFY2(std::fabs(restored.g - c.g) < 1e-4f, "G restored");
	QVERIFY2(std::fabs(restored.b - c.b) < 1e-4f, "B restored");
}

void ColorOpsTest::testPowerLimiter()
{
	QList<WireRgbF> colors;
	colors << WireRgbF{ 1.f, 1.f, 1.f } << WireRgbF{ 0.5f, 0.5f, 0.5f };

	// cap == 100 is no-op
	{
		auto c = colors;
		ColorOps::PowerLimiterParams p;
		p.brightnessCapPercent = 100.f;
		ColorOps::applyPowerLimiter(c, p);
		QCOMPARE(c[0].r, 1.f);
		QCOMPARE(c[1].r, 0.5f);
	}

	// cap-only never scales up
	{
		auto c = colors;
		ColorOps::PowerLimiterParams p;
		p.brightnessCapPercent = 30.f;
		ColorOps::applyPowerLimiter(c, p);
		QVERIFY(c[0].r + c[0].g + c[0].b <= 0.3f * 3.f + 1e-5f);
		QVERIFY(c[0].r <= 1.f);
	}

	// PSU-only scales down when over budget
	{
		auto c = colors;
		ColorOps::PowerLimiterParams p;
		p.ledMilliAmps = 50.f;
		p.powerSupplyAmps = 0.01f; // tiny supply
		ColorOps::applyPowerLimiter(c, p);
		QVERIFY(c[0].r < 1.f);
	}

	// Never scales up from PSU
	{
		auto c = colors;
		ColorOps::PowerLimiterParams p;
		p.ledMilliAmps = 50.f;
		p.powerSupplyAmps = 100.f; // huge supply
		ColorOps::applyPowerLimiter(c, p);
		QCOMPARE(c[0].r, 1.f);
	}
}

void ColorOpsTest::testQuantize()
{
	QCOMPARE(ColorOps::quantize(1.f, 8), static_cast<quint16>(255));
	QCOMPARE(ColorOps::quantize(0.f, 8), static_cast<quint16>(0));
	QCOMPARE(ColorOps::quantize(0.5f, 8), static_cast<quint16>(128)); // round-half-up

	QList<WireRgbF> colors;
	for (int i = 0; i < 10; ++i)
		colors << WireRgbF{ 0.333f, 0.333f, 0.333f };
	QList<StructRgb> out;
	ColorOps::quantizeDithered(colors, 8, out);
	double sumIn = 0, sumOut = 0;
	for (int i = 0; i < colors.size(); ++i) {
		sumIn += colors[i].r * 255.0;
		sumOut += out[i].r;
	}
	QVERIFY2(std::fabs(sumIn - sumOut) <= colors.size() + 1e-6, "dither conserves sum ±1/code");
}

void ColorOpsTest::testLuminosityThresholdFloat()
{
	// Feed a pair whose Lab a/b would overflow char (±128) in the legacy path.
	QList<WireRgbF> colors;
	colors << WireRgbF{ 1.f, 0.f, 0.f };   // saturated red
	colors << WireRgbF{ 0.f, 0.f, 1.f };   // saturated blue — large Δa/Δb vs average
	ColorOps::applyLuminosityThreshold(colors, 50, true);
	// No NaN / sign-flip explosion: channels stay finite and in range.
	for (const WireRgbF &c : colors) {
		QVERIFY(std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b));
		QVERIFY(c.r >= -1e-5f && c.r <= 1.f + 1e-5f);
		QVERIFY(c.g >= -1e-5f && c.g <= 1.f + 1e-5f);
		QVERIFY(c.b >= -1e-5f && c.b <= 1.f + 1e-5f);
	}
}
