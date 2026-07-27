/*
 * HostColorSmoothing.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 *
 *	Lightpack is very simple implementation of the backlight for a laptop
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "HostColorSmoothing.hpp"
#include <QColor>
#include <QtGlobal>

void HostColorSmoothing::reset(int numberOfLeds)
{
	m_transitionStart = QList<QRgb>(numberOfLeds, qRgb(0, 0, 0));
	m_target = QList<QRgb>(numberOfLeds, qRgb(0, 0, 0));
	m_displayed = QList<QRgb>(numberOfLeds, qRgb(0, 0, 0));
	m_active = false;
}

void HostColorSmoothing::setDurationMs(int durationMs)
{
	m_durationMs = durationMs;
}

void HostColorSmoothing::retarget(const QList<QRgb>& target, qint64 nowMs)
{
	if (m_durationMs <= 0) {
		setDisplayedImmediately(target);
		return;
	}

	if (m_active && m_target == target)
		return; // identical target: keep the clock and endpoints unchanged

	if (m_active) {
		const double t = qBound(0.0, double(nowMs - m_transitionStartMs) / m_durationMs, 1.0);
		applyInterpolation(t); // materialize the current position into m_displayed
	}

	m_transitionStart = m_displayed;
	m_target = target;
	m_transitionStartMs = nowMs;
	m_active = true;
}

void HostColorSmoothing::setDisplayedImmediately(const QList<QRgb>& colors)
{
	m_displayed = colors;
	m_target = colors;
	m_transitionStart = colors;
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
		advance(nowMs); // materialize at now, using the OLD duration, into m_displayed;
		                // may itself already finish the transition (m_active -> false)

	m_durationMs = newDurationMs;

	if (newDurationMs <= 0) {
		setDisplayedImmediately(m_target);
		return;
	}

	// Only restart the clock if a transition is still actually in flight - the
	// materialization above may have just completed it, in which case there is
	// nothing left to retarget and resurrecting it would wrongly extend it.
	if (m_active) {
		m_transitionStart = m_displayed;
		m_transitionStartMs = nowMs;
	}
}

QRgb HostColorSmoothing::interpolate(QRgb start, QRgb target, double t) const
{
	const int r = qRound(qRed(start) + (qRed(target) - qRed(start)) * t);
	const int g = qRound(qGreen(start) + (qGreen(target) - qGreen(start)) * t);
	const int b = qRound(qBlue(start) + (qBlue(target) - qBlue(start)) * t);
	return qRgb(r, g, b);
}

bool HostColorSmoothing::applyInterpolation(double t)
{
	bool changed = false;
	for (int i = 0; i < m_displayed.size(); i++) {
		const QRgb interpolated = interpolate(m_transitionStart[i], m_target[i], t);
		if (m_displayed[i] != interpolated) {
			m_displayed[i] = interpolated;
			changed = true;
		}
	}
	return changed;
}
