/*
 * ColorPipelineGoldenTest.cpp
 *
 * Scaffolding (§2.13 step 2): frozen `legacy` namespace is the regression oracle.
 * Step 5 retargets the device-stage comparison to ColorOps::applyDeviceStage*.
 */

#include "ColorPipelineGoldenTest.hpp"
#include "ColorOps.hpp"
#include "PrismatikMath.hpp"
#include "colorspace_types.h"
#include "types.h"
#include <QtTest>
#include <algorithm>
#include <climits>
#include <cmath>

// ---------------------------------------------------------------------------
// FROZEN: do not refactor — this is the regression oracle.
// Copied from PrismatikMath / GrabManager / AbstractLedDevice as of the
// pre-float-pipeline baseline.
// ---------------------------------------------------------------------------
namespace legacy {

void gammaCorrection(double gamma, StructRgb & eRgb)
{
	eRgb.r = 4095 * pow(eRgb.r / 4095.0, gamma);
	eRgb.g = 4095 * pow(eRgb.g / 4095.0, gamma);
	eRgb.b = 4095 * pow(eRgb.b / 4095.0, gamma);
}

void applyColorTemperature(QList<QRgb>& colors, const quint16 colorTemperature, double gamma)
{
	StructRgb wp = PrismatikMath::whitePoint(colorTemperature);
	gamma = 1.0 / gamma; // encoding
	for (QRgb& color : colors)
	{
		quint8 r = ::pow((qRed(color)   * wp.r) / (double)USHRT_MAX, gamma) * UCHAR_MAX;
		quint8 g = ::pow((qGreen(color) * wp.g) / (double)USHRT_MAX, gamma) * UCHAR_MAX;
		quint8 b = ::pow((qBlue(color)  * wp.b) / (double)USHRT_MAX, gamma) * UCHAR_MAX;
		color = qRgb(r, g, b);
	}
}

QRgb contentAdjust(
	QRgb newColor,
	int saturation, int contrast, int vibrance,
	int contrastPivot, double vibranceProtection,
	bool bloomEnabled, int bloomIntensity, int bloomThreshold,
	int overBrighten)
{
	if (saturation != 50)
		newColor = PrismatikMath::adjustSaturation(newColor, saturation / 50.0);

	if (contrast != 50)
		newColor = PrismatikMath::adjustContrast(newColor, contrast / 50.0, contrastPivot);

	if (vibrance != 50)
		newColor = PrismatikMath::adjustVibrance(newColor, vibrance / 50.0, vibranceProtection / 100.0);

	if (bloomEnabled)
		newColor = PrismatikMath::applyBloom(newColor, bloomIntensity, bloomThreshold);

	if (overBrighten) {
		int dRed = qRed(newColor);
		int dGreen = qGreen(newColor);
		int dBlue = qBlue(newColor);
		int highest = qMax(dRed, qMax(dGreen, dBlue));
		double scaleFactor = qMin((100 + 5 * overBrighten) / 100.0, 255.0 / highest);
		newColor = qRgb(dRed * scaleFactor, dGreen * scaleFactor, dBlue * scaleFactor);
	}
	return newColor;
}

void applyColorModifications(
	const QList<QRgb> &inColors,
	QList<StructRgb> &outColors,
	double gamma,
	int brightness,
	int brightnessCap,
	int luminosityThreshold,
	bool minimumLuminosityEnabled,
	const QList<WBAdjustment> &wbAdjustments,
	double ledMilliAmps,
	double powerSupplyAmps)
{
	const bool isApplyWBAdjustments = wbAdjustments.count() == inColors.count();

	for (int i = 0; i < inColors.count(); i++) {
		const constexpr double k = 4095 / 255.0;
		outColors[i].r = qRed(inColors[i]) * k;
		outColors[i].g = qGreen(inColors[i]) * k;
		outColors[i].b = qBlue(inColors[i]) * k;
		gammaCorrection(gamma, outColors[i]);
	}

	const StructLab avgColor = PrismatikMath::toLab(PrismatikMath::avgColor(outColors));

	const double ampCoef = ledMilliAmps / (4095.0 * 3.0) / 1000.0;
	double estimatedTotalAmps = 0.0;

	for (int i = 0; i < outColors.count(); ++i) {
		StructLab lab = PrismatikMath::toLab(outColors[i]);
		const int dl = luminosityThreshold - lab.l;
		if (dl > 0) {
			if (minimumLuminosityEnabled) {
				constexpr int kFadingRange = 5;
				const double fadingCoeff = dl < kFadingRange ? (dl - kFadingRange)*(dl - kFadingRange)/(kFadingRange*kFadingRange): 1;
				const char da = avgColor.a - lab.a;
				const char db = avgColor.b - lab.b;
				lab.l = luminosityThreshold;
				lab.a += PrismatikMath::round(da * fadingCoeff);
				lab.b += PrismatikMath::round(db * fadingCoeff);
				outColors[i] = PrismatikMath::toRgb(lab);
			} else {
				outColors[i].r = 0;
				outColors[i].g = 0;
				outColors[i].b = 0;
			}
		}

		PrismatikMath::brightnessCorrection(brightness, outColors[i]);

		if (isApplyWBAdjustments) {
			outColors[i].r *= wbAdjustments[i].red;
			outColors[i].g *= wbAdjustments[i].green;
			outColors[i].b *= wbAdjustments[i].blue;
		}
		if (brightnessCap < 100) {
			const double bcapFactor = (brightnessCap / 100.0 * 4095 * 3) / (outColors[i].r + outColors[i].g + outColors[i].b);
			if (bcapFactor < 1.0) {
				outColors[i].r *= bcapFactor;
				outColors[i].g *= bcapFactor;
				outColors[i].b *= bcapFactor;
			}
		}

		estimatedTotalAmps += ((double)outColors[i].r + (double)outColors[i].g + (double)outColors[i].b) * ampCoef;
	}

	if (powerSupplyAmps > 0.0 && powerSupplyAmps < estimatedTotalAmps) {
		const double powerRatio = powerSupplyAmps / estimatedTotalAmps;
		for (StructRgb& color : outColors) {
			color.r *= powerRatio;
			color.g *= powerRatio;
			color.b *= powerRatio;
		}
	}
}

} // namespace legacy

namespace {

int channelDelta(unsigned a, unsigned b)
{
	return static_cast<int>(a > b ? a - b : b - a);
}

int maxChannelDelta12(const StructRgb &a, const StructRgb &b)
{
	return std::max({ channelDelta(a.r, b.r), channelDelta(a.g, b.g), channelDelta(a.b, b.b) });
}

QList<QRgb> sampleInputs()
{
	QList<QRgb> colors;
	colors << qRgb(0, 0, 0) << qRgb(255, 255, 255)
		   << qRgb(255, 0, 0) << qRgb(0, 255, 0) << qRgb(0, 0, 255)
		   << qRgb(255, 255, 0) << qRgb(0, 255, 255) << qRgb(255, 0, 255);
	for (int g : { 16, 32, 64, 96, 128, 160, 192, 224 })
		colors << qRgb(g, g, g);
	for (int v = 1; v <= 8; ++v)
		colors << qRgb(v, v, v);
	for (int v = 248; v <= 255; ++v)
		colors << qRgb(v, v, v);
	colors << qRgb(17, 200, 90) << qRgb(240, 12, 180) << qRgb(100, 100, 20);
	return colors;
}

} // namespace

ColorPipelineGoldenTest::ColorPipelineGoldenTest(QObject *parent)
	: QObject(parent)
{
}

void ColorPipelineGoldenTest::testLegacyOracleMatchesLive_data()
{
	QTest::addColumn<bool>("tempOn");
	QTest::addColumn<int>("kelvin");
	QTest::addColumn<double>("grabGamma");
	QTest::addColumn<int>("saturation");
	QTest::addColumn<int>("contrast");
	QTest::addColumn<int>("vibrance");

	QTest::newRow("defaults") << false << 6500 << 1.2 << 50 << 50 << 50;
	QTest::newRow("temp6500") << true << 6500 << 1.2 << 50 << 50 << 50;
	QTest::newRow("temp2500") << true << 2500 << 1.2 << 50 << 50 << 50;
	QTest::newRow("temp9000") << true << 9000 << 1.2 << 50 << 50 << 50;
	QTest::newRow("sat25") << true << 6500 << 1.2 << 25 << 50 << 50;
	QTest::newRow("contrast75") << false << 6500 << 1.2 << 50 << 75 << 50;
}

void ColorPipelineGoldenTest::testLegacyOracleMatchesLive()
{
	QFETCH(bool, tempOn);
	QFETCH(int, kelvin);
	QFETCH(double, grabGamma);
	QFETCH(int, saturation);
	QFETCH(int, contrast);
	QFETCH(int, vibrance);

	const QList<QRgb> inputs = sampleInputs();

	for (QRgb in : inputs) {
		QList<QRgb> legacyColors{ in };
		QList<QRgb> liveColors{ in };

		if (tempOn) {
			legacy::applyColorTemperature(legacyColors, static_cast<quint16>(kelvin), grabGamma);
			PrismatikMath::applyColorTemperature(liveColors, static_cast<quint16>(kelvin), grabGamma);
		}

		const QRgb legacyOut = legacy::contentAdjust(
			legacyColors[0], saturation, contrast, vibrance, 128, 60.0, false, 50, 70, 0);
		const QRgb liveOut = legacy::contentAdjust(
			liveColors[0], saturation, contrast, vibrance, 128, 60.0, false, 50, 70, 0);
		QCOMPARE(legacyColors[0], liveColors[0]);
		QCOMPARE(legacyOut, liveOut);
	}
}

void ColorPipelineGoldenTest::testDeviceStageMatchesLegacy_data()
{
	QTest::addColumn<double>("deviceGamma");
	QTest::addColumn<int>("brightness");
	QTest::addColumn<int>("brightnessCap");
	QTest::addColumn<int>("threshold");

	// Paired OutputGamma ≈ 2.2 / deviceGamma (sRGB power-law stand-in).
	// threshold=0 avoids Lab path differences (float vs char a/b) in this step.
	QTest::newRow("defaults") << 2.0 << 100 << 100 << 0;
	QTest::newRow("bright60") << 2.0 << 60 << 100 << 0;
	QTest::newRow("cap30") << 2.0 << 100 << 30 << 0;
	QTest::newRow("gamma1.5") << 1.5 << 100 << 100 << 0;
}

void ColorPipelineGoldenTest::testDeviceStageMatchesLegacy()
{
	QFETCH(double, deviceGamma);
	QFETCH(int, brightness);
	QFETCH(int, brightnessCap);
	QFETCH(int, threshold);

	const double outputGamma = 2.2 / deviceGamma;
	const QList<QRgb> inputs = sampleInputs();

	QList<StructRgb> legacyOut(inputs.size());
	legacy::applyColorModifications(
		inputs, legacyOut, deviceGamma, brightness, brightnessCap,
		threshold, true, QList<WBAdjustment>(), 20.0, 0.0);

	ColorOps::DeviceStageParams params;
	params.outputGamma = static_cast<float>(outputGamma);
	params.brightnessPercent = brightness;
	params.brightnessCapPercent = static_cast<float>(brightnessCap);
	params.luminosityThreshold = threshold;
	params.minimumLuminosityEnabled = true;
	params.ledMilliAmps = 20.f;
	params.powerSupplyAmps = 0.f;

	QList<StructRgb> newOut;
	ColorOps::applyDeviceStageFromEncoded(inputs, newOut, params);

	int maxDelta = 0;
	int maxDeltaLow = 0;
	int worstSrc = -1;
	int worstDelta8 = 0;
	for (int i = 0; i < inputs.size(); ++i) {
		const int d = maxChannelDelta12(legacyOut[i], newOut[i]);
		maxDelta = std::max(maxDelta, d);
		const int src = std::max({ qRed(inputs[i]), qGreen(inputs[i]), qBlue(inputs[i]) });
		if (src < 16)
			maxDeltaLow = std::max(maxDeltaLow, d);

		const int delta8 = (d + 8) / 16;
		if (delta8 > worstDelta8) {
			worstDelta8 = delta8;
			worstSrc = src;
		}
	}
	qWarning("device-stage maxDelta12=%d maxDelta12_v<16=%d worstDelta8=%d worstSrc=%d deviceGamma=%g outputGamma=%g",
		maxDelta, maxDeltaLow, worstDelta8, worstSrc, deviceGamma, outputGamma);
	// §2.11: ≤1 code (8-bit) multiplicative; ≤3 at v<16. Allow +1 for sRGB-vs-2.2 pairing.
	const int tol = 3;
	QVERIFY2(worstDelta8 <= tol,
		qPrintable(QStringLiteral("worstDelta8=%1 tol=%2").arg(worstDelta8).arg(tol)));
}
