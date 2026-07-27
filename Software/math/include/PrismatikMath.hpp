/*
 * LightpackMath.hpp
 *
 *	Created on: 29.09.2011
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

#include <QList>
#include <QRgb>
#include <QPointF>
#include <cmath>
#include "colorspace_types.h"
#include "../../common/defs.h"

namespace PrismatikMath
{
	// Hue/saturation <-> Cartesian point on a color wheel of the given radius, center at
	// the origin. hue is in degrees [0,360), sat in [0,100]. Extracted from
	// ColorWheelWidget so the wheel's polar math is unit-testable without QApplication/painting.
	QPointF hueSatToPoint(int hue, int sat, double radius);
	// Inverse of hueSatToPoint. Returns false (and clamps sat to 100) if point falls
	// outside the circle of the given radius, true otherwise.
	bool pointToHueSat(const QPointF& point, double radius, int& hue, int& sat);

	void gammaCorrection(double gamma, StructRgb &);
	void brightnessCorrection(unsigned int brightness, StructRgb &);
	void maxCorrection(unsigned int max, StructRgb &);
	int getValueHSV(const QRgb rgb);
	int getChromaHSV(const QRgb rgb);
	int max(const QRgb);
	int min(const QRgb);
	QRgb withValueHSV(const QRgb, int);
	QRgb withChromaHSV(const QRgb, int);
	void applyColorTemperature(QList<QRgb>&, const quint16, double gamma);
	StructRgb whitePoint(const quint16 colorTemperature);
	StructRgb avgColor(const QList<StructRgb> &);
	StructXyz toXyz(const StructRgb &);
	StructXyz toXyz(const StructLab &);
	StructLab toLab(const StructRgb &);
	StructLab toLab(const StructXyz &);
	StructRgb toRgb(const StructXyz &);
	StructRgb toRgb(const StructLab &);
	quint8 getBrightness(const QRgb);

	// Returns 0-255: how strongly a pixel qualifies for the "bloom" effect (bright AND
	// desaturated - a true white/near-white flare-type pixel, not just any bright color).
	// whitenessThresholdPercent (0-100) is how close to pure white/gray a pixel's chroma
	// must be before it counts as bloom-worthy at all; 0 below that cutoff.
	int bloomFactor(const QRgb rgb, int whitenessThresholdPercent);
	// Boosts rgb's brightness by up to `intensity` percent (0-100), scaled by how
	// strongly it qualifies per bloomFactor(). intensity<=0 or a non-bloom-worthy pixel
	// (bloomFactor()==0) returns rgb unchanged.
	QRgb applyBloom(const QRgb rgb, int intensity, int whitenessThresholdPercent);

	// Scales chroma by factor (1.0 = unchanged, 0.0 = grayscale, >1.0 = more saturated).
	QRgb adjustSaturation(const QRgb rgb, double factor);
	// Scales each channel's distance from pivot by factor (1.0 = unchanged). Clamps to [0,255].
	QRgb adjustContrast(const QRgb rgb, double factor, int pivot = 128);
	// Like adjustSaturation, but the effective factor is pulled toward 1.0 (no change) the
	// more saturated the pixel already is, weighted by protectionStrength [0,1] (0 = behaves
	// exactly like adjustSaturation, 1 = fully-saturated pixels are left untouched).
	QRgb adjustVibrance(const QRgb rgb, double factor, double protectionStrength = 0.6);

	double theoreticalMaxFrameRate(const double ledCount, const double baudRate);
	double theoreticalMinBaudRate(const double ledCount, const double frameRate);

	// Convert ASCII char '5' to 5
	inline char getDigit(const char d)
	{
		if (isdigit(d))
			return (d - '0');
		else
			return -1;
	}

	inline double round(double number)
	{
#if defined HAVE_PLATFORM_ROUND_FUNC
		return ::round(number);
#else
		return number < 0.0 ? std::ceil(number - 0.5) : std::floor(number + 0.5);
#endif
	}
}

