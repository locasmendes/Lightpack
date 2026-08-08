/*
 * CalibrationPatterns.hpp — pure host-side test patterns for Phase 3 calibration.
 *
 * Generated colors are sRGB-encoded 8-bit (QRgb), sent to the device via the
 * Mood Lamp path (bypass grabber).
 */

#pragma once

#include <QList>
#include <QRgb>
#include <QtGlobal>

namespace CalibrationPatterns
{

enum class PatternId {
	White100 = 0,
	White75,
	White50,
	White25,
	Red,
	Green,
	Blue,
	Cyan,
	Magenta,
	Yellow,
	GrayRamp,
	ColorBars,
	ChaseIdentify
};

/*! Pure: (pattern, ledIndex, ledCount[, chaseIndex]) → color for that LED. */
QRgb colorForLed(PatternId pattern, int ledIndex, int ledCount, int chaseIndex = 0);

/*! Build a full strip. chaseIndex only used for ChaseIdentify. */
QList<QRgb> generate(PatternId pattern, int ledCount, int chaseIndex = 0);

} // namespace CalibrationPatterns
