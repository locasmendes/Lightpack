/*
 * ColorOps.cpp
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#include "ColorOps.hpp"
#include "PrismatikMath.hpp"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
struct ColorFMetaRegistration {
	ColorFMetaRegistration()
	{
		qRegisterMetaType<LinearRgbF>("LinearRgbF");
		qRegisterMetaType<EncodedRgbF>("EncodedRgbF");
		qRegisterMetaType<WireRgbF>("WireRgbF");
		qRegisterMetaType<QList<LinearRgbF>>("QList<LinearRgbF>");
	}
};
const ColorFMetaRegistration g_colorFMetaRegistration;
} // namespace

namespace ColorOps
{
namespace {

float srgbDecodeChannel(float u)
{
	if (u <= 0.04045f)
		return u / 12.92f;
	return std::pow((u + 0.055f) / 1.055f, 2.4f);
}

float srgbEncodeChannel(float linear)
{
	if (linear <= 0.0031308f)
		return 12.92f * linear;
	return 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
}

// 256-entry LUT for 8-bit → linear (built once).
struct SrgbDecodeLut {
	float v[256];
	SrgbDecodeLut()
	{
		for (int i = 0; i < 256; ++i)
			v[i] = srgbDecodeChannel(i / 255.f);
	}
};

const SrgbDecodeLut &decodeLut()
{
	static const SrgbDecodeLut lut;
	return lut;
}

float channelMax(const EncodedRgbF &c)
{
	return std::max(c.r, std::max(c.g, c.b));
}

float channelMin(const EncodedRgbF &c)
{
	return std::min(c.r, std::min(c.g, c.b));
}

float chroma(const EncodedRgbF &c)
{
	return channelMax(c) - channelMin(c);
}

EncodedRgbF withChroma(const EncodedRgbF &c, float newChroma)
{
	const float maxC = channelMax(c);
	const float cur = chroma(c);
	if (cur <= 0.f)
		return c;
	if (newChroma <= 0.f)
		return EncodedRgbF{ maxC, maxC, maxC };

	const float m = 1.f - newChroma / cur;
	EncodedRgbF out;
	out.r = clamp01(c.r + (maxC - c.r) * m);
	out.g = clamp01(c.g + (maxC - c.g) * m);
	out.b = clamp01(c.b + (maxC - c.b) * m);
	return out;
}

// Float Lab (L 0..100, a/b unbounded) for wire-domain luminosity threshold.
struct LabF { float l, a, b; };

LabF wireToLab(const WireRgbF &w)
{
	StructRgb rgb;
	rgb.r = static_cast<unsigned>(std::lround(clamp01(w.r) * 4095.f));
	rgb.g = static_cast<unsigned>(std::lround(clamp01(w.g) * 4095.f));
	rgb.b = static_cast<unsigned>(std::lround(clamp01(w.b) * 4095.f));
	const StructLab lab = PrismatikMath::toLab(rgb);
	return LabF{ static_cast<float>(lab.l), static_cast<float>(lab.a), static_cast<float>(lab.b) };
}

WireRgbF labToWire(const LabF &lab)
{
	StructLab s;
	s.l = static_cast<unsigned char>(std::lround(std::clamp(lab.l, 0.f, 100.f)));
	s.a = static_cast<char>(std::lround(std::clamp(lab.a, -128.f, 127.f)));
	s.b = static_cast<char>(std::lround(std::clamp(lab.b, -128.f, 127.f)));
	const StructRgb rgb = PrismatikMath::toRgb(s);
	return WireRgbF{ rgb.r / 4095.f, rgb.g / 4095.f, rgb.b / 4095.f };
}

// Peak-normalized linear white point from the legacy encoded table, with K clamp.
StructRgb safeEncodedWhitePoint(quint16 kelvin)
{
	const quint16 k = static_cast<quint16>(std::clamp<int>(kelvin, 1000, 40000));
	StructRgb wp = PrismatikMath::whitePoint(k);
	auto clampByte = [](unsigned &ch) {
		if (ch > 255u)
			ch = 255u;
	};
	clampByte(wp.r);
	clampByte(wp.g);
	clampByte(wp.b);
	return wp;
}

} // namespace

float clamp01(float x)
{
	if (x < 0.f)
		return 0.f;
	if (x > 1.f)
		return 1.f;
	return x;
}

LinearRgbF srgbDecode(QRgb encoded8)
{
	const auto &lut = decodeLut();
	return LinearRgbF{ lut.v[qRed(encoded8)], lut.v[qGreen(encoded8)], lut.v[qBlue(encoded8)] };
}

LinearRgbF srgbDecode(EncodedRgbF encoded)
{
	return LinearRgbF{
		srgbDecodeChannel(encoded.r),
		srgbDecodeChannel(encoded.g),
		srgbDecodeChannel(encoded.b)
	};
}

EncodedRgbF srgbEncode(LinearRgbF linear)
{
	return EncodedRgbF{
		srgbEncodeChannel(linear.r),
		srgbEncodeChannel(linear.g),
		srgbEncodeChannel(linear.b)
	};
}

WireRgbF renderToWire(LinearRgbF linear, float outputGamma)
{
	const float g = (outputGamma <= 0.f) ? 1.f : outputGamma;
	const float inv = 1.f / g;
	auto ch = [inv](float L) {
		return std::pow(clamp01(L), inv);
	};
	return WireRgbF{ ch(linear.r), ch(linear.g), ch(linear.b) };
}

LinearRgbF linearWhitePoint(quint16 kelvin)
{
	const StructRgb wp = safeEncodedWhitePoint(kelvin);
	LinearRgbF lin = srgbDecode(qRgb(static_cast<int>(wp.r), static_cast<int>(wp.g), static_cast<int>(wp.b)));
	const float peak = std::max(lin.r, std::max(lin.g, lin.b));
	if (peak > 0.f) {
		lin.r /= peak;
		lin.g /= peak;
		lin.b /= peak;
	}
	return lin;
}

LinearRgbF avgColor(const QList<LinearRgbF> &colors)
{
	LinearRgbF out;
	if (colors.isEmpty())
		return out;
	double r = 0, g = 0, b = 0;
	for (const LinearRgbF &c : colors) {
		r += c.r;
		g += c.g;
		b += c.b;
	}
	const double n = colors.size();
	out.r = static_cast<float>(r / n);
	out.g = static_cast<float>(g / n);
	out.b = static_cast<float>(b / n);
	return out;
}

EncodedRgbF adjustSaturation(EncodedRgbF c, float factor)
{
	const float newChroma = clamp01(chroma(c) * factor);
	return withChroma(c, newChroma);
}

EncodedRgbF adjustContrast(EncodedRgbF c, float factor, float pivot)
{
	auto ch = [factor, pivot](float x) {
		return clamp01(pivot + (x - pivot) * factor);
	};
	return EncodedRgbF{ ch(c.r), ch(c.g), ch(c.b) };
}

EncodedRgbF adjustVibrance(EncodedRgbF c, float factor, float protectionStrength)
{
	const float cur = chroma(c);
	const float satRatio = cur;
	const float protection = clamp01(protectionStrength);
	const float effective = 1.f + (factor - 1.f) * (1.f - satRatio * protection);
	return withChroma(c, clamp01(cur * effective));
}

EncodedRgbF applyBloom(EncodedRgbF c, int intensity, int whitenessThresholdPercent)
{
	if (intensity <= 0)
		return c;

	const float value = channelMax(c);
	const float ch = chroma(c);
	const float whiteness = 1.f - ch;
	const float threshold = clamp01(whitenessThresholdPercent / 100.f);
	if (whiteness < threshold)
		return c;

	const float range = 1.f - threshold;
	const float whitenessScore = range > 0.f ? (whiteness - threshold) / range : 1.f;
	const float factor = whitenessScore * value;
	if (factor <= 0.f)
		return c;

	const float boost = 1.f + (intensity / 100.f) * factor;
	return EncodedRgbF{ clamp01(c.r * boost), clamp01(c.g * boost), clamp01(c.b * boost) };
}

void applyBrightness(WireRgbF &c, int brightnessPercent)
{
	const float s = brightnessPercent / 100.f;
	c.r *= s;
	c.g *= s;
	c.b *= s;
}

void applyWhiteBalance(WireRgbF &c, float coefR, float coefG, float coefB)
{
	c.r *= coefR;
	c.g *= coefG;
	c.b *= coefB;
}

void applyPowerLimiter(QList<WireRgbF> &colors, const PowerLimiterParams &params)
{
	const float ampCoef = (params.ledMilliAmps > 0.f)
		? params.ledMilliAmps / (1.f * 3.f) / 1000.f
		: 0.f;
	double estimatedTotalAmps = 0.0;

	for (WireRgbF &c : colors) {
		if (params.brightnessCapPercent < 100.f) {
			const float sum = c.r + c.g + c.b;
			const float capSum = params.brightnessCapPercent / 100.f * 3.f;
			if (sum > 0.f) {
				const float bcapFactor = capSum / sum;
				if (bcapFactor < 1.f) {
					c.r *= bcapFactor;
					c.g *= bcapFactor;
					c.b *= bcapFactor;
				}
			}
		}
		estimatedTotalAmps += (static_cast<double>(c.r) + c.g + c.b) * ampCoef;
	}

	if (params.powerSupplyAmps > 0.f && params.powerSupplyAmps < estimatedTotalAmps && estimatedTotalAmps > 0.0) {
		const float powerRatio = static_cast<float>(params.powerSupplyAmps / estimatedTotalAmps);
		for (WireRgbF &c : colors) {
			c.r *= powerRatio;
			c.g *= powerRatio;
			c.b *= powerRatio;
		}
	}
}

void applyLuminosityThreshold(QList<WireRgbF> &colors, int luminosityThreshold,
	bool minimumLuminosityEnabled)
{
	if (colors.isEmpty())
		return;

	// Average in wire → Lab (same order as legacy).
	WireRgbF avgWire;
	{
		double r = 0, g = 0, b = 0;
		for (const WireRgbF &c : colors) {
			r += c.r; g += c.g; b += c.b;
		}
		const double n = colors.size();
		avgWire = WireRgbF{ static_cast<float>(r / n), static_cast<float>(g / n), static_cast<float>(b / n) };
	}
	const LabF avgLab = wireToLab(avgWire);

	for (WireRgbF &c : colors) {
		LabF lab = wireToLab(c);
		const float dl = luminosityThreshold - lab.l;
		if (dl > 0.f) {
			if (minimumLuminosityEnabled) {
				constexpr float kFadingRange = 5.f;
				const float fadingCoeff = dl < kFadingRange
					? (dl - kFadingRange) * (dl - kFadingRange) / (kFadingRange * kFadingRange)
					: 1.f;
				const float da = avgLab.a - lab.a;
				const float db = avgLab.b - lab.b;
				lab.l = static_cast<float>(luminosityThreshold);
				lab.a += da * fadingCoeff;
				lab.b += db * fadingCoeff;
				c = labToWire(lab);
			} else {
				c = WireRgbF{};
			}
		}
	}
}

quint16 quantize(float wire01, int bitDepth)
{
	const int maxOut = (1 << bitDepth) - 1;
	const float x = clamp01(wire01);
	return static_cast<quint16>(std::lround(x * maxOut));
}

void quantize(const WireRgbF &c, int bitDepth, StructRgb &out)
{
	out.r = quantize(c.r, bitDepth);
	out.g = quantize(c.g, bitDepth);
	out.b = quantize(c.b, bitDepth);
}

void applyDeviceStage(const QList<LinearRgbF> &linearIn, QList<StructRgb> &out12,
	const DeviceStageParams &params)
{
	out12.resize(linearIn.size());
	if (linearIn.isEmpty())
		return;

	QList<WireRgbF> wire;
	wire.reserve(linearIn.size());
	for (const LinearRgbF &L : linearIn)
		wire.append(renderToWire(L, params.outputGamma));

	applyLuminosityThreshold(wire, params.luminosityThreshold, params.minimumLuminosityEnabled);

	const int wbCount = std::min(params.wb.size(), wire.size());
	if (!params.wb.isEmpty() && params.wb.size() != wire.size()) {
		static bool s_warnedWbMismatch = false;
		if (!s_warnedWbMismatch) {
			qWarning() << "ColorOps::applyDeviceStage: WB coef count" << params.wb.size()
				<< "!= LED count" << wire.size() << "; applying min(count)";
			s_warnedWbMismatch = true;
		}
	}

	for (int i = 0; i < wire.size(); ++i) {
		applyBrightness(wire[i], params.brightnessPercent);
		if (i < wbCount)
			applyWhiteBalance(wire[i], params.wb[i].r, params.wb[i].g, params.wb[i].b);
	}

	PowerLimiterParams pl;
	pl.brightnessCapPercent = params.brightnessCapPercent;
	pl.ledMilliAmps = params.ledMilliAmps;
	pl.powerSupplyAmps = params.powerSupplyAmps;
	applyPowerLimiter(wire, pl);

	for (int i = 0; i < wire.size(); ++i)
		quantize(wire[i], 12, out12[i]);
}

void applyDeviceStageFromEncoded(const QList<QRgb> &encodedIn, QList<StructRgb> &out12,
	const DeviceStageParams &params)
{
	QList<LinearRgbF> linear;
	linear.reserve(encodedIn.size());
	for (QRgb c : encodedIn)
		linear.append(srgbDecode(c));
	applyDeviceStage(linear, out12, params);
}

void quantizeDithered(QList<WireRgbF> &colors, int bitDepth, QList<StructRgb> &out)
{
	out.resize(colors.size());
	const int maxOut = (1 << bitDepth) - 1;
	const double k = 1.0 / maxOut; // wire step per code

	double carryR = 0, carryG = 0, carryB = 0;
	double meanIndR = 0, meanIndG = 0, meanIndB = 0;

	for (int i = 0; i < colors.size(); ++i) {
		const double tempR = colors[i].r;
		const double tempG = colors[i].g;
		const double tempB = colors[i].b;

		out[i].r = static_cast<unsigned>(std::lround(clamp01(colors[i].r) * maxOut));
		out[i].g = static_cast<unsigned>(std::lround(clamp01(colors[i].g) * maxOut));
		out[i].b = static_cast<unsigned>(std::lround(clamp01(colors[i].b) * maxOut));

		// Keep codes in range after possible dither increments.
		out[i].r = std::min(out[i].r, static_cast<unsigned>(maxOut));
		out[i].g = std::min(out[i].g, static_cast<unsigned>(maxOut));
		out[i].b = std::min(out[i].b, static_cast<unsigned>(maxOut));

		carryR += tempR - out[i].r * k;
		carryG += tempG - out[i].g * k;
		carryB += tempB - out[i].b * k;

		meanIndR += (tempR - out[i].r * k) * i;
		meanIndG += (tempG - out[i].g * k) * i;
		meanIndB += (tempB - out[i].b * k) * i;

		if (carryR >= k) {
			const int ind = static_cast<int>((meanIndR - (carryR - k) * i) / k);
			if (ind >= 0 && ind < out.size() && out[ind].r < static_cast<unsigned>(maxOut))
				out[ind].r++;
			carryR -= k;
			meanIndR = carryR * i;
		}
		if (carryG >= k) {
			const int ind = static_cast<int>((meanIndG - (carryG - k) * i) / k);
			if (ind >= 0 && ind < out.size() && out[ind].g < static_cast<unsigned>(maxOut))
				out[ind].g++;
			carryG -= k;
			meanIndG = carryG * i;
		}
		if (carryB >= k) {
			const int ind = static_cast<int>((meanIndB - (carryB - k) * i) / k);
			if (ind >= 0 && ind < out.size() && out[ind].b < static_cast<unsigned>(maxOut))
				out[ind].b++;
			carryB -= k;
			meanIndB = carryB * i;
		}
	}
}

} // namespace ColorOps
