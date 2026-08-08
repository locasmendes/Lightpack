/*
 * AbstractLedDevice.hpp
 *
 *	Created on: 17.04.2011
 *		Author: Timur Sattarov && Mike Shatohin
 *		Project: Lightpack
 *
 *	Lightpack is very simple implementation of the backlight for a laptop
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
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
#include "colorspace_types.h"
#include "types.h"
#include "SettingsDefaults.hpp"
#include "ColorF.h"
#include "ColorOps.hpp"
/*!
	Abstract class representing any LED device.
	\a LedDeviceManager
*/
class AbstractLedDevice : public QObject
{
	Q_OBJECT
public:
	AbstractLedDevice(QObject * parent) : QObject(parent) {}
	virtual ~AbstractLedDevice(){}
	virtual QString name() const = 0;
	virtual int defaultLedsCount() = 0;
	virtual int maxLedsCount() = 0;

signals:
	void openDeviceSuccess(bool isSuccess);
	void ioDeviceSuccess(bool isSuccess);
	void firmwareVersion(const QString & fwVersion);
	void firmwareVersionUnofficial(const int version);

	/*!
		This signal must be sent at the completion of each command
		(setColors, setTimerOptions, setColorDepth, setSmoothSlowdown, etc.)
		\param ok is command completed successfully
	*/
	void commandCompleted(bool ok);
	void colorsUpdated(QList<QRgb> colors);

public slots:
	virtual void open() = 0;
	virtual void close() = 0;
	virtual void setColors(const QList<LinearRgbF> & colors) = 0;
	/*! Compatibility overload for API / plugins / wizard (decodes then forwards). */
	void setColors(const QList<QRgb> & colors);
	virtual void switchOffLeds() = 0;

	/*!
		\obsolete only form compatibility with Lightpack ver.<=5.5 hardware
		PWM timer period.
		\param value in millis
	*/
	virtual void setRefreshDelay(int value) = 0;
	virtual void setSmoothSlowdown(int value) = 0;
	virtual void setGamma(double value, bool updateColors = true);
	virtual void setBrightness(int value, bool updateColors = true);
	virtual void setBrightnessCap(int value, bool updateColors = true);
	virtual void setDitheringEnabled(bool value, bool updateColors = true);
	virtual void setLedMilliAmps(const int value, const bool updateColors = true);
	virtual void setPowerSupplyAmps(const double value, const bool updateColors = true);
	virtual void setColorSequence(const QString& value) = 0;
	virtual void setLuminosityThreshold(int value, bool updateColors = true);
	virtual void setMinimumLuminosityThresholdEnabled(bool value, bool updateColors = true);
	virtual void updateWBAdjustments(); // Reads from settings
	virtual void updateWBAdjustments(const QList<WBAdjustment> &coefs, bool updateColors = true);
	virtual void requestFirmwareVersion() = 0;
	virtual void updateDeviceSettings();


	/*!
		\obsolete only form compatibility with Lightpack ver.<=5.5 hardware
		\param value bits per channel
	*/
	virtual void setColorDepth(int value) = 0;


	virtual void setUsbPowerLedDisabled(bool isDisabled);

	/*! Phase 4: emit colorsUpdated only while a UI watcher is active (Live colors / Stage / calibration). */
	void setColorFeedbackEnabled(bool enabled);
	bool isColorFeedbackEnabled() const { return m_colorFeedbackEnabled; }

protected:
	virtual void applyColorModifications(const QList<QRgb> & inColors, QList<StructRgb> & outColors, const bool rawColors = false);
	/*! Float-transport entry (step 7); same D1–D5 path without sRGB decode. */
	void applyColorModifications(const QList<LinearRgbF> & inColors, QList<StructRgb> & outColors);
	virtual void applyDithering(QList<StructRgb>& colors, int colorDepth);
	ColorOps::DeviceStageParams deviceStageParams() const;
	/*! Convert post-pipeline StructRgb (N-bit) to display QRgb and emit if feedback is enabled. */
	void emitColorsUpdatedIfEnabled(const QList<StructRgb> & colors, int colorDepth);
	void emitBlackColorsUpdatedIfEnabled(int count);

protected:
	QString m_colorSequence;
	/*! Holds Device/OutputGamma (unified render transform γ_out). */
	double m_gamma{ SettingsScope::Profile::Device::OutputGammaDefault };
	int m_brightness{ SettingsScope::Profile::Device::BrightnessDefault };
	int m_brightnessCap{ SettingsScope::Profile::Device::BrightnessCapDefault };
	int m_ledMilliAmps{ SettingsScope::Main::Device::LedMilliAmpsDefault };
	double m_powerSupplyAmps{ SettingsScope::Main::Device::PowerSupplyAmpsDefault };
	int m_luminosityThreshold{ SettingsScope::Profile::Grab::LuminosityThresholdDefault };
	bool m_isMinimumLuminosityEnabled{ SettingsScope::Profile::Grab::IsMinimumLuminosityEnabledDefault };
	bool m_isDitheringEnabled{ SettingsScope::Profile::Device::IsDitheringEnabledDefault };

	QList<WBAdjustment> m_wbAdjustments;

	QList<LinearRgbF> m_colorsSaved;
	QList<StructRgb> m_colorsBuffer;
	bool m_colorFeedbackEnabled{ false };
};
