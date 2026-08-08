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
#include "ColorOps.hpp"
#include "Settings.hpp"
#include <QTime>
#include "MoodLamp.hpp"

using namespace SettingsScope;

namespace {
	QList<LinearRgbF> toLinearList(const QList<QRgb> &colors)
	{
		QList<LinearRgbF> out;
		out.reserve(colors.size());
		for (QRgb c : colors)
			out.append(ColorOps::srgbDecode(c));
		return out;
	}
}

MoodLampManager::MoodLampManager(QObject *parent) : QObject(parent)
{
	m_isMoodLampEnabled = false;

	m_timer.setTimerType(Qt::PreciseTimer);
	connect(&m_timer, &QTimer::timeout, this, qOverload<>(&MoodLampManager::updateColors));

	m_smoothingDriver = new SmoothingDriver(this);
	connect(m_smoothingDriver, &SmoothingDriver::colorsUpdated,
		this, &MoodLampManager::updateLedsColors);
	m_smoothingDriver->setDurationMs(Settings::getGrabHostSmoothingDuration());
	syncHostSmoothingEnabled();

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

	if (m_isMoodLampEnabled && m_colorMode == SettingsScope::MoodLampColorMode::Liquid)
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

void MoodLampManager::setColorMode(SettingsScope::MoodLampColorMode mode)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << static_cast<int>(mode);
	m_colorMode = mode;
	applyEffectiveLamp();
	emit moodlampFrametime(1000); // reset FPS to 1
	if (m_colorMode == SettingsScope::MoodLampColorMode::Liquid && m_isMoodLampEnabled)
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
	m_smoothingDriver->setSendAlways(!state);
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
	setColorMode(Settings::getMoodLampColorMode());
	m_isSendDataOnlyIfColorsChanged = Settings::isSendDataOnlyIfColorsChanges();
	m_smoothingDriver->setDurationMs(Settings::getGrabHostSmoothingDuration());
	m_smoothingDriver->setSendAlways(!m_isSendDataOnlyIfColorsChanged);
	syncHostSmoothingEnabled();

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

	int effectiveId = m_requestedLampId;
	switch (m_colorMode) {
	case SettingsScope::MoodLampColorMode::Constant:
		effectiveId = MoodLampBase::defaultLampId();
		break;
	case SettingsScope::MoodLampColorMode::Breathing:
		effectiveId = MoodLampBase::breathingLampId();
		break;
	case SettingsScope::MoodLampColorMode::Liquid:
		effectiveId = m_requestedLampId;
		break;
	}
	m_lamp = MoodLampBase::createWithID(effectiveId);
	emit moodlampFrametime(1000); // reset FPS to 1
	if (m_isMoodLampEnabled && m_lamp)
		m_timer.start(m_lamp->interval());
}

void MoodLampManager::updateColors(const bool forceUpdate)
{
	DEBUG_HIGH_LEVEL << Q_FUNC_INFO << static_cast<int>(m_colorMode);

	const QColor newColor = (m_colorMode == SettingsScope::MoodLampColorMode::Liquid)
		? m_generator.current()
		: m_currentColor;

	DEBUG_MID_LEVEL << Q_FUNC_INFO << newColor.rgb();

	bool changed = (m_lamp ? m_lamp->shine(newColor, m_colors) : false);

	// Per-group color overrides only make sense as fixed/static colors, so they only
	// apply in Constant mode (not Breathing, not Liquid); elsewhere they're ignored (the
	// configuration is kept, it simply takes effect again once Constant mode is reselected).
	if (m_colorMode == SettingsScope::MoodLampColorMode::Constant) {
		if (applyGroupColorOverrides(m_colors, Settings::getLedGroups()))
			changed = true;
	}

	if (changed || !m_isSendDataOnlyIfColorsChanged || forceUpdate) {
		const QList<LinearRgbF> linear = toLinearList(m_colors);
		m_smoothingDriver->onColors(linear);
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

void MoodLampManager::onHostSmoothingDurationChanged(int ms)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << ms;
	m_smoothingDriver->setDurationMs(ms);
}

void MoodLampManager::onConnectedDeviceChanged(const SupportedDevices::DeviceType device)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << device;

	// The Lightpack device owns its own firmware smoothing; cancel any
	// in-flight host transition immediately, mirroring GrabManager's handling
	// of the same setting (see GrabManager::onConnectedDeviceChanged).
	if (device == SupportedDevices::DeviceTypeLightpack)
		m_smoothingDriver->setDisplayedImmediately(toLinearList(m_colors));
	syncHostSmoothingEnabled();
}

void MoodLampManager::syncHostSmoothingEnabled()
{
	m_smoothingDriver->setEnabled(
		Settings::getConnectedDevice() != SupportedDevices::DeviceTypeLightpack);
}

void MoodLampManager::onMoodLampEffectParamsChanged(int lampId)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << lampId;

	// Effect params only matter for the freely-selectable Liquid-mode lamp - Constant/Breathing
	// force Static/Breathing, neither of which reads Speed/Density/Direction.
	if (m_colorMode == SettingsScope::MoodLampColorMode::Liquid && lampId == m_requestedLampId)
		applyEffectiveLamp();
}

QStringList MoodLampManager::visibleEffectParamsForLamp(int lampId)
{
	if (lampId < 0)
		return QStringList();

	if (lampId == MoodLampBase::idByName(QStringLiteral("Twinkle")))
		return { QStringLiteral("Speed"), QStringLiteral("Density") };

	if (lampId == MoodLampBase::idByName(QStringLiteral("Rainbow"))
		|| lampId == MoodLampBase::idByName(QStringLiteral("Comet"))
		|| lampId == MoodLampBase::idByName(QStringLiteral("Theater Chase")))
		return { QStringLiteral("Speed"), QStringLiteral("Direction") };

	return QStringList();
}

void MoodLampManager::initColors(int numberOfLeds)
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO << numberOfLeds;

	m_colors.clear();

	for (int i = 0; i < numberOfLeds; i++)
		m_colors << 0;

	m_smoothingDriver->reset(numberOfLeds);
}

bool MoodLampManager::applyGroupColorOverrides(QList<QRgb>& colors, const QList<SettingsScope::LedGroup>& groups)
{
	bool changed = false;
	for (const SettingsScope::LedGroup& group : groups) {
		if (!group.enabled || !group.hasColor)
			continue;

		const QRgb rgb = group.color.rgb();
		for (int id : group.memberIds) {
			if (id < 0 || id >= colors.size())
				continue;
			if (!Settings::isLedEnabled(id))
				continue;
			if (colors[id] != rgb) {
				colors[id] = rgb;
				changed = true;
			}
		}
	}
	return changed;
}

void MoodLampManager::requestLampList()
{
	QList<MoodLampLampInfo> list;
	int recommended = 0;

	MoodLampBase::populateNameList(list, recommended);

	emit lampList(list, recommended);
}
