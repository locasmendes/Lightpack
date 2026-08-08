/*
 * SmoothingDriver.hpp — shared QTimer + QElapsedTimer + HostColorSmoothing orchestration.
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
	void setEnabled(bool enabled);

	void onColors(const QList<LinearRgbF> &colors);
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
