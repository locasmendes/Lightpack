/*
 * SmoothingDriver.hpp — shared QTimer + QElapsedTimer + HostColorSmoothing orchestration.
 *
 * Used by GrabManager, MoodLampManager, and SoundManagerBase (§2.7 / §2.13 step 8).
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#pragma once

#include "ColorF.h"
#include "HostColorSmoothing.hpp"
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class SmoothingDriver : public QObject
{
	Q_OBJECT
public:
	explicit SmoothingDriver(QObject *parent = nullptr);

	HostColorSmoothing &engine() { return m_engine; }
	const HostColorSmoothing &engine() const { return m_engine; }

	void reset(int numberOfLeds);
	void setDurationMs(int ms);
	void setSendAlways(bool sendAlways);
	/*! When false (e.g. Lightpack firmware smoothing), onColors snaps immediately. */
	void setEnabled(bool enabled);
	bool isEnabled() const { return m_enabled; }

	/*! True while a host transition timer is running. */
	bool isActive() const { return m_timer.isActive(); }

	const QList<LinearRgbF> &displayedColors() const { return m_engine.displayedColors(); }

	void onColors(const QList<LinearRgbF> &colors);
	void setDisplayedImmediately(const QList<LinearRgbF> &colors);
	void emitDisplayed();

signals:
	void colorsUpdated(const QList<LinearRgbF> &colors);

private slots:
	void onTick();

private:
	HostColorSmoothing m_engine;
	QTimer m_timer;
	QElapsedTimer m_clock;
	bool m_sendAlways = false;
	bool m_enabled = true;
};
