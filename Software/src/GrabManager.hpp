/*
 * GrabManager.hpp
 *
 *	Created on: 26.07.2010
 *		Project: Lightpack
 *
 *	Copyright (c) 2010, 2011 Mike Shatohin, mikeshatohin [at] gmail.com
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

#include <QtGui>
#include <QElapsedTimer>
#include <QHash>

#include "GrabberBase.hpp"
#include "HostColorSmoothing.hpp"
#include "ColorF.h"
#include "enums.hpp"

class GrabberContext;
class TimeEvaluations;
class D3D10Grabber;

namespace BlueLightReduction { class Client; };
namespace SystemSession { enum Status : int; };

class GrabManager : public QObject
{
	Q_OBJECT

public:
	GrabManager(QWidget *parent = 0);
	virtual ~GrabManager();

signals:
	void updateLedsColors(const QList<LinearRgbF> & colors);
	void ambilightTimeOfUpdatingColors(double ms);
	/*! Emitted on topology changes and when zone-screen availability flips (Phase 1). */
	void changeScreen();
	void onSessionChange(SystemSession::Status change);

public:

	// Grab options

	// Common options
	void setNumberOfLeds(int numberOfLeds);
	void reset();

	/*! True after N consecutive ticks with no screen covering any zone (Phase 1.3). */
	bool areZoneScreensMissing() const { return m_zonesScreenMissing; }

public slots:
	void onGrabberTypeChanged(const Grab::GrabberType grabberType);
	void onGrabSlowdownChanged(int ms);
	void onGrabAvgColorsEnabledChanged(bool state);
	void onGrabOverBrightenChanged(int value);
	void onGrabBloomEnabledChanged(bool state);
	void onGrabBloomIntensityChanged(int value);
	void onGrabBloomThresholdChanged(int value);
	void onGrabSaturationChanged(int value);
	void onGrabContrastChanged(int value);
	void onGrabVibranceChanged(int value);
	void onGrabContrastPivotChanged(int value);
	void onGrabVibranceProtectionChanged(int value);
	void onGrabApplyBlueLightReductionChanged(bool state);
	void onGrabApplyColorTemperatureChanged(bool state);
	void onGrabColorTemperatureChanged(int value);
	void onGrabGammaChanged(double value);
	void onSendDataOnlyIfColorsEnabledChanged(bool state);
	void onGrabHostSmoothingDurationChanged(int ms);
	void onConnectedDeviceChanged(const SupportedDevices::DeviceType device);
#ifdef D3D10_GRAB_SUPPORT
	void onDx1011GrabberEnabledChanged(bool state);
	void onDx9GrabberEnabledChanged(bool state);
#endif
	void start(bool isGrabEnabled);
	void ledDeviceCallSuccess(bool isSuccess);
	void ledDeviceOpenSuccess(bool isSuccess);
	void settingsProfileChanged(const QString &profileName);
	void setVisibleLedWidgets(bool state);
	void setColoredLedWidgets(bool state);
	void setWhiteLedWidgets(bool state);
	void setLiveColorsLedWidgets(bool state);
	void updateLiveLedColors(const QList<QRgb> & colors);
	void onGrabberStateChangeRequested(bool isStartRequested);

private slots:
	void handleGrabbedColors();
	void advanceHostTransition();
	void timeoutFakeGrab();
	void timeoutUpdateFPS();
	void pauseWhileResizeOrMoving();
	void resumeAfterResizeOrMoving();
	void onFrameGrabAttempted(GrabResult result);
	void updateScreenGeometry();
	void onScreenCountChanged(QScreen* screen);
	void onPrimaryScreenChanged(QScreen* screen);
	void onScreenGeometryChanged(const QScreen* screen, const QRect& geometry);
	void restoreLedPositionsFromSettings();

private:
	void syncScreenConnections();
	void scheduleRestoreLedPositions();
	void evaluateZoneScreenAvailability();
	void persistZoneScreenIdentity();
	QList<QPoint> zoneCenters() const;
	GrabberBase *queryGrabber(Grab::GrabberType grabber);
	void initGrabbers();
	GrabberBase *initGrabber(GrabberBase *grabber);
#ifdef D3D10_GRAB_SUPPORT
	void reinitDx1011Grabber();
#endif
	void initColorLists(int numberOfLeds);
	void clearColorsNew();
	void clearColorsCurrent();
	void initLedWidgets(int numberOfLeds);
	bool isHostSmoothingApplicable() const;

private:
	QList<GrabberBase*> m_grabbers;
	GrabberBase *m_grabber;
	/*! Phase 1.1: keyed by QScreen* so geometryChanged lambdas never hold a stale index. */
	QHash<const QScreen*, QRect> m_lastScreenGeometry;

#ifdef D3D10_GRAB_SUPPORT
	D3D10Grabber *m_d3d10Grabber;
#endif

	BlueLightReduction::Client* m_blueLightClient;

	QTimer *m_timerUpdateFPS;
	QTimer *m_timerFakeGrab;
	// TODO(Phase 2 follow-up): migrate host-smoothing orchestration to SmoothingDriver
	// (Software/src/SmoothingDriver.*) — GrabManager and MoodLampManager still own local
	// QTimer + QElapsedTimer + HostColorSmoothing triplets; SoundManagerBase has none yet.
	// Risk: Lightpack firmware-smoothing bypass (isHostSmoothingApplicable) and
	// send-always / forceUpdate edge cases must stay bit-identical. Prefer a dedicated
	// pass with HostColorSmoothingTest + MoodLampManagerTest green before swapping.
	QTimer *m_timerHostSmoothing;
	QTimer *m_timerRestoreLedPositions;
	QElapsedTimer m_hostSmoothingClock;
	HostColorSmoothing m_hostSmoothing;
	QWidget *m_parentWidget;
	QList<GrabWidget *> m_ledWidgets;
	QList<QRgb> m_grabResult;
	const static QColor m_backgroundAndTextColors[10][2];

	QList<LinearRgbF> m_colorsCurrent;
	QList<QRgb> m_colorsNew;
	QList<QRgb> m_colorsProcessing;

	int m_consecutiveNoScreenMisses = 0;
	bool m_zonesScreenMissing = false;

	bool m_isPauseGrabWhileResizeOrMoving;
	bool m_isSendDataOnlyIfColorsChanged;
	bool m_avgColorsOnAllLeds;
	bool m_isGrabbingStarted;
	bool m_isGrabbingSuspendedDueToDeviceError;
	int m_overBrighten;
	bool m_bloomEnabled;
	int m_bloomIntensity;
	int m_bloomThreshold;
	int m_saturation;
	int m_contrast;
	int m_vibrance;
	int m_contrastPivot;
	int m_vibranceProtection;
	bool m_isApplyBlueLightReduction;
	bool m_isApplyColorTemperature;
	double m_gamma;
	int m_colorTemperature;

	QList<bool> m_changeLatch;

	int m_grabCountThisInterval;
	int m_grabCountLastInterval;

	bool m_isGrabWidgetsVisible;
	bool m_isLiveColorsEnabled;
	GrabberContext * m_grabberContext;
};
