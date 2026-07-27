/*
 * LayoutRecipeGenerator.cpp
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

#include "LayoutRecipeGenerator.hpp"
#include "AreaDistributor.hpp"
#include "CustomDistributor.hpp"

namespace LayoutRecipeGenerator
{
	QMap<int, QRect> generate(AreaDistributor& distributor, bool invertOrder, int numberingOffset, int startingLed)
	{
		QMap<int, QRect> result;
		const int areaCount = distributor.areaCount();
		if (areaCount <= 0)
			return result;

		for (int i = 0; i < areaCount; i++) {
			const ScreenArea* const sf = distributor.next();
			const QRect r(sf->hScanStart(), sf->vScanStart(),
				sf->hScanEnd() - sf->hScanStart(), sf->vScanEnd() - sf->vScanStart());
			delete sf;

			int id = ((invertOrder ? areaCount - (i + 1) : i) + numberingOffset) % areaCount;
			id = (id + areaCount) % areaCount;
			result.insert(id + startingLed, r);
		}
		return result;
	}

	bool isValid(const MonitorRecipe& recipe, const QRect& contentRect)
	{
		if (recipe.topLeds < 0 || recipe.sideLeds < 0 || recipe.bottomLeds < 0)
			return false;
		if (recipe.topLeds + 2 * recipe.sideLeds + recipe.bottomLeds <= 0)
			return false;
		if (contentRect.width() <= 0 || contentRect.height() <= 0)
			return false;
		if (recipe.thicknessPercent < 0 || recipe.thicknessPercent > 100)
			return false;
		if (recipe.standWidthPercent < 0 || recipe.standWidthPercent > 100)
			return false;
		return true;
	}

	QMap<int, QRect> generate(const MonitorRecipe& recipe, const QRect& contentRect)
	{
		if (!isValid(recipe, contentRect))
			return QMap<int, QRect>();

		CustomDistributor distributor(
			contentRect,
			recipe.topLeds,
			recipe.sideLeds,
			recipe.bottomLeds,
			recipe.thicknessPercent / 100.0,
			recipe.standWidthPercent / 100.0,
			recipe.skipCorners
		);

		return generate(distributor, recipe.invertOrder, recipe.numberingOffset, recipe.startingLed);
	}

	QJsonObject toJson(const MonitorRecipe& recipe)
	{
		QJsonObject obj;
		obj["startingLed"] = recipe.startingLed;
		obj["topLeds"] = recipe.topLeds;
		obj["sideLeds"] = recipe.sideLeds;
		obj["bottomLeds"] = recipe.bottomLeds;
		obj["thicknessPercent"] = recipe.thicknessPercent;
		obj["standWidthPercent"] = recipe.standWidthPercent;
		obj["skipCorners"] = recipe.skipCorners;
		obj["invertOrder"] = recipe.invertOrder;
		obj["numberingOffset"] = recipe.numberingOffset;

		QJsonObject rect;
		rect["x"] = recipe.baseRect.x();
		rect["y"] = recipe.baseRect.y();
		rect["width"] = recipe.baseRect.width();
		rect["height"] = recipe.baseRect.height();
		obj["baseRect"] = rect;

		return obj;
	}

	MonitorRecipe fromJson(const QJsonObject& obj)
	{
		MonitorRecipe recipe;
		recipe.startingLed = obj["startingLed"].toInt();
		recipe.topLeds = obj["topLeds"].toInt();
		recipe.sideLeds = obj["sideLeds"].toInt();
		recipe.bottomLeds = obj["bottomLeds"].toInt();
		recipe.thicknessPercent = obj["thicknessPercent"].toInt(15);
		recipe.standWidthPercent = obj["standWidthPercent"].toInt(0);
		recipe.skipCorners = obj["skipCorners"].toBool();
		recipe.invertOrder = obj["invertOrder"].toBool();
		recipe.numberingOffset = obj["numberingOffset"].toInt();

		const QJsonObject rect = obj["baseRect"].toObject();
		recipe.baseRect = QRect(rect["x"].toInt(), rect["y"].toInt(), rect["width"].toInt(), rect["height"].toInt());

		return recipe;
	}
}
