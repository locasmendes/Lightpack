/*
 * HostColorSmoothing.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 *
 *	Interpolates in EncodedRgbF (perceptual); I/O is LinearRgbF (§2.6 C2).
 */

#pragma once

#include "ColorF.h"
#include <QList>

class HostColorSmoothing
{
public:
	void reset(int numberOfLeds);

	void setDurationMs(int durationMs);
	int durationMs() const { return m_durationMs; }

	bool isActive() const { return m_active; }

	void retarget(const QList<LinearRgbF>& target, qint64 nowMs);
	void setDisplayedImmediately(const QList<LinearRgbF>& colors);
	bool advance(qint64 nowMs);
	void changeDurationAndRetarget(int newDurationMs, qint64 nowMs);

	const QList<LinearRgbF>& displayedColors() const { return m_displayedLinear; }

private:
	EncodedRgbF interpolateEncoded(const EncodedRgbF& start, const EncodedRgbF& target, double t) const;
	bool applyInterpolation(double t);
	bool targetsNearlyEqual(const QList<EncodedRgbF>& a, const QList<EncodedRgbF>& b) const;
	void syncLinearFromEncoded();

	int m_durationMs = 0;
	bool m_active = false;
	qint64 m_transitionStartMs = 0;

	QList<EncodedRgbF> m_transitionStart;
	QList<EncodedRgbF> m_target;
	QList<EncodedRgbF> m_displayed;
	QList<LinearRgbF> m_displayedLinear;
};
