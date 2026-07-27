/*
 * BulkResizeTest.cpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#include "BulkResizeTest.hpp"
#include "wizard/BulkResize.hpp"
#include <QtTest>

BulkResizeTest::BulkResizeTest(QObject *parent) :
	QObject(parent)
{
}

void BulkResizeTest::testResizedKeepingAnchorTopLeft()
{
	const QRect initial(100, 200, 50, 60);
	const QRect result = BulkResize::resizedKeepingAnchor(initial, 80, 90, Qt::TopLeftCorner);

	QCOMPARE(result.x(), 100);
	QCOMPARE(result.y(), 200);
	QCOMPARE(result.width(), 80);
	QCOMPARE(result.height(), 90);
}

void BulkResizeTest::testResizedKeepingAnchorTopRight()
{
	const QRect initial(100, 200, 50, 60); // right edge is 100 + 50 = 150
	const QRect result = BulkResize::resizedKeepingAnchor(initial, 80, 90, Qt::TopRightCorner);

	QCOMPARE(result.x() + result.width(), 150);
	QCOMPARE(result.y(), 200);
	QCOMPARE(result.width(), 80);
	QCOMPARE(result.height(), 90);
	QCOMPARE(result.x(), 70);
}

void BulkResizeTest::testResizedKeepingAnchorBottomLeft()
{
	const QRect initial(100, 200, 50, 60); // bottom edge is 200 + 60 = 260
	const QRect result = BulkResize::resizedKeepingAnchor(initial, 80, 90, Qt::BottomLeftCorner);

	QCOMPARE(result.x(), 100);
	QCOMPARE(result.y() + result.height(), 260);
	QCOMPARE(result.width(), 80);
	QCOMPARE(result.height(), 90);
	QCOMPARE(result.y(), 170);
}

void BulkResizeTest::testResizedKeepingAnchorBottomRight()
{
	const QRect initial(100, 200, 50, 60); // right is 150, bottom is 260
	const QRect result = BulkResize::resizedKeepingAnchor(initial, 80, 90, Qt::BottomRightCorner);

	QCOMPARE(result.x() + result.width(), 150);
	QCOMPARE(result.y() + result.height(), 260);
	QCOMPARE(result.width(), 80);
	QCOMPARE(result.height(), 90);
	QCOMPARE(result.x(), 70);
	QCOMPARE(result.y(), 170);
}

void BulkResizeTest::testResizedKeepingAnchorDisabledAxis()
{
	const QRect initial(100, 200, 50, 60);

	// newWidth <= 0 -> keep current width
	const QRect resultW = BulkResize::resizedKeepingAnchor(initial, -1, 100, Qt::TopLeftCorner);
	QCOMPARE(resultW.width(), 50);
	QCOMPARE(resultW.height(), 100);

	// newHeight <= 0 -> keep current height
	const QRect resultH = BulkResize::resizedKeepingAnchor(initial, 120, 0, Qt::TopLeftCorner);
	QCOMPARE(resultH.width(), 120);
	QCOMPARE(resultH.height(), 60);

	// both <= 0 -> keep current width & height
	const QRect resultBoth = BulkResize::resizedKeepingAnchor(initial, -10, -5, Qt::TopLeftCorner);
	QCOMPARE(resultBoth.width(), 50);
	QCOMPARE(resultBoth.height(), 60);
}

void BulkResizeTest::testResizedKeepingAnchorMinimumDimensions()
{
	const QRect initial(100, 200, 50, 60);

	// Width and height requested as negative or zero culls to at least 1x1 if initial was smaller,
	// or requested dimensions < 1 cap at 1
	const QRect resultMin = BulkResize::resizedKeepingAnchor(initial, -50, -50, Qt::TopLeftCorner);
	QVERIFY(resultMin.width() >= 1);
	QVERIFY(resultMin.height() >= 1);

	const QRect initialTiny(100, 200, 0, -5);
	const QRect resultTiny = BulkResize::resizedKeepingAnchor(initialTiny, 0, 0, Qt::TopLeftCorner);
	QCOMPARE(resultTiny.width(), 1);
	QCOMPARE(resultTiny.height(), 1);
}
