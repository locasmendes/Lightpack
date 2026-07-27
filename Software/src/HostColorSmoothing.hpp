/*
 * HostColorSmoothing.hpp
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

#pragma once

#include <QList>
#include <QRgb>

/*!
	Generic host-side linear color transition engine, used by GrabManager to smooth
	updates for devices without their own firmware smoothing (see
	docs/plans/smoothing-host-side.md). Deliberately dependency-free (no QTimer/
	QElapsedTimer): callers supply an arbitrary monotonic "now" in milliseconds, which
	keeps this class fully unit-testable with known values instead of real clocks.
*/
class HostColorSmoothing
{
public:
	// Resizes/clears all internal color arrays to numberOfLeds (all black) and cancels
	// any active transition. Must be called before retarget()/advance() are used with a
	// given LED count, and again whenever that count changes (profile switch, resize).
	void reset(int numberOfLeds);

	void setDurationMs(int durationMs);
	int durationMs() const { return m_durationMs; }

	bool isActive() const { return m_active; }

	// Adopts `target` as the new color to transition towards. If a transition is
	// already active, first materializes the color currently displayed at `nowMs` and
	// uses it as the new transition's starting point (no jump/discontinuity); the full
	// duration restarts from `nowMs`. If durationMs() == 0, instead syncs displayed
	// colors straight to `target` and leaves no transition active (immediate bypass).
	// `target` must be the same size as the last reset().
	void retarget(const QList<QRgb>& target, qint64 nowMs);

	// Immediately sets the displayed colors to `colors` and cancels any active
	// transition, without going through retarget()'s materialization step. Used when
	// host smoothing is not applicable (Lightpack device, or duration == 0 and no
	// transition to materialize) so the caller can emit displayedColors() unchanged.
	void setDisplayedImmediately(const QList<QRgb>& colors);

	// Advances an active transition to `nowMs`; a no-op if !isActive(). Returns true if
	// displayedColors() changed value since the previous advance()/retarget() call.
	// Stops the transition (isActive() becomes false) once nowMs reaches the end of the
	// duration, leaving displayedColors() exactly equal to the target.
	bool advance(qint64 nowMs);

	// Changes the duration while keeping the same target: if a transition is active,
	// materializes the current position (with the OLD duration) and restarts the clock
	// towards the unchanged target with the NEW duration - no discontinuity. If
	// newDurationMs <= 0, instead finishes immediately at the target. If no transition
	// is active, only updates durationMs() for the next retarget().
	void changeDurationAndRetarget(int newDurationMs, qint64 nowMs);

	const QList<QRgb>& displayedColors() const { return m_displayed; }

private:
	QRgb interpolate(QRgb start, QRgb target, double t) const;
	// Writes the interpolated colors for `t` into m_displayed; returns true if any value changed.
	bool applyInterpolation(double t);

	int m_durationMs = 0;
	bool m_active = false;
	qint64 m_transitionStartMs = 0;

	QList<QRgb> m_transitionStart;
	QList<QRgb> m_target;
	QList<QRgb> m_displayed;
};
