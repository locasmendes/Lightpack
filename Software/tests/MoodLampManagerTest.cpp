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

	// Collects the QList<QRgb> argument of every updateLedsColors emission captured by spy.
	QList<QList<QRgb>> capturedColorLists(const QSignalSpy& spy)
	{
		QList<QList<QRgb>> result;
		for (const QList<QVariant>& args : spy)
			result.append(args.at(0).value<QList<QRgb>>());
		return result;
	}
}

void MoodLampManagerTest::initTestCase()
{
	qRegisterMetaType<QList<QRgb>>("QList<QRgb>");

	static QTemporaryDir tempDir;
	if (tempDir.isValid())
		Settings::Initialize(tempDir.path() + QStringLiteral("/"), false);

	QVERIFY(staticLampId() >= 0);
	QVERIFY(fireLampId() >= 0);
	QVERIFY(staticLampId() != fireLampId());
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
	manager.setLiquidMode(false);
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
	manager.setLiquidMode(false);
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
	manager.setLiquidMode(true);
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
	manager.setLiquidMode(false);
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
	manager.setLiquidMode(false);
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
	manager.setLiquidMode(false);
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
