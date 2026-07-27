/*
 * LayoutRecipeGenerator.hpp
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

#include <QMap>
#include <QRect>
#include <QJsonObject>

class AreaDistributor;

/*!
	Single implementation of "generate rects from a distributor" (id/order
	math) and of "generate rects from a persisted per-monitor recipe" (see
	docs/plans/presets-aspect-ratio.md Fase 3). ZonePlacementPage::distributeAreas
	and ZoneLayoutRuntime::applyContentAspectPreset both go through this, so
	there is exactly one place that can get the id/offset/invert math wrong.
*/
namespace LayoutRecipeGenerator
{
	// Exhausts `distributor` (areaCount() calls to next()), applying
	// invertOrder/numberingOffset exactly as the wizard does today, then shifts
	// ids by startingLed. Returns an empty map if distributor->areaCount() <= 0.
	QMap<int, QRect> generate(AreaDistributor& distributor, bool invertOrder, int numberingOffset, int startingLed);

	// A single monitor's persisted layout recipe: everything CustomDistributor
	// needs (see Software/src/wizard/CustomDistributor.hpp) plus the base rect
	// it was captured against and the LED id range it owns.
	struct MonitorRecipe {
		int startingLed = 0;
		int topLeds = 0;
		int sideLeds = 0;
		int bottomLeds = 0;
		int thicknessPercent = 15;
		int standWidthPercent = 0;
		bool skipCorners = false;
		bool invertOrder = false;
		int numberingOffset = 0;
		QRect baseRect; // monitor rect (post wizard margins) the recipe was captured against
	};

	// Non-negative counts, total LED count > 0, positive content rect
	// dimensions, thickness/stand-width percentages in [0, 100]. Does not
	// guard against every possible CustomDistributor edge case (a couple of
	// its own division sites are separately clamped in CustomDistributor.cpp),
	// but rejects the inputs that would make the distribution meaningless.
	bool isValid(const MonitorRecipe& recipe, const QRect& contentRect);

	// Builds a CustomDistributor from `recipe` over `contentRect` (NOT
	// recipe.baseRect - callers pass whatever rect they want distributed,
	// e.g. a content-aspect-adjusted rect) and generates LED id -> QRect via
	// generate(). Returns an empty map if !isValid(recipe, contentRect).
	QMap<int, QRect> generate(const MonitorRecipe& recipe, const QRect& contentRect);

	QJsonObject toJson(const MonitorRecipe& recipe);
	MonitorRecipe fromJson(const QJsonObject& obj);
}
