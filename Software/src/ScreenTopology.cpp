/*
 * ScreenTopology.cpp
 *
 * Phase 1 (multi-screen): pure topology helpers.
 */

#include "ScreenTopology.hpp"

namespace ScreenTopology {

bool Identity::isEmpty() const
{
	return name.isEmpty() && manufacturer.isEmpty() && serialNumber.isEmpty();
}

bool Identity::operator==(const Identity &other) const
{
	return name == other.name
		&& manufacturer == other.manufacturer
		&& serialNumber == other.serialNumber;
}

QString Identity::toSettingsString() const
{
	if (isEmpty())
		return QString();
	return name + QLatin1Char('|') + manufacturer + QLatin1Char('|') + serialNumber;
}

Identity Identity::fromSettingsString(const QString &raw)
{
	Identity id;
	if (raw.isEmpty())
		return id;

	const QStringList parts = raw.split(QLatin1Char('|'));
	if (parts.size() >= 1)
		id.name = parts.at(0);
	if (parts.size() >= 2)
		id.manufacturer = parts.at(1);
	if (parts.size() >= 3)
		id.serialNumber = parts.mid(2).join(QLatin1Char('|'));
	return id;
}

static bool containsCenter(const QRect &geometry, const QPoint &center)
{
	return geometry.contains(center);
}

QList<Identity> screensContainingZones(
	const QList<QPoint> &zoneCenters,
	const QHash<QString, ScreenEntry> &screensByKey)
{
	QList<Identity> result;
	for (auto it = screensByKey.constBegin(); it != screensByKey.constEnd(); ++it) {
		const ScreenEntry &entry = it.value();
		for (const QPoint &center : zoneCenters) {
			if (containsCenter(entry.geometry, center)) {
				result.append(entry.id);
				break;
			}
		}
	}
	return result;
}

bool anyZoneHasValidScreen(
	const QList<QPoint> &zoneCenters,
	const QHash<QString, ScreenEntry> &screensByKey)
{
	if (zoneCenters.isEmpty() || screensByKey.isEmpty())
		return false;

	for (const QPoint &center : zoneCenters) {
		for (auto it = screensByKey.constBegin(); it != screensByKey.constEnd(); ++it) {
			if (containsCenter(it.value().geometry, center))
				return true;
		}
	}
	return false;
}

bool activeScreenReturned(
	const Identity &savedActive,
	const QHash<QString, ScreenEntry> &screensByKey)
{
	if (savedActive.isEmpty())
		return false;

	for (auto it = screensByKey.constBegin(); it != screensByKey.constEnd(); ++it) {
		if (it.value().id == savedActive)
			return true;
	}
	return false;
}

Identity primaryScreenForZones(
	const QList<QPoint> &zoneCenters,
	const QHash<QString, ScreenEntry> &screensByKey)
{
	Identity best;
	int bestCount = 0;

	for (auto it = screensByKey.constBegin(); it != screensByKey.constEnd(); ++it) {
		int count = 0;
		for (const QPoint &center : zoneCenters) {
			if (containsCenter(it.value().geometry, center))
				++count;
		}
		if (count > bestCount) {
			bestCount = count;
			best = it.value().id;
		}
	}
	return best;
}

int nextConsecutiveMissCount(int previousMisses, bool hasValidScreen)
{
	if (hasValidScreen)
		return 0;
	return previousMisses + 1;
}

bool shouldTurnOffForMissingScreen(int consecutiveMisses, int threshold)
{
	return consecutiveMisses >= threshold;
}

bool shouldRestoreLightsAfterReconnect(
	bool lightsWereTurnedOffForDisconnect,
	bool hasValidScreen,
	bool activeReturnedOrUntracked)
{
	return lightsWereTurnedOffForDisconnect
		&& hasValidScreen
		&& activeReturnedOrUntracked;
}

} // namespace ScreenTopology
