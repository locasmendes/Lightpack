/*
 * LedGroupRuntimeTest.hpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#pragma once

#include <QObject>

class LedGroupRuntimeTest : public QObject
{
	Q_OBJECT
public:
	explicit LedGroupRuntimeTest(QObject *parent = nullptr);

private slots:
	void initTestCase();
	void testLedGroupJsonRoundTrip();
	void testApplyGroupDirectSettings();
	void testInvalidMemberIdsIgnored();
	void testOverlappingGroupsLastWins();
	void testRegressionEmptyGroups();
};
