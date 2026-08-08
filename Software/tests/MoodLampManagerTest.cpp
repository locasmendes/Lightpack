/*
 * MoodLampManagerTest.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#include "MoodLampManagerTest.hpp"
#include "MoodLampManager.hpp"
#include "MoodLamp.hpp"
#include "Settings.hpp"
#include "ColorOps.hpp"
#include "ColorF.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

using namespace SettingsScope;

namespace {
	const int NumberOfLeds = 4;

	int lampIdByName(const QString& name)
	{
		QList<MoodLampLampInfo> list;
		int recommended = 0;
		MoodLampBase::populateNameList(list, recommended);
		for (const MoodLampLampInfo& info : list) {
			if (info.name == name)
				return info.id;
		}
		return -1;
	}

	int staticLampId() { return MoodLampBase::defaultLampId(); }
	int fireLampId() { return lampIdByName(QStringLiteral("Fire")); }
	int breathingLampId() { return MoodLampBase::breathingLampId(); }
	int rainbowLampId() { return lampIdByName(QStringLiteral("Rainbow")); }
	int cometLampId() { return lampIdByName(QStringLiteral("Comet")); }
	int theaterChaseLampId() { return lampIdByName(QStringLiteral("Theater Chase")); }
	int twinkleLampId() { return lampIdByName(QStringLiteral("Twinkle")); }

	// Collects updateLedsColors emissions and converts LinearRgbF ? QRgb for assertions.
	QList<QList<QRgb>> capturedColorLists(const QSignalSpy& spy)
	{
		QList<QList<QRgb>> result;
		for (const QList<QVariant>& args : spy) {
			const QList<LinearRgbF> linear = args.at(0).value<QList<LinearRgbF>>();
			QList<QRgb> encoded;
			encoded.reserve(linear.size());
			for (const LinearRgbF &L : linear) {
				const EncodedRgbF e = ColorOps::srgbEncode(L);
				encoded.append(qRgb(
					qBound(0, qRound(e.r * 255.f), 255),
					qBound(0, qRound(e.g * 255.f), 255),
					qBound(0, qRound(e.b * 255.f), 255)));
			}
			result.append(encoded);
		}
		return result;
	}
}

void MoodLampManagerTest::initTestCase()
{
	qRegisterMetaType<QList<LinearRgbF>>("QList<LinearRgbF>");

	static QTemporaryDir tempDir;
	if (tempDir.isValid())
		Settings::Initialize(tempDir.path() + QStringLiteral("/"), false);

	QVERIFY(staticLampId() >= 0);
	QVERIFY(fireLampId() >= 0);
	QVERIFY(staticLampId() != fireLampId());
	QVERIFY(breathingLampId() >= 0);
	QVERIFY(rainbowLampId() >= 0);
	QVERIFY(cometLampId() >= 0);
	QVERIFY(theaterChaseLampId() >= 0);
	QVERIFY(twinkleLampId() >= 0);
}

void MoodLampManagerTest::cleanup()
{
	// Every test starts from a clean, device-agnostic, no-smoothing baseline.
	Settings::setConnectedDevice(SupportedDevices::DeviceTypeVirtual);
	Settings::setGrabHostSmoothingDuration(0);
}

void MoodLampManagerTest::testConstantModeWithStaticStaysConstant()
{
	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false); // force a signal on every tick, changed or not
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Constant);
	manager.setCurrentLamp(staticLampId());
	manager.setCurrentColor(QColor(200, 50, 10));

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.start(true);
	QTest::qWait(180); // several 50ms Static ticks

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(emissions.size() >= 2);
	for (const QList<QRgb>& colors : emissions)
		QCOMPARE(colors, emissions.first());
}

void MoodLampManagerTest::testConstantModeForcesStaticEvenWhenFireRequested()
{
	// Regression test: selecting Constant color must hold the LEDs still even
	// if Fire (or RGB is Life) is still the persisted MoodLampLamp preference.
	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Constant);
	manager.setCurrentLamp(fireLampId());
	manager.setCurrentColor(QColor(10, 220, 30));

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.start(true);
	QTest::qWait(180);

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(emissions.size() >= 2);
	for (const QList<QRgb>& colors : emissions)
		QCOMPARE(colors, emissions.first());
}

void MoodLampManagerTest::testLiquidModeWithFireAnimates()
{
	// Contrast test: Fire must still be free to animate when Liquid mode is
	// genuinely selected - the Constant-mode override must not leak here.
	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setCurrentColor(QColor(10, 220, 30));
	manager.setColorMode(MoodLampColorMode::Liquid);
	manager.setCurrentLamp(fireLampId());

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.start(true);
	QTest::qWait(250);

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(emissions.size() >= 2);

	bool sawDifferentFrame = false;
	for (const QList<QRgb>& colors : emissions) {
		if (colors != emissions.first()) {
			sawDifferentFrame = true;
			break;
		}
	}
	QVERIFY(sawDifferentFrame);
}

void MoodLampManagerTest::testHostSmoothingProducesGradualTransition()
{
	Settings::setConnectedDevice(SupportedDevices::DeviceTypeVirtual);
	Settings::setGrabHostSmoothingDuration(300);

	MoodLampManager manager; // reads the 300ms duration from Settings in initFromSettings()
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Constant);
	manager.setCurrentLamp(staticLampId());
	manager.setCurrentColor(QColor(0, 0, 0));
	manager.start(true);

	QTest::qWait(60); // let the initial black frame settle

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.setCurrentColor(QColor(255, 255, 255));
	QTest::qWait(400); // well past the 300ms transition

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY2(emissions.size() > 2, "expected several interpolated frames, not an immediate jump");

	QList<QList<QRgb>> distinctFrames;
	for (const QList<QRgb>& colors : emissions) {
		if (!distinctFrames.contains(colors))
			distinctFrames.append(colors);
	}
	QVERIFY2(distinctFrames.size() > 2, "expected multiple distinct intermediate colors while smoothing");

	const int finalRed = qRed(emissions.last().first());
	QVERIFY(finalRed > 250); // converged (near-)exactly to the white target
}

void MoodLampManagerTest::testHostSmoothingDisabledJumpsImmediately()
{
	Settings::setConnectedDevice(SupportedDevices::DeviceTypeVirtual);
	Settings::setGrabHostSmoothingDuration(0);

	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Constant);
	manager.setCurrentLamp(staticLampId());
	manager.setCurrentColor(QColor(0, 0, 0));
	manager.start(true);
	QTest::qWait(60);

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.setCurrentColor(QColor(255, 255, 255));
	QTest::qWait(120);

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(!emissions.isEmpty());
	// No transition engine engaged: every frame after the change is already the target.
	for (const QList<QRgb>& colors : emissions)
		QCOMPARE(qRed(colors.first()), 255);
}

void MoodLampManagerTest::testHostSmoothingSkippedForLightpackDevice()
{
	Settings::setConnectedDevice(SupportedDevices::DeviceTypeLightpack);
	Settings::setGrabHostSmoothingDuration(300);

	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Constant);
	manager.setCurrentLamp(staticLampId());
	manager.setCurrentColor(QColor(0, 0, 0));
	manager.start(true);
	QTest::qWait(60);

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.setCurrentColor(QColor(255, 255, 255));
	QTest::qWait(120);

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(!emissions.isEmpty());
	for (const QList<QRgb>& colors : emissions)
		QCOMPARE(qRed(colors.first()), 255);
}

void MoodLampManagerTest::testGroupColorOverrideAppliesToMembers()
{
	QList<QRgb> colors = { qRgb(1, 1, 1), qRgb(2, 2, 2), qRgb(3, 3, 3), qRgb(4, 4, 4) };
	Settings::setLedEnabled(0, true);
	Settings::setLedEnabled(2, true);

	LedGroup group;
	group.name = QStringLiteral("bottom");
	group.memberIds = { 0, 2 };
	group.hasColor = true;
	group.color = QColor(200, 100, 50);

	const bool changed = MoodLampManager::applyGroupColorOverrides(colors, { group });

	QVERIFY(changed);
	QCOMPARE(colors[0], group.color.rgb());
	QCOMPARE(colors[2], group.color.rgb());
	QCOMPARE(colors[1], qRgb(2, 2, 2)); // untouched, not a member
	QCOMPARE(colors[3], qRgb(4, 4, 4)); // untouched, not a member
}

void MoodLampManagerTest::testGroupColorOverrideSkipsDisabledLeds()
{
	QList<QRgb> colors = { qRgb(1, 1, 1), qRgb(2, 2, 2) };
	Settings::setLedEnabled(0, true);
	Settings::setLedEnabled(1, false);

	LedGroup group;
	group.name = QStringLiteral("all");
	group.memberIds = { 0, 1 };
	group.hasColor = true;
	group.color = QColor(200, 100, 50);

	MoodLampManager::applyGroupColorOverrides(colors, { group });

	QCOMPARE(colors[0], group.color.rgb());
	QCOMPARE(colors[1], qRgb(2, 2, 2)); // disabled LED left untouched
	Settings::setLedEnabled(1, true);
}

void MoodLampManagerTest::testGroupColorOverrideOverlapLastWins()
{
	QList<QRgb> colors = { qRgb(0, 0, 0) };
	Settings::setLedEnabled(0, true);

	LedGroup first;
	first.name = QStringLiteral("first");
	first.memberIds = { 0 };
	first.hasColor = true;
	first.color = QColor(100, 0, 0);

	LedGroup second;
	second.name = QStringLiteral("second");
	second.memberIds = { 0 };
	second.hasColor = true;
	second.color = QColor(0, 100, 0);

	MoodLampManager::applyGroupColorOverrides(colors, { first, second });

	QCOMPARE(colors[0], second.color.rgb());
}

void MoodLampManagerTest::testGroupColorOverrideIgnoredWhenGroupDisabledOrNoColor()
{
	const QList<QRgb> original = { qRgb(9, 9, 9) };
	Settings::setLedEnabled(0, true);

	LedGroup disabledGroup;
	disabledGroup.name = QStringLiteral("disabled");
	disabledGroup.memberIds = { 0 };
	disabledGroup.enabled = false;
	disabledGroup.hasColor = true;
	disabledGroup.color = QColor(200, 100, 50);

	QList<QRgb> colors1 = original;
	QVERIFY(!MoodLampManager::applyGroupColorOverrides(colors1, { disabledGroup }));
	QCOMPARE(colors1, original);

	LedGroup noColorGroup;
	noColorGroup.name = QStringLiteral("nocolor");
	noColorGroup.memberIds = { 0 };
	noColorGroup.enabled = true;
	noColorGroup.hasColor = false;

	QList<QRgb> colors2 = original;
	QVERIFY(!MoodLampManager::applyGroupColorOverrides(colors2, { noColorGroup }));
	QCOMPARE(colors2, original);
}

void MoodLampManagerTest::testGroupColorOverrideOnlyAppliedInConstantMode()
{
	LedGroup group;
	group.name = QStringLiteral("bottom");
	group.memberIds = { 0 };
	group.hasColor = true;
	group.color = QColor(9, 200, 9);
	Settings::setLedEnabled(0, true);
	Settings::setLedGroups({ group });

	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Constant);
	manager.setCurrentLamp(staticLampId());
	manager.setCurrentColor(QColor(200, 0, 0));

	QSignalSpy constantSpy(&manager, &MoodLampManager::updateLedsColors);
	manager.start(true);
	QTest::qWait(120);
	QVERIFY(!constantSpy.isEmpty());
	QCOMPARE(capturedColorLists(constantSpy).last().first(), group.color.rgb());

	manager.setColorMode(MoodLampColorMode::Liquid);
	QSignalSpy liquidSpy(&manager, &MoodLampManager::updateLedsColors);
	QTest::qWait(120);
	QVERIFY(!liquidSpy.isEmpty());
	QVERIFY(capturedColorLists(liquidSpy).last().first() != group.color.rgb());

	Settings::setLedGroups({});
}

void MoodLampManagerTest::testBreathingModeForcesBreathingLampAndPulses()
{
	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Breathing);
	manager.setCurrentLamp(staticLampId());
	manager.setCurrentColor(QColor(200, 50, 10));

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.start(true);
	QTest::qWait(200); // several 33ms Breathing ticks, well under one ~4s cycle

	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(emissions.size() >= 2);

	bool sawDifferentFrame = false;
	for (const QList<QRgb>& colors : emissions) {
		if (colors != emissions.first()) {
			sawDifferentFrame = true;
			break;
		}
	}
	QVERIFY2(sawDifferentFrame, "Breathing should pulse brightness over time, unlike Static");
}

void MoodLampManagerTest::testBreathingModeIgnoresRequestedLamp()
{
	// Regression test mirroring testConstantModeForcesStaticEvenWhenFireRequested:
	// Breathing must force its own lamp even if Fire is still the persisted preference.
	MoodLampManager manager;
	manager.setSendDataOnlyIfColorsChanged(false);
	manager.setNumberOfLeds(NumberOfLeds);
	manager.setColorMode(MoodLampColorMode::Breathing);
	manager.setCurrentLamp(fireLampId());
	manager.setCurrentColor(QColor(10, 220, 30));

	QSignalSpy spy(&manager, &MoodLampManager::updateLedsColors);
	manager.start(true);
	QTest::qWait(200);

	// Breathing pulses every LED identically (uniform brightness); Fire would instead
	// diverge per-pixel. This is a structural proxy for "Fire isn't the active lamp".
	const QList<QList<QRgb>> emissions = capturedColorLists(spy);
	QVERIFY(!emissions.isEmpty());
	for (const QList<QRgb>& colors : emissions) {
		for (int i = 1; i < colors.size(); ++i)
			QCOMPARE(colors[i], colors.first());
	}
}

void MoodLampManagerTest::testNewLampEffectsPreserveSizeAndRespectDisabledLeds()
{
	Settings::setLedEnabled(0, true);
	Settings::setLedEnabled(1, false);

	const QList<int> lampIds = { rainbowLampId(), cometLampId(), theaterChaseLampId(), twinkleLampId() };
	for (int lampId : lampIds) {
		QVERIFY(lampId >= 0);
		MoodLampBase* const lamp = MoodLampBase::createWithID(lampId);
		QVERIFY(lamp != nullptr);

		QList<QRgb> colors = { 0, 0, 0, 0 };
		for (int tick = 0; tick < 5; ++tick)
			lamp->shine(QColor(200, 100, 50), colors);

		QCOMPARE(colors.size(), 4);
		QCOMPARE(colors[1], static_cast<QRgb>(0)); // disabled LED always stays off

		delete lamp;
	}

	Settings::setLedEnabled(1, true);
}

void MoodLampManagerTest::testTheaterChaseExactPattern()
{
	for (int i = 0; i < 6; ++i)
		Settings::setLedEnabled(i, true);

	MoodLampBase* const lamp = MoodLampBase::createWithID(theaterChaseLampId());
	QVERIFY(lamp != nullptr);

	QList<QRgb> colors(6, 0);
	lamp->shine(QColor(255, 255, 255), colors); // frame 0: offset 0 -> LEDs 0,3 lit
	QVERIFY(colors[0] != 0);
	QCOMPARE(colors[1], static_cast<QRgb>(0));
	QCOMPARE(colors[2], static_cast<QRgb>(0));
	QVERIFY(colors[3] != 0);
	QCOMPARE(colors[4], static_cast<QRgb>(0));
	QCOMPARE(colors[5], static_cast<QRgb>(0));

	lamp->shine(QColor(255, 255, 255), colors); // frame 1: offset 1 -> LEDs 2,5 lit
	QCOMPARE(colors[0], static_cast<QRgb>(0));
	QCOMPARE(colors[1], static_cast<QRgb>(0));
	QVERIFY(colors[2] != 0);
	QCOMPARE(colors[3], static_cast<QRgb>(0));
	QCOMPARE(colors[4], static_cast<QRgb>(0));
	QVERIFY(colors[5] != 0);

	delete lamp;
}

void MoodLampManagerTest::testVisibleEffectParamsForLamp()
{
	// Twinkle has no directionality (sparse random flashes), so it exposes Density instead.
	QCOMPARE(MoodLampManager::visibleEffectParamsForLamp(twinkleLampId()),
		QStringList({ QStringLiteral("Speed"), QStringLiteral("Density") }));

	// Rainbow/Comet/Theater Chase all move along the strip, so Direction is meaningful
	// instead of Density.
	QCOMPARE(MoodLampManager::visibleEffectParamsForLamp(rainbowLampId()),
		QStringList({ QStringLiteral("Speed"), QStringLiteral("Direction") }));
	QCOMPARE(MoodLampManager::visibleEffectParamsForLamp(cometLampId()),
		QStringList({ QStringLiteral("Speed"), QStringLiteral("Direction") }));
	QCOMPARE(MoodLampManager::visibleEffectParamsForLamp(theaterChaseLampId()),
		QStringList({ QStringLiteral("Speed"), QStringLiteral("Direction") }));

	// Effects with no per-tick parameters (or no animation at all) expose nothing.
	QVERIFY(MoodLampManager::visibleEffectParamsForLamp(staticLampId()).isEmpty());
	QVERIFY(MoodLampManager::visibleEffectParamsForLamp(fireLampId()).isEmpty());
	QVERIFY(MoodLampManager::visibleEffectParamsForLamp(breathingLampId()).isEmpty());
	QVERIFY(MoodLampManager::visibleEffectParamsForLamp(-1).isEmpty());
}

void MoodLampManagerTest::testEffectSpeedRespectsPersistedSetting()
{
	const int lampId = cometLampId();
	QVERIFY(lampId >= 0);
	for (int i = 0; i < 6; ++i)
		Settings::setLedEnabled(i, true);

	// 3x speed: the comet head should be 3 LEDs further along on the second tick than the
	// default 1.0x speed would put it (see testTheaterChaseExactPattern for the 1.0x case).
	Settings::setMoodLampEffectSpeed(lampId, 3.0);
	Settings::setMoodLampEffectDirection(lampId, 1);
	{
		MoodLampBase* const lamp = MoodLampBase::createWithID(lampId);
		QVERIFY(lamp != nullptr);
		QList<QRgb> colors(6, 0);
		lamp->shine(QColor(255, 255, 255), colors); // frame 0: head at LED 0
		lamp->shine(QColor(255, 255, 255), colors); // frame 1: head at LED 0 + 3*1 = 3
		QVERIFY(colors[3] != 0);
		delete lamp;
	}

	// Reversed direction at default speed: the head should move backward (wrapping to the
	// last LED) instead of forward.
	Settings::setMoodLampEffectSpeed(lampId, 1.0);
	Settings::setMoodLampEffectDirection(lampId, -1);
	{
		MoodLampBase* const lamp = MoodLampBase::createWithID(lampId);
		QVERIFY(lamp != nullptr);
		QList<QRgb> colors(6, 0);
		lamp->shine(QColor(255, 255, 255), colors); // frame 0: head at LED 0
		lamp->shine(QColor(255, 255, 255), colors); // frame 1: head at LED 0 - 1, wraps to 5
		QVERIFY(colors[5] != 0);
		delete lamp;
	}

	// Restore neutral defaults so later tests aren't affected by this test's persisted state.
	Settings::setMoodLampEffectSpeed(lampId, 1.0);
	Settings::setMoodLampEffectDirection(lampId, 1);
}
