/*
 * ScreenTopologyTest.hpp
 *
 * Phase 1 unit tests for multi-screen topology decisions (BulkResize-style).
 */

#pragma once

#include <QObject>

class ScreenTopologyTest : public QObject
{
	Q_OBJECT
public:
	explicit ScreenTopologyTest(QObject *parent = nullptr);

private slots:
	void testScreensContainingZones();
	void testAnyZoneHasValidScreen();
	void testActiveScreenReturned();
	void testPrimaryScreenForZones();
	void testConsecutiveMissAndTurnOff();
	void testShouldRestoreLightsAfterReconnect();
	void testIdentitySettingsRoundTrip();
	void testTopologyEventSequence();
};
