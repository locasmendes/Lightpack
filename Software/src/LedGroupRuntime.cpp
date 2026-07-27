/*
 * LedGroupRuntime.cpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#include "LedGroupRuntime.hpp"
#include "wizard/BulkResize.hpp"

bool LedGroupRuntime::applyGroup(const SettingsScope::LedGroup& group)
{
	if (!group.enabled)
		return true;

	const SupportedDevices::DeviceType currentDevice = SettingsScope::Settings::getConnectedDevice();
	const int maxLeds = SettingsScope::Settings::getNumberOfLeds(currentDevice);

	int targetW = -1;
	int targetH = -1;
	Qt::Corner anchor = Qt::TopLeftCorner;

	switch (group.edge) {
	case SettingsScope::LedGroup::Edge::Top:
		targetW = -1;
		targetH = group.height;
		anchor = Qt::TopLeftCorner;
		break;
	case SettingsScope::LedGroup::Edge::Bottom:
		targetW = -1;
		targetH = group.height;
		anchor = Qt::BottomLeftCorner;
		break;
	case SettingsScope::LedGroup::Edge::Left:
		targetW = group.width;
		targetH = -1;
		anchor = Qt::TopLeftCorner;
		break;
	case SettingsScope::LedGroup::Edge::Right:
		targetW = group.width;
		targetH = -1;
		anchor = Qt::TopRightCorner;
		break;
	case SettingsScope::LedGroup::Edge::Custom:
		targetW = group.width;
		targetH = group.height;
		anchor = Qt::TopLeftCorner;
		break;
	}

	for (int ledId : group.memberIds) {
		if (ledId < 0 || ledId >= maxLeds)
			continue; // silently skip invalid memberIds

		const QPoint pos = SettingsScope::Settings::getLedPosition(ledId);
		const QSize sz = SettingsScope::Settings::getLedSize(ledId);
		const QRect current(pos, sz);

		const QRect updated = BulkResize::resizedKeepingAnchor(current, targetW, targetH, anchor);

		SettingsScope::Settings::setLedPosition(ledId, updated.topLeft());
		SettingsScope::Settings::setLedSize(ledId, updated.size());
	}

	return true;
}

bool LedGroupRuntime::applyAll()
{
	const QList<SettingsScope::LedGroup> groups = SettingsScope::Settings::getLedGroups();
	if (groups.isEmpty())
		return true; // zero changes if no groups defined (regression requirement)

	for (const SettingsScope::LedGroup& g : groups) {
		applyGroup(g);
	}

	return true;
}
