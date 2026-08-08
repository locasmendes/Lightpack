/*
 * ScreenTopologyTest.cpp
 *
 * Phase 1: pure topology resolution without QApplication screen objects.
 */

#include "ScreenTopologyTest.hpp"
#include "ScreenTopology.hpp"
#include <QtTest>

using namespace ScreenTopology;

ScreenTopologyTest::ScreenTopologyTest(QObject *parent) :
	QObject(parent)
{
}

void ScreenTopologyTest::testScreensContainingZones()
{
	QHash<QString, ScreenEntry> screens;
	screens.insert(QStringLiteral("A"),
		{ Identity{QStringLiteral("DP-1"), QStringLiteral("Dell"), QStringLiteral("SN1")},
		  QRect(0, 0, 1920, 1080) });
	screens.insert(QStringLiteral("B"),
		{ Identity{QStringLiteral("HDMI-1"), QStringLiteral("LG"), QStringLiteral("SN2")},
		  QRect(1920, 0, 1920, 1080) });

	const QList<QPoint> zonesOnA{ QPoint(100, 100), QPoint(200, 200) };
	const QList<Identity> hitA = screensContainingZones(zonesOnA, screens);
	QCOMPARE(hitA.size(), 1);
	QCOMPARE(hitA.first().name, QStringLiteral("DP-1"));

	const QList<QPoint> zonesOnBoth{ QPoint(100, 100), QPoint(2000, 100) };
	const QList<Identity> hitBoth = screensContainingZones(zonesOnBoth, screens);
	QCOMPARE(hitBoth.size(), 2);

	const QList<QPoint> zonesNowhere{ QPoint(-500, -500) };
	QCOMPARE(screensContainingZones(zonesNowhere, screens).size(), 0);
}

void ScreenTopologyTest::testAnyZoneHasValidScreen()
{
	QHash<QString, ScreenEntry> screens;
	screens.insert(QStringLiteral("A"),
		{ Identity{QStringLiteral("DP-1"), QStringLiteral("Dell"), QStringLiteral("SN1")},
		  QRect(0, 0, 1920, 1080) });

	QVERIFY(anyZoneHasValidScreen({ QPoint(10, 10) }, screens));
	QVERIFY(!anyZoneHasValidScreen({ QPoint(-10, -10) }, screens));
	QVERIFY(!anyZoneHasValidScreen({ QPoint(10, 10) }, QHash<QString, ScreenEntry>()));
}

void ScreenTopologyTest::testActiveScreenReturned()
{
	QHash<QString, ScreenEntry> screens;
	screens.insert(QStringLiteral("A"),
		{ Identity{QStringLiteral("DP-1"), QStringLiteral("Dell"), QStringLiteral("SN1")},
		  QRect(0, 0, 1920, 1080) });

	const Identity saved{ QStringLiteral("DP-1"), QStringLiteral("Dell"), QStringLiteral("SN1") };
	const Identity other{ QStringLiteral("HDMI-1"), QStringLiteral("LG"), QStringLiteral("SN2") };

	QVERIFY(activeScreenReturned(saved, screens));
	QVERIFY(!activeScreenReturned(other, screens));
	QVERIFY(!activeScreenReturned(Identity{}, screens));
}

void ScreenTopologyTest::testPrimaryScreenForZones()
{
	QHash<QString, ScreenEntry> screens;
	screens.insert(QStringLiteral("A"),
		{ Identity{QStringLiteral("DP-1"), QStringLiteral("Dell"), QStringLiteral("SN1")},
		  QRect(0, 0, 1920, 1080) });
	screens.insert(QStringLiteral("B"),
		{ Identity{QStringLiteral("HDMI-1"), QStringLiteral("LG"), QStringLiteral("SN2")},
		  QRect(1920, 0, 1920, 1080) });

	const Identity primary = primaryScreenForZones(
		{ QPoint(100, 100), QPoint(150, 150), QPoint(2000, 100) }, screens);
	QCOMPARE(primary.name, QStringLiteral("DP-1"));

	QVERIFY(primaryScreenForZones({ QPoint(-1, -1) }, screens).isEmpty());
}

void ScreenTopologyTest::testConsecutiveMissAndTurnOff()
{
	QCOMPARE(nextConsecutiveMissCount(0, true), 0);
	QCOMPARE(nextConsecutiveMissCount(5, true), 0);
	QCOMPARE(nextConsecutiveMissCount(0, false), 1);
	QCOMPARE(nextConsecutiveMissCount(2, false), 3);

	QVERIFY(!shouldTurnOffForMissingScreen(0));
	QVERIFY(!shouldTurnOffForMissingScreen(kNoScreenTurnOffThreshold - 1));
	QVERIFY(shouldTurnOffForMissingScreen(kNoScreenTurnOffThreshold));
	QVERIFY(shouldTurnOffForMissingScreen(kNoScreenTurnOffThreshold + 2));
}

void ScreenTopologyTest::testShouldRestoreLightsAfterReconnect()
{
	QVERIFY(shouldRestoreLightsAfterReconnect(true, true, true));
	QVERIFY(shouldRestoreLightsAfterReconnect(true, true, /*activeReturnedOrUntracked*/ true));
	QVERIFY(!shouldRestoreLightsAfterReconnect(false, true, true));
	QVERIFY(!shouldRestoreLightsAfterReconnect(true, false, true));
	QVERIFY(!shouldRestoreLightsAfterReconnect(true, true, false));
}

void ScreenTopologyTest::testIdentitySettingsRoundTrip()
{
	const Identity id{ QStringLiteral("\\\\.\\DISPLAY1"), QStringLiteral("ACR"), QStringLiteral("ABC123") };
	const QString raw = id.toSettingsString();
	const Identity back = Identity::fromSettingsString(raw);
	QCOMPARE(back, id);

	QVERIFY(Identity::fromSettingsString(QString()).isEmpty());
}

void ScreenTopologyTest::testTopologyEventSequence()
{
	// Simulate: two screens → remove the one with zones → reconnect same identity.
	QHash<QString, ScreenEntry> screens;
	const Identity zoneScreen{ QStringLiteral("DP-1"), QStringLiteral("Dell"), QStringLiteral("SN1") };
	const Identity other{ QStringLiteral("HDMI-1"), QStringLiteral("LG"), QStringLiteral("SN2") };

	screens.insert(QStringLiteral("A"), { zoneScreen, QRect(0, 0, 1920, 1080) });
	screens.insert(QStringLiteral("B"), { other, QRect(1920, 0, 1920, 1080) });

	const QList<QPoint> zones{ QPoint(100, 100), QPoint(200, 300) };
	QVERIFY(anyZoneHasValidScreen(zones, screens));
	QCOMPARE(primaryScreenForZones(zones, screens), zoneScreen);

	// Unplug screen A (zones orphaned). Remaining screen stays away from zone coords
	// (Windows may also relocate survivors; here we keep them non-overlapping).
	screens.remove(QStringLiteral("A"));
	screens[QStringLiteral("B")].geometry = QRect(1920, 0, 1920, 1080);
	QVERIFY(!anyZoneHasValidScreen(zones, screens));
	QVERIFY(!activeScreenReturned(zoneScreen, screens));

	int misses = 0;
	misses = nextConsecutiveMissCount(misses, false);
	misses = nextConsecutiveMissCount(misses, false);
	misses = nextConsecutiveMissCount(misses, false);
	QVERIFY(shouldTurnOffForMissingScreen(misses));

	// Replug A (possibly at a new virtual-desktop origin).
	screens.insert(QStringLiteral("A"), { zoneScreen, QRect(1920, 0, 1920, 1080) });
	screens[QStringLiteral("B")].geometry = QRect(0, 0, 1920, 1080);

	QVERIFY(activeScreenReturned(zoneScreen, screens));
	// Absolute zone coords still orphaned until GrabWidget::settingsProfileChanged restore;
	// identity return is what authorizes turning lights back on after reconnect.
	QVERIFY(shouldRestoreLightsAfterReconnect(
		/*lightsWereTurnedOffForDisconnect=*/true,
		/*hasValidScreen=*/activeScreenReturned(zoneScreen, screens),
		/*activeReturnedOrUntracked=*/true));
}
