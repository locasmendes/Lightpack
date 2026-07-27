/*
 * ContentAspectPreset.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Prismatik
 *
 *	Lightpack is an open-source, USB content-driving ambient lighting
 *	hardware.
 *
 *	Prismatik is a free, open-source software: you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Prismatik and Lightpack files is distributed in the hope that it will be
 *	useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <QRect>
#include <QString>
#include <QStringList>

/*!
	Single implementation of the "content aspect" rule from
	docs/plans/presets-aspect-ratio.md: given a monitor's global QRect and a
	preset, returns the (possibly smaller, pillar/letterboxed) rect the LED
	layout should be redistributed over. Pure/stateless - no Settings, no
	widgets, no wizard dependency - so it is trivially unit-testable.
*/
namespace ContentAspectPreset
{
	extern const QString Fill;
	extern const QString Ratio16x9;
	extern const QString Ratio4x3;

	// {Fill, Ratio16x9, Ratio4x3}, in display order.
	QStringList validPresets();

	// Maps an arbitrary/persisted string to one of validPresets(), defaulting
	// unknown/empty input to Fill rather than rejecting it - callers reading a
	// possibly-stale or hand-edited Settings value should always get a usable
	// preset back.
	QString normalize(const QString& preset);

	// Computes the content rect for `preset` inside `monitor`, preserving
	// monitor's own origin (important on multi-monitor desktops where
	// QScreen::geometry() is not zero-based). For Fill, or when monitor's
	// aspect ratio already matches the preset, returns `monitor` unchanged -
	// no centering offset at all. Otherwise centers a same-aspect rect inside
	// monitor: pillarboxed (full height, narrower width) if monitor is wider
	// than the target ratio, letterboxed (full width, shorter height) if
	// monitor is narrower. Leftover pixels split via integer division, so the
	// two opposing margins never differ by more than one pixel.
	QRect contentRect(const QRect& monitor, const QString& preset);
}
