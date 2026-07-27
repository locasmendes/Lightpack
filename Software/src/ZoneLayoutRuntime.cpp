/*
 * ZoneLayoutRuntime.cpp
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

#include "ZoneLayoutRuntime.hpp"
#include "Settings.hpp"
#include "wizard/ContentAspectPreset.hpp"
#include "wizard/LayoutRecipeGenerator.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRect>

using namespace SettingsScope;

bool ZoneLayoutRuntime::applyContentAspectPreset(const QString& preset)
{
	if (!Settings::hasLayoutRecipe())
		return false;

	const QString normalized = ContentAspectPreset::normalize(preset);
	const QJsonArray recipe = Settings::getLayoutRecipe();

	// Generate every monitor's rects first, and only write to Settings if all
	// of them succeed - a malformed recipe entry must not leave the profile
	// with some zones regenerated and others untouched.
	QMap<int, QRect> allRects;
	for (const QJsonValue& entry : recipe) {
		const LayoutRecipeGenerator::MonitorRecipe monitorRecipe = LayoutRecipeGenerator::fromJson(entry.toObject());
		const QRect contentRect = ContentAspectPreset::contentRect(monitorRecipe.baseRect, normalized);
		const QMap<int, QRect> rects = LayoutRecipeGenerator::generate(monitorRecipe, contentRect);
		if (rects.isEmpty())
			return false;

		for (auto it = rects.constBegin(); it != rects.constEnd(); ++it)
			allRects.insert(it.key(), it.value());
	}

	if (allRects.isEmpty())
		return false;

	for (auto it = allRects.constBegin(); it != allRects.constEnd(); ++it) {
		Settings::setLedPosition(it.key(), it.value().topLeft());
		Settings::setLedSize(it.key(), it.value().size());
	}

	Settings::setContentAspectPreset(normalized);
	return true;
}

QString ZoneLayoutRuntime::previewText(const QString& preset)
{
	const QJsonArray recipe = Settings::getLayoutRecipe();
	if (recipe.isEmpty())
		return tr("No layout recipe yet — run the setup wizard first.");

	const LayoutRecipeGenerator::MonitorRecipe monitorRecipe = LayoutRecipeGenerator::fromJson(recipe.first().toObject());
	const QString normalized = ContentAspectPreset::normalize(preset);
	const QRect contentRect = ContentAspectPreset::contentRect(monitorRecipe.baseRect, normalized);

	return tr("%1 — %2×%3 in %4×%5")
		.arg(normalized)
		.arg(contentRect.width())
		.arg(contentRect.height())
		.arg(monitorRecipe.baseRect.width())
		.arg(monitorRecipe.baseRect.height());
}
