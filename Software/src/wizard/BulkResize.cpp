/*
 * BulkResize.cpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#include "BulkResize.hpp"
#include <QtGlobal>

namespace BulkResize {

QRect resizedKeepingAnchor(const QRect& current, int newWidth, int newHeight, Qt::Corner anchor)
{
	const int targetW = (newWidth <= 0) ? current.width() : newWidth;
	const int w = qMax(1, targetW);

	const int targetH = (newHeight <= 0) ? current.height() : newHeight;
	const int h = qMax(1, targetH);

	int x = current.x();
	int y = current.y();

	switch (anchor) {
	case Qt::TopLeftCorner:
		x = current.x();
		y = current.y();
		break;
	case Qt::TopRightCorner:
		x = current.x() + current.width() - w;
		y = current.y();
		break;
	case Qt::BottomLeftCorner:
		x = current.x();
		y = current.y() + current.height() - h;
		break;
	case Qt::BottomRightCorner:
		x = current.x() + current.width() - w;
		y = current.y() + current.height() - h;
		break;
	}

	return QRect(x, y, w, h);
}

} // namespace BulkResize
