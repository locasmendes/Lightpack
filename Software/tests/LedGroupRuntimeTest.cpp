/*
 * LedGroupRuntimeTest.cpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#include "LedGroupRuntimeTest.hpp"
#include "LedGroupRuntime.hpp"
#include "Settings.hpp"
#include <QtTest>
#include <QTemporaryDir>

LedGroupRuntimeTest::LedGroupRuntimeTest(QObject *parent) :
	QObject(parent)
{
}

void LedGroupRuntimeTest::initTestCase()
{
	static QTemporaryDir tempDir;
	if (tempDir.isValid()) {
		SettingsScope::Settings::Initialize(tempDir.path() + QStringLiteral("/"), false);
	}
}

void LedGroupRuntimeTest::testLedGroupJsonRoundTrip()
{
	SettingsScope::LedGroup group;
	group.name = QStringLiteral("TopStand");
	group.memberIds = { 0, 1, 2, 3 };
	group.edge = SettingsScope::LedGroup::Edge::Top;
	group.width = -1;
	group.height = 120;
	group.enabled = true;
	group.hasColor = false;

	const QJsonObject json = group.toJson();
	const SettingsScope::LedGroup deserialized = SettingsScope::LedGroup::fromJson(json);

	QCOMPARE(deserialized.name, group.name);
	QCOMPARE(deserialized.memberIds, group.memberIds);
	QCOMPARE(static_cast<int>(deserialized.edge), static_cast<int>(group.edge));
	QCOMPARE(deserialized.width, group.width);
	QCOMPARE(deserialized.height, group.height);
	QCOMPARE(deserialized.enabled, group.enabled);
	QCOMPARE(deserialized.hasColor, group.hasColor);
	QCOMPARE(deserialized, group);
}

void LedGroupRuntimeTest::testLedGroupJsonRoundTripWithColor()
{
	SettingsScope::LedGroup group;
	group.name = QStringLiteral("Bottom");
	group.memberIds = { 4, 5, 6 };
	group.edge = SettingsScope::LedGroup::Edge::Bottom;
	group.hasColor = true;
	group.color = QColor(200, 100, 50);

	const QJsonObject json = group.toJson();
	const SettingsScope::LedGroup deserialized = SettingsScope::LedGroup::fromJson(json);

	QCOMPARE(deserialized.hasColor, true);
	QCOMPARE(deserialized.color, group.color);
	QCOMPARE(deserialized, group);
}

void LedGroupRuntimeTest::testApplyGroupDirectSettings()
{
	// Setup LED 0
	const QPoint initPos0(100, 100);
	const QSize initSize0(50, 50);
	SettingsScope::Settings::setLedPosition(0, initPos0);
	SettingsScope::Settings::setLedSize(0, initSize0);

	// 1. Apply Top group (should only alter height, anchor top-left)
	SettingsScope::LedGroup topGroup;
	topGroup.name = QStringLiteral("TopGroup");
	topGroup.memberIds = { 0 };
	topGroup.edge = SettingsScope::LedGroup::Edge::Top;
	topGroup.width = 200; // should be ignored for Edge::Top (-1 targetW)
	topGroup.height = 80;
	topGroup.enabled = true;

	LedGroupRuntime::applyGroup(topGroup);

	QCOMPARE(SettingsScope::Settings::getLedPosition(0), initPos0); // top-left position unchanged
	QCOMPARE(SettingsScope::Settings::getLedSize(0).width(), 50);   // width unchanged
	QCOMPARE(SettingsScope::Settings::getLedSize(0).height(), 80);  // height updated to 80

	// 2. Apply Custom group (should alter both width and height)
	SettingsScope::LedGroup customGroup;
	customGroup.name = QStringLiteral("CustomGroup");
	customGroup.memberIds = { 0 };
	customGroup.edge = SettingsScope::LedGroup::Edge::Custom;
	customGroup.width = 75;
	customGroup.height = 95;
	customGroup.enabled = true;

	LedGroupRuntime::applyGroup(customGroup);

	QCOMPARE(SettingsScope::Settings::getLedSize(0).width(), 75);  // width updated to 75
	QCOMPARE(SettingsScope::Settings::getLedSize(0).height(), 95); // height updated to 95
}

void LedGroupRuntimeTest::testInvalidMemberIdsIgnored()
{
	const int maxLeds = SettingsScope::Settings::getNumberOfLeds(SettingsScope::Settings::getConnectedDevice());
	const QPoint initPos(10, 10);
	const QSize initSize(30, 30);

	// Setup valid LED 0
	SettingsScope::Settings::setLedPosition(0, initPos);
	SettingsScope::Settings::setLedSize(0, initSize);

	SettingsScope::LedGroup group;
	group.name = QStringLiteral("MixedIds");
	// Includes negative ID -1 and out of bounds maxLeds + 10
	group.memberIds = { -1, 0, maxLeds + 10 };
	group.edge = SettingsScope::LedGroup::Edge::Custom;
	group.width = 60;
	group.height = 60;
	group.enabled = true;

	// Applying group should not fail or crash
	const bool success = LedGroupRuntime::applyGroup(group);
	QVERIFY(success);

	// Valid LED 0 should be updated
	QCOMPARE(SettingsScope::Settings::getLedSize(0), QSize(60, 60));
}

void LedGroupRuntimeTest::testOverlappingGroupsLastWins()
{
	const QPoint initPos(0, 0);
	const QSize initSize(40, 40);
	SettingsScope::Settings::setLedPosition(0, initPos);
	SettingsScope::Settings::setLedSize(0, initSize);

	SettingsScope::LedGroup group1;
	group1.name = QStringLiteral("First");
	group1.memberIds = { 0 };
	group1.edge = SettingsScope::LedGroup::Edge::Custom;
	group1.width = 100;
	group1.height = 100;

	SettingsScope::LedGroup group2;
	group2.name = QStringLiteral("Second");
	group2.memberIds = { 0 };
	group2.edge = SettingsScope::LedGroup::Edge::Custom;
	group2.width = 150;
	group2.height = 150;

	QList<SettingsScope::LedGroup> groups = { group1, group2 };
	SettingsScope::Settings::setLedGroups(groups);

	LedGroupRuntime::applyAll();

	// Last group (group2) should win
	QCOMPARE(SettingsScope::Settings::getLedSize(0), QSize(150, 150));
}

void LedGroupRuntimeTest::testRegressionEmptyGroups()
{
	// Clear all groups
	SettingsScope::Settings::setLedGroups(QList<SettingsScope::LedGroup>());

	const QPoint pos(20, 20);
	const QSize sz(40, 40);
	SettingsScope::Settings::setLedPosition(0, pos);
	SettingsScope::Settings::setLedSize(0, sz);

	// applyAll should do nothing when groups list is empty
	const bool success = LedGroupRuntime::applyAll();
	QVERIFY(success);

	QCOMPARE(SettingsScope::Settings::getLedPosition(0), pos);
	QCOMPARE(SettingsScope::Settings::getLedSize(0), sz);
}
