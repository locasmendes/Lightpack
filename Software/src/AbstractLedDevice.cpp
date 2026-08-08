/*
 * AbstractLedDevice.cpp
 *
 *	Created on: 05.02.2013
 *		Project: Prismatik
 *
 *	Copyright (c) 2013 Timur Sattarov, tim.helloworld [at] gmail.com
 *
 *	Lightpack is an open-source, USB content-driving ambient lighting
 *	hardware.
 *
 *	Prismatik is a free, open-source software: you can redistribute it and/or 
 *	modify it under the terms of the GNU General Public License as published 
 *	by the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Prismatik and Lightpack files is distributed in the hope that it will be
 *	useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU 
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "AbstractLedDevice.hpp"
#include "colorspace_types.h"
#include "ColorOps.hpp"
#include "Settings.hpp"

void AbstractLedDevice::setUsbPowerLedDisabled(bool isDisabled) {
	Q_UNUSED(isDisabled);
	emit commandCompleted(true);
}

void AbstractLedDevice::setColors(const QList<QRgb> &colors)
{
	QList<LinearRgbF> linear;
	linear.reserve(colors.size());
	for (QRgb c : colors)
		linear.append(ColorOps::srgbDecode(c));
	setColors(linear);
}

void AbstractLedDevice::setGamma(double value, bool updateColors) {
	// m_gamma holds Device/OutputGamma (unified render transform).
	m_gamma = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setBrightness(int value, bool updateColors) {
	m_brightness = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setBrightnessCap(int value, bool updateColors) {
	m_brightnessCap = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setDitheringEnabled(bool value, bool updateColors) {
	m_isDitheringEnabled = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setLedMilliAmps(const int value, const bool updateColors) {
	m_ledMilliAmps = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setPowerSupplyAmps(const double value, const bool updateColors) {
	m_powerSupplyAmps = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setLuminosityThreshold(int value, bool updateColors) {
	m_luminosityThreshold = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::setMinimumLuminosityThresholdEnabled(bool value, bool updateColors) {
	m_isMinimumLuminosityEnabled = value;
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::updateWBAdjustments() {
	updateWBAdjustments(SettingsScope::Settings::getLedCoefs());
}

void AbstractLedDevice::updateWBAdjustments(const QList<WBAdjustment> &coefs, bool updateColors) {
	m_wbAdjustments.clear();
	m_wbAdjustments.append(coefs);
	if (updateColors)
		setColors(m_colorsSaved);
}

void AbstractLedDevice::updateDeviceSettings()
{
	using namespace SettingsScope;
	setGamma(Settings::getDeviceOutputGamma(), false);
	setBrightness(Settings::getDeviceBrightness(), false);
	setBrightnessCap(Settings::getDeviceBrightnessCap(), false);
	setLuminosityThreshold(Settings::getLuminosityThreshold(), false);
	setMinimumLuminosityThresholdEnabled(Settings::isMinimumLuminosityEnabled(), false);
	setDitheringEnabled(Settings::isDeviceDitheringEnabled(), false);
	updateWBAdjustments(Settings::getLedCoefs(), false);

	setColors(m_colorsSaved);
}

ColorOps::DeviceStageParams AbstractLedDevice::deviceStageParams() const
{
	ColorOps::DeviceStageParams p;
	p.outputGamma = static_cast<float>(m_gamma);
	p.brightnessPercent = m_brightness;
	p.brightnessCapPercent = static_cast<float>(m_brightnessCap);
	p.luminosityThreshold = m_luminosityThreshold;
	p.minimumLuminosityEnabled = m_isMinimumLuminosityEnabled;
	p.ledMilliAmps = static_cast<float>(m_ledMilliAmps);
	p.powerSupplyAmps = static_cast<float>(m_powerSupplyAmps);
	if (!m_wbAdjustments.isEmpty()) {
		p.wb.reserve(m_wbAdjustments.size());
		for (const WBAdjustment &wb : m_wbAdjustments)
			p.wb.append(ColorOps::WhiteBalanceCoef{
				static_cast<float>(wb.red),
				static_cast<float>(wb.green),
				static_cast<float>(wb.blue)});
	}
	return p;
}

/*!
	Modifies colors according to OutputGamma, luminosity threshold, white balance and brightness.
	Modifications run in WireRgbF; \a outColors receives 12-bit codes (legacy buffer scale).
*/
void AbstractLedDevice::applyColorModifications(const QList<QRgb> &inColors, QList<StructRgb> &outColors, const bool rawColors) {

	outColors.resize(inColors.size());

	if (rawColors) {
		// R9 bridge: expand 8→12 without the device stage (UDP switchOff zeros path).
		const constexpr double k = 4095 / 255.0;
		for (int i = 0; i < inColors.count(); i++) {
			outColors[i].r = qRed(inColors[i]) * k;
			outColors[i].g = qGreen(inColors[i]) * k;
			outColors[i].b = qBlue(inColors[i]) * k;
		}
		return;
	}

	ColorOps::applyDeviceStageFromEncoded(inColors, outColors, deviceStageParams());
}

void AbstractLedDevice::applyColorModifications(const QList<LinearRgbF> &inColors, QList<StructRgb> &outColors)
{
	outColors.resize(inColors.size());
	ColorOps::applyDeviceStage(inColors, outColors, deviceStageParams());
}

void AbstractLedDevice::applyDithering(QList<StructRgb>& colors, int colorDepth)
{
	// Convert 12-bit buffer → wire → quantize / dither at target depth (R7).
	QList<WireRgbF> wire;
	wire.reserve(colors.size());
	for (const StructRgb &c : colors) {
		wire.append(WireRgbF{
			ColorOps::clamp01(c.r / 4095.f),
			ColorOps::clamp01(c.g / 4095.f),
			ColorOps::clamp01(c.b / 4095.f)});
	}

	if (m_isDitheringEnabled) {
		ColorOps::quantizeDithered(wire, colorDepth, colors);
	} else {
		colors.resize(wire.size());
		for (int i = 0; i < wire.size(); ++i)
			ColorOps::quantize(wire[i], colorDepth, colors[i]);
	}
}
