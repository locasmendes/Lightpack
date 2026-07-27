/*
 * ContentAspectPreset.cpp
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

#include "ContentAspectPreset.hpp"
#include <QtGlobal>
#include <cmath>

namespace ContentAspectPreset
{
	const QString Fill = QStringLiteral("fill");
	const QString Ratio16x9 = QStringLiteral("16:9");
	const QString Ratio4x3 = QStringLiteral("4:3");

	QStringList validPresets()
	{
		return { Fill, Ratio16x9, Ratio4x3 };
	}

	QString normalize(const QString& preset)
	{
		return validPresets().contains(preset) ? preset : Fill;
	}

	namespace
	{
		// Target width/height ratio for a preset, or false for Fill/unknown.
		bool ratioFor(const QString& preset, double& outRatio)
		{
			if (preset == Ratio16x9) { outRatio = 16.0 / 9.0; return true; }
			if (preset == Ratio4x3) { outRatio = 4.0 / 3.0; return true; }
			return false;
		}
	}

	QRect contentRect(const QRect& monitor, const QString& preset)
	{
		double ratio = 0.0;
		if (!ratioFor(normalize(preset), ratio))
			return monitor;

		if (monitor.width() <= 0 || monitor.height() <= 0)
			return monitor;

		const double monitorRatio = (double)monitor.width() / monitor.height();

		// Exact (or near-exact) match: no centering offset at all.
		if (std::abs(monitorRatio - ratio) < 1e-9)
			return monitor;

		if (monitorRatio > ratio) {
			// Monitor wider than target: full height, pillarbox the sides.
			const int height = monitor.height();
			const int width = qMax(1, (int)std::floor(height * ratio));
			const int left = monitor.left() + (monitor.width() - width) / 2;
			return QRect(left, monitor.top(), width, height);
		} else {
			// Monitor narrower than target: full width, letterbox top/bottom.
			const int width = monitor.width();
			const int height = qMax(1, (int)std::floor(width / ratio));
			const int top = monitor.top() + (monitor.height() - height) / 2;
			return QRect(monitor.left(), top, width, height);
		}
	}
}
