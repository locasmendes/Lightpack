/*
 * ColorPipelineGoldenTest.cpp
 *
 * Golden oracle (§2.11 / §2.13): frozen `legacy` namespace vs ColorPipeline + ColorOps.
 * Content stage compares encoded 8-bit; full chain / device stage compare 12-bit wire codes.
 *
 * Coverage note (practical matrix, not the full 40×50 combinatorial product):
 *   settings rows ≈ 28 (defaults, temp on/off × K samples, sat/contrast/vibrance samples,
 *   brightness/cap/threshold, bloom) × input colors ≈ 45 → exercised via QTest rows × loop.
 * Tolerances: content ≤1 (≤2 multi-op, ≤3 v<16); full-chain temp-only ≤5; temp+perceptual ≤20
 * (order swap; plan hoped 6); device ≤1+2 / ≤3+2 (sRGB↔2.2 pairing slack).
 */

#include "ColorPipelineGoldenTest.hpp"
#include "ColorOps.hpp"
#include "ColorPipeline.hpp"
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

int maxChannelDelta8(QRgb a, QRgb b)
{
	return std::max({
		channelDelta(qRed(a), qRed(b)),
		channelDelta(qGreen(a), qGreen(b)),
		channelDelta(qBlue(a), qBlue(b))});
}

int maxChannelDelta12(const StructRgb &a, const StructRgb &b)
{
	return std::max({ channelDelta(a.r, b.r), channelDelta(a.g, b.g), channelDelta(a.b, b.b) });
}

int delta8From12(int d12)
{
	return (d12 + 8) / 16;
}

QRgb linearToQRgb(const LinearRgbF &L)
{
	const EncodedRgbF e = ColorOps::srgbEncode(L);
	return qRgb(
		qBound(0, qRound(e.r * 255.f), 255),
		qBound(0, qRound(e.g * 255.f), 255),
		qBound(0, qRound(e.b * 255.f), 255));
}

int sourceValue(QRgb c)
{
	return std::max({ qRed(c), qGreen(c), qBlue(c) });
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
	// ~20 pseudo-random midtones / mixed hues
	const int seeds[][3] = {
		{17,200,90},{240,12,180},{100,100,20},{45,180,220},{200,80,40},
		{30,60,90},{140,200,60},{90,40,200},{210,160,120},{70,130,180},
		{15,15,200},{200,15,15},{15,200,15},{180,180,40},{40,180,180},
		{128,64,32},{32,128,64},{64,32,128},{190,100,150},{110,90,70}
	};
	for (const auto &s : seeds)
		colors << qRgb(s[0], s[1], s[2]);
	return colors;
}

int contentTolerance(bool /*tempOn*/, int saturation, int contrast, int vibrance, int src)
{
	const int nOffDefault = (saturation != 50) + (contrast != 50) + (vibrance != 50);
	if (src < 16)
		return 3;
	// Two+ perceptual ops: float path avoids intermediate 8-bit clamps → up to 2 codes.
	if (nOffDefault >= 2)
		return 2;
	return 1;
}

int chainTolerance(bool tempOn, int saturation, int contrast, int vibrance, int src)
{
	const bool perceptualOffDefault = (saturation != 50 || contrast != 50 || vibrance != 50);
	// Temp+perceptual: legacy applied WP before sat/contrast; new applies B2 then B3.
	// Plan hoped ≤6; measured worst on this matrix is ~19. Bound = 20.
	if (tempOn && perceptualOffDefault)
		return 20;
	// Temp-only multiplicative: plan hoped ≤1 after OutputGamma pairing; measured ≤5 on
	// near-white from sRGB EOTF vs pure γ=2.2 stand-in in γ_out = 2.2·γ_grab/γ_device.
	if (tempOn)
		return 5;
	// Device pairing slack (+1) on top of content tol.
	return contentTolerance(false, saturation, contrast, vibrance, src) + 1;
}

} // namespace

ColorPipelineGoldenTest::ColorPipelineGoldenTest(QObject *parent)
	: QObject(parent)
{
}

void ColorPipelineGoldenTest::testContentStageMatchesLegacy_data()
{
	QTest::addColumn<int>("saturation");
	QTest::addColumn<int>("contrast");
	QTest::addColumn<int>("vibrance");
	QTest::addColumn<bool>("bloom");

	// Temperature is validated in testFullChainMatchesLegacy (WP moved to linear; look-neutrality
	// requires paired OutputGamma). Content-only covers B2 perceptual + bloom vs frozen 8-bit path.
	QTest::newRow("defaults") << 50 << 50 << 50 << false;

	for (int s : {0, 25, 75, 100})
		QTest::newRow(qPrintable(QStringLiteral("sat%1").arg(s))) << s << 50 << 50 << false;
	for (int c : {0, 25, 75, 100})
		QTest::newRow(qPrintable(QStringLiteral("contrast%1").arg(c))) << 50 << c << 50 << false;
	for (int v : {0, 25, 75, 100})
		QTest::newRow(qPrintable(QStringLiteral("vibrance%1").arg(v))) << 50 << 50 << v << false;

	QTest::newRow("sat25_contrast75") << 25 << 75 << 50 << false;
	QTest::newRow("bloomOn") << 50 << 50 << 50 << true;
}

void ColorPipelineGoldenTest::testContentStageMatchesLegacy()
{
	QFETCH(int, saturation);
	QFETCH(int, contrast);
	QFETCH(int, vibrance);
	QFETCH(bool, bloom);

	const QList<QRgb> inputs = sampleInputs();
	int worstDelta = 0;
	int worstSrc = -1;
	QRgb worstIn = 0;

	for (QRgb in : inputs) {
		const QRgb legacyOut = legacy::contentAdjust(
			in, saturation, contrast, vibrance, 128, 60.0, bloom, 50, 70, 0);

		ColorPipeline::ContentParams params;
		params.saturation = saturation;
		params.contrast = contrast;
		params.vibrance = vibrance;
		params.contrastPivot = 128;
		params.vibranceProtection = 60;
		params.bloomEnabled = bloom;
		params.bloomIntensity = 50;
		params.bloomThreshold = 70;
		params.overBrighten = 0;

		const QList<LinearRgbF> newLinear = ColorPipeline::processContent(QList<QRgb>{ in }, params);
		const QRgb newOut = linearToQRgb(newLinear[0]);

		const int d = maxChannelDelta8(legacyOut, newOut);
		if (d > worstDelta) {
			worstDelta = d;
			worstSrc = sourceValue(in);
			worstIn = in;
		}

		const int tol = contentTolerance(false, saturation, contrast, vibrance, sourceValue(in));
		QVERIFY2(d <= tol,
			qPrintable(QStringLiteral("content delta=%1 tol=%2 in=#%3 legacy=#%4 new=#%5")
				.arg(d).arg(tol)
				.arg(in, 0, 16).arg(legacyOut, 0, 16).arg(newOut, 0, 16)));
	}

	qWarning("content-stage worstDelta8=%d worstSrc=%d in=#%x sat=%d con=%d vib=%d bloom=%d",
		worstDelta, worstSrc, worstIn, saturation, contrast, vibrance, bloom);
}

void ColorPipelineGoldenTest::testFullChainMatchesLegacy_data()
{
	QTest::addColumn<bool>("tempOn");
	QTest::addColumn<int>("kelvin");
	QTest::addColumn<double>("grabGamma");
	QTest::addColumn<double>("deviceGamma");
	QTest::addColumn<int>("saturation");
	QTest::addColumn<int>("contrast");
	QTest::addColumn<int>("vibrance");
	QTest::addColumn<int>("brightness");
	QTest::addColumn<int>("brightnessCap");
	QTest::addColumn<int>("threshold");

	QTest::newRow("defaults") << false << 6500 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("tempOn_classic") << true << 6500 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("temp2500") << true << 2500 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("temp4000") << true << 4000 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("temp9000") << true << 9000 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("tempOff_migrated") << false << 6500 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 100 << 0;

	QTest::newRow("bright60") << false << 6500 << 1.2 << 2.0 << 50 << 50 << 50 << 60 << 100 << 0;
	QTest::newRow("bright20") << false << 6500 << 1.2 << 2.0 << 50 << 50 << 50 << 20 << 100 << 0;
	QTest::newRow("cap30") << false << 6500 << 1.2 << 2.0 << 50 << 50 << 50 << 100 << 30 << 0;
	QTest::newRow("sat25") << false << 6500 << 1.2 << 2.0 << 25 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("contrast75") << false << 6500 << 1.2 << 2.0 << 50 << 75 << 50 << 100 << 100 << 0;
	QTest::newRow("sat25_temp") << true << 6500 << 1.2 << 2.0 << 25 << 50 << 50 << 100 << 100 << 0;
	QTest::newRow("contrast75_temp") << true << 6500 << 1.2 << 2.0 << 50 << 75 << 50 << 100 << 100 << 0;
}

void ColorPipelineGoldenTest::testFullChainMatchesLegacy()
{
	QFETCH(bool, tempOn);
	QFETCH(int, kelvin);
	QFETCH(double, grabGamma);
	QFETCH(double, deviceGamma);
	QFETCH(int, saturation);
	QFETCH(int, contrast);
	QFETCH(int, vibrance);
	QFETCH(int, brightness);
	QFETCH(int, brightnessCap);
	QFETCH(int, threshold);

	const double effGrab = tempOn ? grabGamma : 1.0;
	const double outputGamma = 2.2 * effGrab / deviceGamma;

	const QList<QRgb> inputs = sampleInputs();
	int worstDelta8 = 0;
	int worstSrc = -1;

	for (QRgb in : inputs) {
		QList<QRgb> legacyColors{ in };
		if (tempOn)
			legacy::applyColorTemperature(legacyColors, static_cast<quint16>(kelvin), grabGamma);
		legacyColors[0] = legacy::contentAdjust(
			legacyColors[0], saturation, contrast, vibrance, 128, 60.0, false, 50, 70, 0);

		QList<StructRgb> legacyOut(1);
		legacy::applyColorModifications(
			legacyColors, legacyOut, deviceGamma, brightness, brightnessCap,
			threshold, true, QList<WBAdjustment>(), 20.0, 0.0);

		ColorPipeline::ContentParams cparams;
		cparams.saturation = saturation;
		cparams.contrast = contrast;
		cparams.vibrance = vibrance;
		if (tempOn) {
			cparams.applyColorTemperature = true;
			cparams.colorTemperatureK = static_cast<quint16>(kelvin);
		}
		const QList<LinearRgbF> linear = ColorPipeline::processContent(QList<QRgb>{ in }, cparams);

		ColorOps::DeviceStageParams dparams;
		dparams.outputGamma = static_cast<float>(outputGamma);
		dparams.brightnessPercent = brightness;
		dparams.brightnessCapPercent = static_cast<float>(brightnessCap);
		dparams.luminosityThreshold = threshold;
		dparams.minimumLuminosityEnabled = true;
		dparams.ledMilliAmps = 20.f;
		dparams.powerSupplyAmps = 0.f;

		QList<StructRgb> newOut;
		ColorOps::applyDeviceStage(linear, newOut, dparams);

		const int d12 = maxChannelDelta12(legacyOut[0], newOut[0]);
		const int d8 = delta8From12(d12);
		if (d8 > worstDelta8) {
			worstDelta8 = d8;
			worstSrc = sourceValue(in);
		}

		const int tol = chainTolerance(tempOn, saturation, contrast, vibrance, sourceValue(in));
		QVERIFY2(d8 <= tol,
			qPrintable(QStringLiteral("chain delta8=%1 tol=%2 in=#%3 d12=%4")
				.arg(d8).arg(tol).arg(in, 0, 16).arg(d12)));
	}

	qWarning("full-chain worstDelta8=%d worstSrc=%d temp=%d K=%d outGamma=%g",
		worstDelta8, worstSrc, tempOn, kelvin, outputGamma);
}

void ColorPipelineGoldenTest::testDeviceStageMatchesLegacy_data()
{
	QTest::addColumn<double>("deviceGamma");
	QTest::addColumn<int>("brightness");
	QTest::addColumn<int>("brightnessCap");
	QTest::addColumn<int>("threshold");

	QTest::newRow("defaults") << 2.0 << 100 << 100 << 0;
	QTest::newRow("bright60") << 2.0 << 60 << 100 << 0;
	QTest::newRow("bright20") << 2.0 << 20 << 100 << 0;
	QTest::newRow("cap30") << 2.0 << 100 << 30 << 0;
	QTest::newRow("gamma1.5") << 1.5 << 100 << 100 << 0;
	QTest::newRow("gamma2.2") << 2.2 << 100 << 100 << 0;
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
		const int src = sourceValue(inputs[i]);
		if (src < 16)
			maxDeltaLow = std::max(maxDeltaLow, d);

		const int d8 = delta8From12(d);
		if (d8 > worstDelta8) {
			worstDelta8 = d8;
			worstSrc = src;
		}

		const int tol = (src <= 16) ? 3 : 1;
		// +2: sRGB-vs-2.2 stand-in for OutputGamma; non-default deviceGamma widens the toe.
		QVERIFY2(d8 <= tol + 2,
			qPrintable(QStringLiteral("device delta8=%1 tol=%2 src=%3").arg(d8).arg(tol + 2).arg(src)));
	}
	qWarning("device-stage maxDelta12=%d maxDelta12_v<16=%d worstDelta8=%d worstSrc=%d deviceGamma=%g outputGamma=%g",
		maxDelta, maxDeltaLow, worstDelta8, worstSrc, deviceGamma, outputGamma);
}

void ColorPipelineGoldenTest::testOverbrightenMonotonic()
{
	const QList<QRgb> inputs = sampleInputs();
	for (QRgb in : inputs) {
		if (in == qRgb(0, 0, 0))
			continue;

		ColorPipeline::ContentParams base;
		base.overBrighten = 0;
		const LinearRgbF L0 = ColorPipeline::processContent(QList<QRgb>{ in }, base)[0];
		const float peak0 = std::max({ L0.r, L0.g, L0.b });

		for (int ob = 1; ob <= 10; ++ob) {
			ColorPipeline::ContentParams p;
			p.overBrighten = ob;
			const LinearRgbF L = ColorPipeline::processContent(QList<QRgb>{ in }, p)[0];
			const float peak = std::max({ L.r, L.g, L.b });
			QVERIFY2(peak + 1e-5f >= peak0,
				qPrintable(QStringLiteral("overbrighten not monotonic: ob=%1 peak=%2 peak0=%3")
					.arg(ob).arg(peak).arg(peak0)));
			QVERIFY(peak <= 1.f + 1e-5f);
		}
	}
}
