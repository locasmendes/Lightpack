/*
 * ZoneLayoutRuntime.hpp
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

#include <QString>
#include <QCoreApplication>

/*!
	Runtime (outside the wizard) application of content-aspect presets - see
	docs/plans/presets-aspect-ratio.md. The single entry point used by both the
	SettingsWindow UI and the network API, so there is exactly one
	implementation of "switch preset" rather than one per caller.
*/
class ZoneLayoutRuntime
{
	Q_DECLARE_TR_FUNCTIONS(ZoneLayoutRuntime)
public:
	// Regenerates the current profile's LED zone Position/Size from its
	// persisted layout recipe (SettingsScope::Settings::getLayoutRecipe()) for
	// `preset` ("fill"/"16:9"/"4:3") and persists the preset choice. LED ids,
	// IsEnabled and per-LED coefficients are never touched. Returns false
	// (nothing written, preset choice not changed) if the current profile has
	// no layout recipe, or if the recipe turns out to be malformed/unusable.
	static bool applyContentAspectPreset(const QString& preset);

	// Human-readable summary of what applying `preset` would produce right
	// now, e.g. "16:9 - 2560x1440 in 3440x1440", or an explanatory string if
	// there is no layout recipe to apply it to.
	static QString previewText(const QString& preset);
};
