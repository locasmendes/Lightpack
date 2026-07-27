/*
 * MoodLampManagerTest.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#pragma once

#include <QObject>

class MoodLampManagerTest : public QObject
{
	Q_OBJECT

private Q_SLOTS:
	void initTestCase();
	void cleanup();

	void testConstantModeWithStaticStaysConstant();
	void testConstantModeForcesStaticEvenWhenFireRequested();
	void testLiquidModeWithFireAnimates();
	void testHostSmoothingProducesGradualTransition();
	void testHostSmoothingDisabledJumpsImmediately();
	void testHostSmoothingSkippedForLightpackDevice();

	void testGroupColorOverrideAppliesToMembers();
	void testGroupColorOverrideSkipsDisabledLeds();
	void testGroupColorOverrideOverlapLastWins();
	void testGroupColorOverrideIgnoredWhenGroupDisabledOrNoColor();
	void testGroupColorOverrideOnlyAppliedInConstantMode();

	void testBreathingModeForcesBreathingLampAndPulses();
	void testBreathingModeIgnoresRequestedLamp();
	void testNewLampEffectsPreserveSizeAndRespectDisabledLeds();
	void testTheaterChaseExactPattern();

	void testVisibleEffectParamsForLamp();
	void testEffectSpeedRespectsPersistedSetting();
};
