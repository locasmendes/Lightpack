/*
 * MoodLampManager.cpp
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


#include "MoodLampManager.hpp"
#include "PrismatikMath.hpp"
#include "Settings.hpp"
#include <QTime>
#include "MoodLamp.hpp"

using namespace SettingsScope;

namespace {
	constexpr int HostSmoothingIntervalMs = 16; // ~62.5 Hz, matches GrabManager::HOST_SMOOTHING_INTERVAL
}

MoodLampManager::MoodLampManager(QObject *parent) : QObject(parent)
{
	m_isMoodLampEnabled = false;

	m_timer.setTimerType(Qt::PreciseTimer);
	connect(&m_timer, &QTimer::timeout, this, qOverload<>(&MoodLampManager::updateColors));

	m_timerHostSmoothing = new QTimer(this);
	m_timerHostSmoothing->setTimerType(Qt::PreciseTimer);
	connect(m_timerHostSmoothing, &QTimer::timeout, this, &MoodLampManager::advanceHostTransition);
	m_timerHostSmoothing->setSingleShot(false);
	m_timerHostSmoothing->setInterval(HostSmoothingIntervalMs);
	m_hostSmoothingClock.start();

	initFromSettings();
}

MoodLampManager::~MoodLampManager()
{
	if (m_lamp)
		delete m_lamp;
}

void MoodLampManager::start(bool isEnabled)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << isEnabled;

	m_isMoodLampEnabled = isEnabled;

	if (m_isMoodLampEnabled)
	{
		// This is usable if start is called after API unlock, and we should force set current color
		updateColors(true);
	}

	if (m_isMoodLampEnabled && m_isLiquidMode)
		m_generator.start();
	else
		m_generator.stop();

	if (m_isMoodLampEnabled && m_lamp)
		m_timer.start(m_lamp->interval());
	else
		m_timer.stop();
}

void MoodLampManager::setCurrentColor(QColor color)
{
	DEBUG_MID_LEVEL << Q_FUNC_INFO << color;

	m_currentColor = color;

	if (m_isMoodLampEnabled)
		updateColors();
}

void MoodLampManager::setLiquidMode(bool state)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << state;
	m_isLiquidMode = state;
	applyEffectiveLamp();
	emit moodlampFrametime(1000); // reset FPS to 1
	if (m_isLiquidMode && m_isMoodLampEnabled)
		m_generator.start();
	else {
		m_generator.stop();
		if (m_isMoodLampEnabled)
			updateColors();
	}
}

void MoodLampManager::setLiquidModeSpeed(int value)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << value;
	m_generator.setSpeed(value);
}

void MoodLampManager::setSendDataOnlyIfColorsChanged(bool state)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << state;
	m_isSendDataOnlyIfColorsChanged = state;
	emit moodlampFrametime(1000); // reset FPS to 1
}

void MoodLampManager::setNumberOfLeds(int numberOfLeds)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << numberOfLeds;

	initColors(numberOfLeds);
}

void MoodLampManager::reset()
{
	m_generator.reset();
}

void MoodLampManager::settingsProfileChanged(const QString &profileName)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO;
	Q_UNUSED(profileName)
	initFromSettings();
}

void MoodLampManager::initFromSettings()
{
	m_generator.setSpeed(Settings::getMoodLampSpeed());
	m_currentColor = Settings::getMoodLampColor();
	setLiquidMode(Settings::isMoodLampLiquidMode());
	m_isSendDataOnlyIfColorsChanged = Settings::isSendDataOnlyIfColorsChanges();
	m_hostSmoothing.setDurationMs(Settings::getGrabHostSmoothingDuration());

	initColors(Settings::getNumberOfLeds(Settings::getConnectedDevice()));
	setCurrentLamp(Settings::getMoodLampLamp());
}

void MoodLampManager::setCurrentLamp(const int id)
{
	m_requestedLampId = id;
	applyEffectiveLamp();
}

void MoodLampManager::applyEffectiveLamp()
{
	m_timer.stop();

	if (m_lamp) {
		delete m_lamp;
		m_lamp = nullptr;
	}

	const int effectiveId = m_isLiquidMode ? m_requestedLampId : MoodLampBase::defaultLampId();
	m_lamp = MoodLampBase::createWithID(effectiveId);
	emit moodlampFrametime(1000); // reset FPS to 1
	if (m_isMoodLampEnabled && m_lamp)
		m_timer.start(m_lamp->interval());
}

void MoodLampManager::updateColors(const bool forceUpdate)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO << m_isLiquidMode;

	QColor newColor;

	if (m_isLiquidMode)
	{
		newColor = m_generator.current();
	}
	else
	{
		newColor = m_currentColor;
	}

	DEBUG_MID_LEVEL << Q_FUNC_INFO << newColor.rgb();

	bool changed = (m_lamp ? m_lamp->shine(newColor, m_colors) : false);
	if (changed || !m_isSendDataOnlyIfColorsChanged || forceUpdate) {
		if (isHostSmoothingApplicable()) {
			m_hostSmoothing.retarget(m_colors, m_hostSmoothingClock.elapsed());
			if (!m_timerHostSmoothing->isActive())
				m_timerHostSmoothing->start();
		} else {
			m_hostSmoothing.setDisplayedImmediately(m_colors);
			emit updateLedsColors(m_hostSmoothing.displayedColors());
		}
		if (forceUpdate) {
			emit moodlampFrametime(1000);
			m_elapsedTimer.restart();
			m_frames = 0;
		} else if (m_elapsedTimer.hasExpired(1000)) { // 1s
			emit moodlampFrametime(m_elapsedTimer.restart() / m_frames);
			m_frames = 0;
		}
		m_frames++;
	}
}

void MoodLampManager::advanceHostTransition()
{
	const bool changed = m_hostSmoothing.advance(m_hostSmoothingClock.elapsed());

	if (changed || !m_isSendDataOnlyIfColorsChanged)
		emit updateLedsColors(m_hostSmoothing.displayedColors());

	if (!m_hostSmoothing.isActive())
		m_timerHostSmoothing->stop();
}

void MoodLampManager::onHostSmoothingDurationChanged(int ms)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << ms;

	m_hostSmoothing.changeDurationAndRetarget(ms, m_hostSmoothingClock.elapsed());

	if (m_hostSmoothing.isActive()) {
		if (!m_timerHostSmoothing->isActive())
			m_timerHostSmoothing->start();
	} else {
		m_timerHostSmoothing->stop();
		emit updateLedsColors(m_hostSmoothing.displayedColors());
	}
}

void MoodLampManager::onConnectedDeviceChanged(const SupportedDevices::DeviceType device)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << device;

	// The Lightpack device owns its own firmware smoothing; cancel any
	// in-flight host transition immediately, mirroring GrabManager's handling
	// of the same setting (see GrabManager::onConnectedDeviceChanged).
	if (device == SupportedDevices::DeviceTypeLightpack) {
		m_timerHostSmoothing->stop();
		m_hostSmoothing.setDisplayedImmediately(m_colors);
	}
}

bool MoodLampManager::isHostSmoothingApplicable() const
{
	return m_hostSmoothing.durationMs() > 0
		&& Settings::getConnectedDevice() != SupportedDevices::DeviceTypeLightpack;
}

void MoodLampManager::initColors(int numberOfLeds)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << numberOfLeds;

	m_colors.clear();

	for (int i = 0; i < numberOfLeds; i++)
		m_colors << 0;

	m_timerHostSmoothing->stop();
	m_hostSmoothing.reset(numberOfLeds);
}

void MoodLampManager::requestLampList()
{
	QList<MoodLampLampInfo> list;
	int recommended = 0;

	MoodLampBase::populateNameList(list, recommended);

	emit lampList(list, recommended);
}
