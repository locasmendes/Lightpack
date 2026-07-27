/*
 * DeviceDiscoveryDefaultTest.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#pragma once

#include <QObject>

class DeviceDiscoveryDefaultTest : public QObject
{
	Q_OBJECT

private Q_SLOTS:
	void initTestCase();
	void cleanupTestCase();

	void testFreshWizardDefaultsToLightpack();
	void testExistingLightpackProfileDefaultsToLightpack();
	void testExistingAdalightProfileDoesNotDefaultToLightpack();
	void testExistingArdulightProfileDoesNotDefaultToLightpack();
};
