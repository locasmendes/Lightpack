/*
 * DeviceDiscoveryDefaultTest.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#include "DeviceDiscoveryDefaultTest.hpp"
#include "wizard/DeviceDiscoveryDefault.hpp"
#include <QtTest>

void DeviceDiscoveryDefaultTest::initTestCase()
{
}

void DeviceDiscoveryDefaultTest::cleanupTestCase()
{
}

void DeviceDiscoveryDefaultTest::testFreshWizardDefaultsToLightpack()
{
	// Not initialized from an existing profile (isInitFromSettings == false) - a brand
	// new setup should still default to the detected Lightpack, regardless of
	// configuredDevice (which is meaningless before any profile exists).
	QVERIFY(DeviceDiscoveryDefault::shouldSelectLightpack(false, SupportedDevices::DeviceTypeLightpack));
	QVERIFY(DeviceDiscoveryDefault::shouldSelectLightpack(false, SupportedDevices::DeviceTypeAdalight));
}

void DeviceDiscoveryDefaultTest::testExistingLightpackProfileDefaultsToLightpack()
{
	QVERIFY(DeviceDiscoveryDefault::shouldSelectLightpack(true, SupportedDevices::DeviceTypeLightpack));
}

void DeviceDiscoveryDefaultTest::testExistingAdalightProfileDoesNotDefaultToLightpack()
{
	// Regression test for the bug: a profile already configured for Adalight
	// (Arduino over serial) must not be silently bounced onto the 10-LED
	// Lightpack path just because a Lightpack HID interface was detected.
	QVERIFY(!DeviceDiscoveryDefault::shouldSelectLightpack(true, SupportedDevices::DeviceTypeAdalight));
}

void DeviceDiscoveryDefaultTest::testExistingArdulightProfileDoesNotDefaultToLightpack()
{
	QVERIFY(!DeviceDiscoveryDefault::shouldSelectLightpack(true, SupportedDevices::DeviceTypeArdulight));
}
