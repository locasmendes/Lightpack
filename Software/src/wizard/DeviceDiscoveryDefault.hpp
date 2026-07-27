/*
 * DeviceDiscoveryDefault.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#pragma once

#include "enums.hpp"

namespace DeviceDiscoveryDefault {

/*!
 * Whether LightpackDiscoveryPage should default to "Lightpack selected" (skipping
 * SelectDevicePage/ConfigureDevicePage) purely because a Lightpack HID interface was
 * detected. Detecting a Lightpack must not silently override a profile already
 * configured for a different device (e.g. Adalight/Ardulight over serial) - doing so
 * used to leave _transSettings->ledDevice on the discovery page's placeholder
 * LedDeviceLightpack (10 LEDs max) instead of the user's real device.
 */
bool shouldSelectLightpack(bool isInitFromSettings, SupportedDevices::DeviceType configuredDevice);

} // namespace DeviceDiscoveryDefault
