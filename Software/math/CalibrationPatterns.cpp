/*
 * CalibrationPatterns.cpp
 */

#include "CalibrationPatterns.hpp"
#include <algorithm>
#include <cmath>

namespace CalibrationPatterns
{
namespace {

int clampByte(int v)
{
	return std::max(0, std::min(255, v));
}

QRgb gray(int level)
{
	level = clampByte(level);
	return qRgb(level, level, level);
}

} // namespace

QRgb colorForLed(PatternId pattern, int ledIndex, int ledCount, int chaseIndex)
{
	Q_UNUSED(ledIndex);
	switch (pattern) {
	case PatternId::White100: return qRgb(255, 255, 255);
	case PatternId::White75:  return gray(qRound(255 * 0.75));
	case PatternId::White50:  return gray(128);
	case PatternId::White25:  return gray(qRound(255 * 0.25));
	case PatternId::Red:      return qRgb(255, 0, 0);
	case PatternId::Green:    return qRgb(0, 255, 0);
	case PatternId::Blue:     return qRgb(0, 0, 255);
	case PatternId::Cyan:     return qRgb(0, 255, 255);
	case PatternId::Magenta:  return qRgb(255, 0, 255);
	case PatternId::Yellow:   return qRgb(255, 255, 0);
	case PatternId::GrayRamp: {
		if (ledCount <= 1)
			return qRgb(255, 255, 255);
		const int level = qRound(255.0 * ledIndex / double(ledCount - 1));
		return gray(level);
	}
	case PatternId::ColorBars: {
		static const QRgb kBars[] = {
			qRgb(255, 0, 0),
			qRgb(0, 255, 0),
			qRgb(0, 0, 255),
			qRgb(0, 255, 255),
			qRgb(255, 0, 255),
			qRgb(255, 255, 0),
		};
		const int idx = ledIndex < 0 ? 0 : (ledIndex % 6);
		return kBars[idx];
	}
	case PatternId::ChaseIdentify:
		return (ledIndex == chaseIndex) ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
	}
	return qRgb(0, 0, 0);
}

QList<QRgb> generate(PatternId pattern, int ledCount, int chaseIndex)
{
	QList<QRgb> out;
	if (ledCount <= 0)
		return out;
	out.reserve(ledCount);
	for (int i = 0; i < ledCount; ++i)
		out.append(colorForLed(pattern, i, ledCount, chaseIndex));
	return out;
}

} // namespace CalibrationPatterns
