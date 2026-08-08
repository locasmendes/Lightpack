/*
 * ColorPipeline.hpp — content stage (B1–B4) as a pure function over ContentParams.
 *
 * No QObject, no signals, does not read Settings.
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#pragma once

#include "ColorF.h"
#include <QList>
#include <QRgb>

namespace ColorPipeline
{

/*! Hysteresis thresholds in encoded domain (§2.9 R14 / §2.5 C1). */
constexpr float kChangeEnterEncoded = 1.5f / 255.f;
constexpr float kChangeExitEncoded  = 0.5f / 255.f;

struct ContentParams {
	bool avgColorsOnAllLeds = false;
	/*! Empty = all LEDs participate in B1; otherwise per-index enable mask. */
	QList<bool> areaEnabled;

	bool applyColorTemperature = false;
	bool applyBlueLightReduction = false;
	quint16 colorTemperatureK = 6500;
	/*! When blue-light reduction is on, Kelvin from the OS client (0 = skip). */
	quint16 blueLightKelvin = 0;

	int saturation = 50;   // 0..100, 50 = identity
	int contrast = 50;
	int vibrance = 50;
	int contrastPivot = 128; // 0..255
	int vibranceProtection = 60; // percent
	bool bloomEnabled = false;
	int bloomIntensity = 50;
	int bloomThreshold = 70; // whiteness percent
	int overBrighten = 0; // 0 = off; linear exposure gain
};

/*! Decode each QRgb → LinearRgbF, then run B1–B4. */
QList<LinearRgbF> processContent(const QList<QRgb> &encodedIn, const ContentParams &params);

/*! B1–B4 on already-linear colors (skips source decode). */
QList<LinearRgbF> processContentLinear(const QList<LinearRgbF> &linearIn, const ContentParams &params);

/*! True if any channel differs by more than enter threshold (encoded domain). */
bool colorsChangedEncoded(const EncodedRgbF &a, const EncodedRgbF &b, float enterThreshold = kChangeEnterEncoded);

/*! Hysteresis: once "changed", stay until below exit threshold. */
bool updateChangeHysteresis(bool currentlyChanged, const EncodedRgbF &prev, const EncodedRgbF &next);

} // namespace ColorPipeline
