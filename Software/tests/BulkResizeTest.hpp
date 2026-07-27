/*
 * BulkResizeTest.hpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#pragma once

#include <QObject>

class BulkResizeTest : public QObject
{
	Q_OBJECT
public:
	explicit BulkResizeTest(QObject *parent = nullptr);

private slots:
	void testResizedKeepingAnchorTopLeft();
	void testResizedKeepingAnchorTopRight();
	void testResizedKeepingAnchorBottomLeft();
	void testResizedKeepingAnchorBottomRight();
	void testResizedKeepingAnchorDisabledAxis();
	void testResizedKeepingAnchorMinimumDimensions();
};
