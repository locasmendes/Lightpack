/*
 * SmoothingDriver.cpp
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#include "SmoothingDriver.hpp"

SmoothingDriver::SmoothingDriver(QObject *parent)
	: QObject(parent)
{
	m_timer.setTimerType(Qt::PreciseTimer);
	connect(&m_timer, &QTimer::timeout, this, &SmoothingDriver::onTick);
	m_clock.start();
}

void SmoothingDriver::reset(int numberOfLeds)
{
	m_timer.stop();
	m_engine.reset(numberOfLeds);
}

void SmoothingDriver::setDurationMs(int ms)
{
	m_engine.changeDurationAndRetarget(ms, m_clock.elapsed());
	if (m_engine.isActive()) {
		if (m_enabled && !m_timer.isActive())
			m_timer.start(16);
	} else {
		m_timer.stop();
		emitDisplayed();
	}
}

void SmoothingDriver::setSendAlways(bool sendAlways)
{
	m_sendAlways = sendAlways;
}

void SmoothingDriver::setEnabled(bool enabled)
{
	m_enabled = enabled;
	if (!enabled) {
		m_timer.stop();
		m_engine.setDisplayedImmediately(m_engine.displayedColors());
	}
}

void SmoothingDriver::onColors(const QList<LinearRgbF> &colors)
{
	if (!m_enabled || m_engine.durationMs() <= 0) {
		m_engine.setDisplayedImmediately(colors);
		emit colorsUpdated(m_engine.displayedColors());
		return;
	}

	m_engine.retarget(colors, m_clock.elapsed());
	if (m_engine.isActive() && !m_timer.isActive())
		m_timer.start(16);
	else if (!m_engine.isActive())
		emit colorsUpdated(m_engine.displayedColors());
}

void SmoothingDriver::emitDisplayed()
{
	emit colorsUpdated(m_engine.displayedColors());
}

void SmoothingDriver::onTick()
{
	const bool changed = m_engine.advance(m_clock.elapsed());
	if (changed || m_sendAlways)
		emit colorsUpdated(m_engine.displayedColors());
	if (!m_engine.isActive())
		m_timer.stop();
}
