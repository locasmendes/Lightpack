/*
 * MoodLampManager.hpp
 *
 *	Created on: 11.12.2011
 *		Project: Lightpack
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack a USB content-driving ambient lighting system
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <QObject>
#include <QColor>
#include <QTimer>
#include <QElapsedTimer>
#include "LiquidColorGenerator.hpp"
#include "MoodLamp.hpp"
#include "HostColorSmoothing.hpp"
#include "enums.hpp"

namespace SettingsScope { struct LedGroup; }

class MoodLampManager : public QObject
{
	Q_OBJECT
public:
	explicit MoodLampManager(QObject *parent = 0);
	~MoodLampManager();

signals:
	void updateLedsColors(const QList<QRgb> & colors);
	void lampList(const QList<MoodLampLampInfo> &, int);
	void moodlampFrametime(const double frameMs);

public:
	void start(bool isMoodLampEnabled);

	// Common options
	void reset();

public slots:
	void initFromSettings();
	void setLiquidMode(bool isEnabled);
	void setLiquidModeSpeed(int value);
	void settingsProfileChanged(const QString &profileName);
	void setNumberOfLeds(int value);
	void setCurrentColor(QColor color);
	void setCurrentLamp(const int id);
	void requestLampList();
	void setSendDataOnlyIfColorsChanged(bool state);
	void onHostSmoothingDurationChanged(int ms);
	void onConnectedDeviceChanged(const SupportedDevices::DeviceType device);

public:
	// Overwrites colors[id] for every enabled LedGroup with hasColor=true, for each id in
	// memberIds (bounds/enabled-checked, last group wins on overlap). Pure/static so it's
	// unit-testable without a live MoodLampManager/QTimer. Only meaningful in Constant
	// color mode - callers must gate on that themselves (see updateColors()).
	static bool applyGroupColorOverrides(QList<QRgb>& colors, const QList<SettingsScope::LedGroup>& groups);

private slots:
	void updateColors(const bool forceUpdate);
	void updateColors() { updateColors(false); };
	void advanceHostTransition();

private:
	void initColors(int numberOfLeds);
	// Recreates m_lamp from m_requestedLampId, except while !m_isLiquidMode:
	// lamp effects (Fire/RGB is Life) animate every tick regardless of which
	// color they're fed, so "Constant color" forces Static (the only effect
	// with no per-tick animation) to actually hold still, without touching
	// the persisted MoodLampLamp preference (restored as soon as Liquid mode
	// is re-enabled).
	void applyEffectiveLamp();
	bool isHostSmoothingApplicable() const;

private:
	MoodLampBase* m_lamp{ nullptr };
	int m_requestedLampId{ 0 };

	LiquidColorGenerator m_generator;
	QList<QRgb> m_colors;

	bool	m_isMoodLampEnabled;
	QColor  m_currentColor;
	bool	m_isLiquidMode;
	bool	m_isSendDataOnlyIfColorsChanged;

	QTimer m_timer;
	QElapsedTimer m_elapsedTimer;
	size_t m_frames{ 1 };

	QTimer *m_timerHostSmoothing;
	QElapsedTimer m_hostSmoothingClock;
	HostColorSmoothing m_hostSmoothing;
};
