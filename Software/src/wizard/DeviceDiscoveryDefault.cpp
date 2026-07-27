/*
 * DeviceDiscoveryDefault.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#include "DeviceDiscoveryDefault.hpp"

namespace DeviceDiscoveryDefault {

bool shouldSelectLightpack(bool isInitFromSettings, SupportedDevices::DeviceType configuredDevice)
{
	return !isInitFromSettings || configuredDevice == SupportedDevices::DeviceTypeLightpack;
}

} // namespace DeviceDiscoveryDefault
