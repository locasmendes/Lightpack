/*
 * ColorPipeline.cpp
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#include "ColorPipeline.hpp"
#include "ColorOps.hpp"
#include <QtGlobal>
#include <cmath>

namespace ColorPipeline
{
namespace {

float channelMaxAbsDiff(const EncodedRgbF &a, const EncodedRgbF &b)
{
	return std::max({
		std::fabs(a.r - b.r),
		std::fabs(a.g - b.g),
		std::fabs(a.b - b.b)});
}

void applyWhitePointLinear(QList<LinearRgbF> &colors, quint16 kelvin)
{
	const LinearRgbF wp = ColorOps::linearWhitePoint(kelvin);
	for (LinearRgbF &c : colors) {
		c.r *= wp.r;
		c.g *= wp.g;
		c.b *= wp.b;
	}
}

bool perceptualDefaults(const ContentParams &p)
{
	return p.saturation == 50 && p.contrast == 50 && p.vibrance == 50 && !p.bloomEnabled;
}

void applyPerceptualSandwich(QList<LinearRgbF> &colors, const ContentParams &p)
{
	if (perceptualDefaults(p))
		return;

	const float satF = p.saturation / 50.f;
	const float conF = p.contrast / 50.f;
	const float vibF = p.vibrance / 50.f;
	const float pivot = p.contrastPivot / 255.f;
	const float vibProt = p.vibranceProtection / 100.f;

	for (LinearRgbF &L : colors) {
		EncodedRgbF e = ColorOps::srgbEncode(L);
		if (p.saturation != 50)
			e = ColorOps::adjustSaturation(e, satF);
		if (p.contrast != 50)
			e = ColorOps::adjustContrast(e, conF, pivot);
		if (p.vibrance != 50)
			e = ColorOps::adjustVibrance(e, vibF, vibProt);
		if (p.bloomEnabled)
			e = ColorOps::applyBloom(e, p.bloomIntensity, p.bloomThreshold);
		L = ColorOps::srgbDecode(e);
	}
}

void applyOverbrighten(QList<LinearRgbF> &colors, int overBrighten)
{
	if (overBrighten <= 0)
		return;

	// Linear exposure gain with peak-preserving clamp (§2.6 B4).
	const float gainWanted = 1.f + 0.05f * overBrighten;
	for (LinearRgbF &L : colors) {
		const float peak = std::max({ L.r, L.g, L.b, 1e-8f });
		const float gain = std::min(gainWanted, 1.f / peak);
		L.r *= gain;
		L.g *= gain;
		L.b *= gain;
	}
}

} // namespace

QList<LinearRgbF> processContent(const QList<QRgb> &encodedIn, const ContentParams &params)
{
	QList<LinearRgbF> linear;
	linear.reserve(encodedIn.size());
	for (QRgb c : encodedIn)
		linear.append(ColorOps::srgbDecode(c));
	return processContentLinear(linear, params);
}

QList<LinearRgbF> processContentLinear(const QList<LinearRgbF> &linearIn, const ContentParams &params)
{
	QList<LinearRgbF> colors = linearIn;

	// B1 — average all enabled LEDs (linear, double accumulator via ColorOps::avgColor).
	if (params.avgColorsOnAllLeds && !colors.isEmpty()) {
		QList<LinearRgbF> enabled;
		enabled.reserve(colors.size());
		for (int i = 0; i < colors.size(); ++i) {
			const bool on = params.areaEnabled.isEmpty()
				|| (i < params.areaEnabled.size() && params.areaEnabled[i]);
			if (on)
				enabled.append(colors[i]);
		}
		if (!enabled.isEmpty()) {
			const LinearRgbF avg = ColorOps::avgColor(enabled);
			for (int i = 0; i < colors.size(); ++i) {
				const bool on = params.areaEnabled.isEmpty()
					|| (i < params.areaEnabled.size() && params.areaEnabled[i]);
				if (on)
					colors[i] = avg;
			}
		}
	}

	// B2 — perceptual sandwich (skipped when all controls at default).
	applyPerceptualSandwich(colors, params);

	// B3 — white point / temperature OR blue-light reduction (linear multiply).
	if (params.applyColorTemperature) {
		applyWhitePointLinear(colors, params.colorTemperatureK);
	} else if (params.applyBlueLightReduction && params.blueLightKelvin > 0) {
		applyWhitePointLinear(colors, params.blueLightKelvin);
	}

	// B4 — overbrighten (linear exposure).
	applyOverbrighten(colors, params.overBrighten);

	return colors;
}

bool colorsChangedEncoded(const EncodedRgbF &a, const EncodedRgbF &b, float enterThreshold)
{
	return channelMaxAbsDiff(a, b) >= enterThreshold;
}

bool updateChangeHysteresis(bool currentlyChanged, const EncodedRgbF &prev, const EncodedRgbF &next)
{
	const float d = channelMaxAbsDiff(prev, next);
	if (currentlyChanged)
		return d >= kChangeExitEncoded;
	return d >= kChangeEnterEncoded;
}

} // namespace ColorPipeline
