/*
 * ColorOps.hpp — pure color math for the unified float pipeline.
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#pragma once

#include "ColorF.h"
#include "colorspace_types.h"
#include <QList>
#include <QRgb>
#include <QtGlobal>

namespace ColorOps
{

// --- Transfer functions (the only two TF families in the codebase) ---

LinearRgbF srgbDecode(QRgb encoded8);
LinearRgbF srgbDecode(EncodedRgbF encoded);
EncodedRgbF srgbEncode(LinearRgbF linear);

/*! w = clamp01(L)^(1/outputGamma). outputGamma 1.0 is physically linear. */
WireRgbF renderToWire(LinearRgbF linear, float outputGamma);

// --- White point (linear, peak-normalized) ---

/*! Decoded white-point coefficients with max(component) == 1. Fixes legacy >255 / log(0). */
LinearRgbF linearWhitePoint(quint16 kelvin);

// --- Content helpers ---

LinearRgbF avgColor(const QList<LinearRgbF> &colors);

// --- Perceptual operators on EncodedRgbF (identity at default factors) ---

EncodedRgbF adjustSaturation(EncodedRgbF c, float factor);
EncodedRgbF adjustContrast(EncodedRgbF c, float factor, float pivot = 128.f / 255.f);
EncodedRgbF adjustVibrance(EncodedRgbF c, float factor, float protectionStrength = 0.6f);
EncodedRgbF applyBloom(EncodedRgbF c, int intensity, int whitenessThresholdPercent);

// --- Device-stage helpers on WireRgbF ---

struct PowerLimiterParams {
	float brightnessCapPercent = 100.f; // 100 = no-op
	float ledMilliAmps = 0.f;
	float powerSupplyAmps = 0.f; // 0 = unlimited
};

/*! One-pass cap + PSU. Never scales up. Equivalent to the legacy two-pass formula. */
void applyPowerLimiter(QList<WireRgbF> &colors, const PowerLimiterParams &params);

/*! Lab luminosity floor / dead-zone on wire, float a/b (no char overflow). */
void applyLuminosityThreshold(QList<WireRgbF> &colors, int luminosityThreshold,
	bool minimumLuminosityEnabled);

void applyBrightness(WireRgbF &c, int brightnessPercent);
void applyWhiteBalance(WireRgbF &c, float coefR, float coefG, float coefB);

/*! Round-half-up quantize to N-bit codes (N typically 8 or 12). */
quint16 quantize(float wire01, int bitDepth);
void quantize(const WireRgbF &c, int bitDepth, StructRgb &out);

/*! Error-diffusion dither across a strip; conserves sum within ~1 code/channel. */
void quantizeDithered(QList<WireRgbF> &colors, int bitDepth, QList<StructRgb> &out);

/*! Optional per-LED white-balance coefficients (1.0 = neutral). Empty = skip WB. */
struct WhiteBalanceCoef {
	float r = 1.f;
	float g = 1.f;
	float b = 1.f;
};

struct DeviceStageParams {
	float outputGamma = 1.10f;
	int brightnessPercent = 100;
	float brightnessCapPercent = 100.f;
	int luminosityThreshold = 3;
	bool minimumLuminosityEnabled = true;
	float ledMilliAmps = 20.f;
	float powerSupplyAmps = 0.f; // 0 = unlimited
	QList<WhiteBalanceCoef> wb; // size must match input to apply
};

/*! D1–D5 on linear input, then quantize to 12-bit StructRgb (legacy wire buffer scale). */
void applyDeviceStage(const QList<LinearRgbF> &linearIn, QList<StructRgb> &out12,
	const DeviceStageParams &params);

/*! Decode 8-bit encoded colors then applyDeviceStage (step-5 bridge before float transport). */
void applyDeviceStageFromEncoded(const QList<QRgb> &encodedIn, QList<StructRgb> &out12,
	const DeviceStageParams &params);

float clamp01(float x);

} // namespace ColorOps
