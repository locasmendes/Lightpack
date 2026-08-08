/*
 * HostColorSmoothing.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#include "HostColorSmoothing.hpp"
#include "ColorOps.hpp"
#include "ColorPipeline.hpp"
#include <QtGlobal>
#include <cmath>

namespace {

EncodedRgbF toEncoded(const LinearRgbF &L)
{
	return ColorOps::srgbEncode(L);
}

LinearRgbF toLinear(const EncodedRgbF &e)
{
	return ColorOps::srgbDecode(e);
}

} // namespace

void HostColorSmoothing::reset(int numberOfLeds)
{
	m_transitionStart = QList<EncodedRgbF>(numberOfLeds);
	m_target = QList<EncodedRgbF>(numberOfLeds);
	m_displayed = QList<EncodedRgbF>(numberOfLeds);
	m_displayedLinear = QList<LinearRgbF>(numberOfLeds);
	m_active = false;
}

void HostColorSmoothing::setDurationMs(int durationMs)
{
	m_durationMs = durationMs;
}

bool HostColorSmoothing::targetsNearlyEqual(const QList<EncodedRgbF>& a, const QList<EncodedRgbF>& b) const
{
	if (a.size() != b.size())
		return false;
	for (int i = 0; i < a.size(); ++i) {
		const float d = std::max({
			std::fabs(a[i].r - b[i].r),
			std::fabs(a[i].g - b[i].g),
			std::fabs(a[i].b - b[i].b)});
		if (d >= ColorPipeline::kChangeExitEncoded)
			return false;
	}
	return true;
}

void HostColorSmoothing::syncLinearFromEncoded()
{
	m_displayedLinear.resize(m_displayed.size());
	for (int i = 0; i < m_displayed.size(); ++i)
		m_displayedLinear[i] = toLinear(m_displayed[i]);
}

void HostColorSmoothing::retarget(const QList<LinearRgbF>& target, qint64 nowMs)
{
	QList<EncodedRgbF> targetEnc;
	targetEnc.reserve(target.size());
	for (const LinearRgbF &L : target)
		targetEnc.append(toEncoded(L));

	if (m_durationMs <= 0) {
		setDisplayedImmediately(target);
		return;
	}

	if (m_active && targetsNearlyEqual(m_target, targetEnc))
		return;

	if (m_active) {
		const double t = qBound(0.0, double(nowMs - m_transitionStartMs) / m_durationMs, 1.0);
		applyInterpolation(t);
	}

	m_transitionStart = m_displayed;
	m_target = targetEnc;
	m_transitionStartMs = nowMs;
	m_active = true;
}

void HostColorSmoothing::setDisplayedImmediately(const QList<LinearRgbF>& colors)
{
	m_displayed.resize(colors.size());
	for (int i = 0; i < colors.size(); ++i)
		m_displayed[i] = toEncoded(colors[i]);
	m_target = m_displayed;
	m_transitionStart = m_displayed;
	m_displayedLinear = colors;
	m_active = false;
}

bool HostColorSmoothing::advance(qint64 nowMs)
{
	if (!m_active)
		return false;

	const double t = m_durationMs > 0
		? qBound(0.0, double(nowMs - m_transitionStartMs) / m_durationMs, 1.0)
		: 1.0;

	const bool changed = applyInterpolation(t);

	if (t >= 1.0)
		m_active = false;

	return changed;
}

void HostColorSmoothing::changeDurationAndRetarget(int newDurationMs, qint64 nowMs)
{
	if (m_active)
		advance(nowMs);

	m_durationMs = newDurationMs;

	if (newDurationMs <= 0) {
		m_displayed = m_target;
		syncLinearFromEncoded();
		m_transitionStart = m_displayed;
		m_active = false;
		return;
	}

	if (m_active) {
		m_transitionStart = m_displayed;
		m_transitionStartMs = nowMs;
	}
}

EncodedRgbF HostColorSmoothing::interpolateEncoded(const EncodedRgbF& start, const EncodedRgbF& target, double t) const
{
	return EncodedRgbF{
		static_cast<float>(start.r + (target.r - start.r) * t),
		static_cast<float>(start.g + (target.g - start.g) * t),
		static_cast<float>(start.b + (target.b - start.b) * t)};
}

bool HostColorSmoothing::applyInterpolation(double t)
{
	bool changed = false;
	for (int i = 0; i < m_displayed.size(); i++) {
		const EncodedRgbF interpolated = interpolateEncoded(m_transitionStart[i], m_target[i], t);
		const float d = std::max({
			std::fabs(m_displayed[i].r - interpolated.r),
			std::fabs(m_displayed[i].g - interpolated.g),
			std::fabs(m_displayed[i].b - interpolated.b)});
		if (d > 1e-8f) {
			m_displayed[i] = interpolated;
			changed = true;
		}
	}
	if (changed)
		syncLinearFromEncoded();
	return changed;
}
