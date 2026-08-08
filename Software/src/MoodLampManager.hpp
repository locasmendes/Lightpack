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
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>
#include "LiquidColorGenerator.hpp"
#include "MoodLamp.hpp"
#include "HostColorSmoothing.hpp"
#include "ColorF.h"
#include "enums.hpp"

namespace SettingsScope { struct LedGroup; }

class MoodLampManager : public QObject
{
	Q_OBJECT
public:
	explicit MoodLampManager(QObject *parent = 0);
	~MoodLampManager();

signals:
	void updateLedsColors(const QList<LinearRgbF> & colors);
	void lampList(const QList<MoodLampLampInfo> &, int);
	void moodlampFrametime(const double frameMs);

public:
	void start(bool isMoodLampEnabled);

	// Common options
	void reset();

public slots:
	void initFromSettings();
	void setColorMode(SettingsScope::MoodLampColorMode mode);
	void setLiquidModeSpeed(int value);
	void settingsProfileChanged(const QString &profileName);
	void setNumberOfLeds(int value);
	void setCurrentColor(QColor color);
	void setCurrentLamp(const int id);
	void requestLampList();
	void setSendDataOnlyIfColorsChanged(bool state);
	void onHostSmoothingDurationChanged(int ms);
	void onConnectedDeviceChanged(const SupportedDevices::DeviceType device);
	// Recreates the active lamp so a just-edited Speed/Density/Direction value (advanced
	// mode) takes effect immediately instead of only on the next explicit effect switch.
	void onMoodLampEffectParamsChanged(int lampId);

public:
	// Overwrites colors[id] for every enabled LedGroup with hasColor=true, for each id in
	// memberIds (bounds/enabled-checked, last group wins on overlap). Pure/static so it's
	// unit-testable without a live MoodLampManager/QTimer. Only meaningful in Constant
	// color mode - callers must gate on that themselves (see updateColors()).
	static bool applyGroupColorOverrides(QList<QRgb>& colors, const QList<SettingsScope::LedGroup>& groups);
	// Which of "Speed"/"Density"/"Direction" are meaningful for a given lamp id, in display
	// order - only the 4 parametrized effects (Rainbow/Comet/Theater Chase/Twinkle) return
	// anything. Pure/static so the advanced-mode UI's field visibility is unit-testable
	// without constructing a settings window.
	static QStringList visibleEffectParamsForLamp(int lampId);

private slots:
	void updateColors(const bool forceUpdate);
	void updateColors() { updateColors(false); };
	void advanceHostTransition();

private:
	void initColors(int numberOfLeds);
	// Recreates m_lamp from m_requestedLampId, except in Constant/Breathing mode: lamp
	// effects (Fire/RGB is Life/Rainbow/...) animate every tick regardless of which color
	// they're fed, so those two modes force a specific non-freely-selectable lamp (Static
	// for Constant, Breathing for Breathing) to actually behave as advertised, without
	// touching the persisted MoodLampLamp preference (restored as soon as Liquid mode is
	// re-enabled).
	void applyEffectiveLamp();
	bool isHostSmoothingApplicable() const;

private:
	MoodLampBase* m_lamp{ nullptr };
	int m_requestedLampId{ 0 };

	LiquidColorGenerator m_generator;
	QList<QRgb> m_colors;

	bool	m_isMoodLampEnabled;
	QColor  m_currentColor;
	SettingsScope::MoodLampColorMode m_colorMode{ SettingsScope::MoodLampColorMode::Constant };
	bool	m_isSendDataOnlyIfColorsChanged;

	QTimer m_timer;
	QElapsedTimer m_elapsedTimer;
	size_t m_frames{ 1 };

	// TODO(Phase 2 follow-up): migrate host-smoothing orchestration to SmoothingDriver
	// (see GrabManager.hpp / SmoothingDriver.hpp). Local QTimer triplet kept for now.
	QTimer *m_timerHostSmoothing;
	QElapsedTimer m_hostSmoothingClock;
	HostColorSmoothing m_hostSmoothing;
};
