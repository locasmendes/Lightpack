/*
 * ScreenTopology.hpp
 *
 * Phase 1 (multi-screen): pure decisions about which screens contain LED zones
 * and whether a previously saved "active" screen identity has returned.
 * Kept free of QObject/Settings/widgets so it is unit-testable like BulkResize.
 */

#pragma once

#include <QHash>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>

namespace ScreenTopology {

/*! Stable monitor identity (name + manufacturer + serial). */
struct Identity {
	QString name;
	QString manufacturer;
	QString serialNumber;

	bool isEmpty() const;
	bool operator==(const Identity &other) const;
	bool operator!=(const Identity &other) const { return !(*this == other); }

	/*! Persist as "name|manufacturer|serial" (pipe-separated). */
	QString toSettingsString() const;
	static Identity fromSettingsString(const QString &raw);
};

struct ScreenEntry {
	Identity id;
	QRect geometry;
};

/*! Default consecutive grab-miss ticks before treating zones as screen-less. */
constexpr int kNoScreenTurnOffThreshold = 3;

/*!
 * Returns identities of screens whose geometry contains at least one zone center.
 * Screens are keyed by a stable string (stand-in for QScreen* in production).
 */
QList<Identity> screensContainingZones(
	const QList<QPoint> &zoneCenters,
	const QHash<QString, ScreenEntry> &screensByKey);

bool anyZoneHasValidScreen(
	const QList<QPoint> &zoneCenters,
	const QHash<QString, ScreenEntry> &screensByKey);

/*! True when savedActive is non-empty and matches any current screen identity. */
bool activeScreenReturned(
	const Identity &savedActive,
	const QHash<QString, ScreenEntry> &screensByKey);

/*!
 * Screen that currently owns the most zone centers (ties: first in hash iteration
 * order among tied keys is acceptable — callers use this only for persistence).
 */
Identity primaryScreenForZones(
	const QList<QPoint> &zoneCenters,
	const QHash<QString, ScreenEntry> &screensByKey);

/*! Pure counter update: reset to 0 on hit, else previous+1. */
int nextConsecutiveMissCount(int previousMisses, bool hasValidScreen);

bool shouldTurnOffForMissingScreen(int consecutiveMisses, int threshold = kNoScreenTurnOffThreshold);

/*!
 * Restore lights only when we had turned them off for disconnect AND zones again
 * have a valid screen AND the saved active identity is back (or was never saved).
 */
bool shouldRestoreLightsAfterReconnect(
	bool lightsWereTurnedOffForDisconnect,
	bool hasValidScreen,
	bool activeReturnedOrUntracked);

} // namespace ScreenTopology
