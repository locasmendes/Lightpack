/*
 * BulkResize.hpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#pragma once

#include <QRect>
#include <Qt>

namespace BulkResize {

/*!
 * Calculates a new QRect with updated width and height while keeping the specified corner anchored in place.
 * - newWidth <= 0: keep current width
 * - newHeight <= 0: keep current height
 * - Result width and height will never be less than 1.
 */
QRect resizedKeepingAnchor(const QRect& current, int newWidth, int newHeight, Qt::Corner anchor);

} // namespace BulkResize
