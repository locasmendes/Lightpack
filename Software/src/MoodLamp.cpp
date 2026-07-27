/*
 * MoodLamp.cpp
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

#include <QTime>
#include <cmath>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#include <QRandomGenerator>
#endif
#include "MoodLamp.hpp"
#include "Settings.hpp"

// Needed to get around the fact that the lamps are macros
class QRandomGeneratorShim {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
	QRandomGenerator m_rnd;
public:
	void seed(quint32 seed) {
		m_rnd.seed(seed);
	}

	int bounded(int highest) {
		return m_rnd.bounded(highest);
	}

	int bounded(int lowest, int highest) {
		return m_rnd.bounded(lowest, highest);
	}
#else
public:
	void seed(quint32 seed) {
		qsrand(seed);
	}

	int bounded(int highest) {
		return qrand() % highest;
	}

	int bounded(int lowest, int highest) {
		return lowest + (qrand() % (highest - lowest + 1));
	}
#endif

};
 /*
	 _OBJ_NAME_	: class name prefix
	 _LABEL_	: name string to be displayed
	 _BODY_		: class declaration body
 */
#define DECLARE_LAMP(_OBJ_NAME_,_LABEL_,_BODY_) \
class _OBJ_NAME_ ## MoodLamp : public MoodLampBase \
{\
public:\
_OBJ_NAME_ ## MoodLamp() : MoodLampBase() {};\
~_OBJ_NAME_ ## MoodLamp() = default;\
static const char* name() { return _LABEL_; };\
static MoodLampBase* create() { return new _OBJ_NAME_ ## MoodLamp(); };\
\
_BODY_\
};\
struct _OBJ_NAME_ ## Register {\
_OBJ_NAME_ ## Register(){\
	g_moodLampMap.insert(lampID, MoodLampLampInfo(_OBJ_NAME_ ## MoodLamp::name(), _OBJ_NAME_ ## MoodLamp::create, lampID));\
	++lampID;\
}\
};\
_OBJ_NAME_ ## Register _OBJ_NAME_ ## Reg;

using namespace SettingsScope;

namespace {
	static QMap<int, MoodLampLampInfo> g_moodLampMap;
	static int lampID = 0;
}

MoodLampBase* MoodLampBase::createWithID(const int id) {
	QMap<int, MoodLampLampInfo>::const_iterator i = g_moodLampMap.constFind(id);
	if (i != g_moodLampMap.cend())
		return i.value().factory();
	qWarning() << Q_FUNC_INFO << "failed to find mood lamp ID: " << id;
	return nullptr;
}

void MoodLampBase::populateNameList(QList<MoodLampLampInfo>& list, int& recommended)
{
	list = g_moodLampMap.values();

	if (list.size() > 0)
		recommended = list[0].id;
}

int MoodLampBase::defaultLampId()
{
	QList<MoodLampLampInfo> list;
	int recommended = 0;
	populateNameList(list, recommended);
	return recommended;
}

int MoodLampBase::idByName(const QString& name)
{
	for (auto it = g_moodLampMap.constBegin(); it != g_moodLampMap.constEnd(); ++it) {
		if (it.value().name == name)
			return it.key();
	}
	return -1;
}

int MoodLampBase::breathingLampId()
{
	return idByName(QStringLiteral("Breathing"));
}

DECLARE_LAMP(Static, "Static (default)",
public:
	std::chrono::milliseconds interval() const { return 50ms; };

	bool shine(const QColor& newColor, QList<QRgb>& colors)
	{
		bool changed = false;
		for (int i = 0; i < colors.size(); i++)
		{
			QRgb rgb = Settings::isLedEnabled(i) ? newColor.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		return changed;
	};
);

DECLARE_LAMP(Fire, "Fire",
public:
	void init() {
		m_rnd.seed(QTime(0, 0, 0).secsTo(QTime::currentTime()));
	};

	bool shine(const QColor& newColor, QList<QRgb>& colors) {
		if (colors.size() < 2)
			return false;

		if (colors.size() > m_lightness.size()) {
			const int oldSize = m_lightness.size();
			m_lightness.reserve(colors.size());
			for (int i = oldSize; i < colors.size(); ++i)
				m_lightness << 0;
		}

		// heavily inspired by FastLED Fire2012 demo
		// https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
		const int centerMax = colors.size() / 4;
		const int middleLed = std::floor(colors.size() / 2);
		const int sparkCount = colors.size() / 12;

		m_center += m_rnd.bounded(2) ? -1 : 1;
		m_center = std::max(-centerMax, std::min(centerMax, m_center));

		for (int i = 0; i < middleLed + m_center; ++i) {
			const int minLightnessReduction = Cooling * std::pow((double)i / (middleLed + m_center), 3);
			const int maxLightnessReduction = minLightnessReduction * 2;
			const int lightnessReduction = minLightnessReduction + m_rnd.bounded(std::max(1, maxLightnessReduction - minLightnessReduction)) + Cooling / 3;
			m_lightness[i] = std::max(0, m_lightness[i] - lightnessReduction);
		}

		for (int i = colors.size() - 1; i >= middleLed + m_center; --i) {
			const int minLightnessReduction = Cooling * std::pow((double)(colors.size() - 1 - i) / (middleLed - m_center), 3);
			const int maxLightnessReduction = minLightnessReduction * 2;
			const int lightnessReduction = minLightnessReduction + m_rnd.bounded(std::max(1, maxLightnessReduction - minLightnessReduction)) + Cooling / 3;
			m_lightness[i] = std::max(0, m_lightness[i] - lightnessReduction);
		}


		for (int k = middleLed + m_center; k > 1; --k)
			m_lightness[k] = (m_lightness[k - 1] + m_lightness[k - 2] * 2) / 3;

		for (int k = middleLed + m_center; k < colors.size() - 2; ++k)
			m_lightness[k] = (m_lightness[k + 1] + m_lightness[k + 2] * 2) / 3;


		if (m_rnd.bounded(2) == 0) {
			int y = m_rnd.bounded(std::max(1, sparkCount));
			m_lightness[y] = std::max(SparkMax, (int)m_lightness[y] + (SparkMin + m_rnd.bounded(SparkMax - SparkMin)));
		}
		if (m_rnd.bounded(2) == 0) {
			int z = colors.size() - 1 - m_rnd.bounded(std::max(1, sparkCount));
			m_lightness[z] = std::max(SparkMax, (int)m_lightness[z] + (SparkMin + m_rnd.bounded(SparkMax - SparkMin)));
		}

		for (int i = 0; i < colors.size(); i++)
		{
			QColor color(newColor);
			color.setHsl(color.hue(), color.saturation(), m_lightness[i]);
			colors[i] = Settings::isLedEnabled(i) ? color.rgb() : 0;
		}
		return true;
	};
private:
	QRandomGeneratorShim m_rnd;
	QList<quint8> m_lightness;
	int m_center{ 0 };

	const size_t Cooling = 8;
	const int SparkMax = 160;
	const int SparkMin = 100;
);

DECLARE_LAMP(RGBLife, "RGB is Life",
public:
	bool shine(const QColor& newColor, QList<QRgb>& colors)
	{
		bool changed = false;
		const int degrees = 360 / colors.size();
		const int step = Speed * m_frames++;
		for (int i = 0; i < colors.size(); i++)
		{
			QColor color(newColor);
			color.setHsl(color.hue() + degrees * i + step, color.saturation(), color.lightness());
			QRgb rgb = Settings::isLedEnabled(i) ? color.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		if (step > 360)
			m_frames = 0;
		return changed;
	};
private:
	const double Speed = 1.5;
);

// Sinusoidal brightness pulse over a single fixed color. The only animated lamp allowed
// to run outside Liquid mode (forced by MoodLampManager while Breathing mode is active) -
// see MoodLampBase::breathingLampId().
DECLARE_LAMP(Breathing, "Breathing",
public:
	bool shine(const QColor& newColor, QList<QRgb>& colors)
	{
		const double phase = (m_frames % CyclePeriodFrames) / (double)CyclePeriodFrames;
		const double brightnessScale = 0.5 - 0.5 * std::cos(2 * M_PI * phase); // 0..1, starts/ends at 0
		m_frames++;

		QColor pulsed(newColor);
		pulsed.setHsl(pulsed.hue(), pulsed.saturation(), static_cast<int>(pulsed.lightness() * brightnessScale));

		bool changed = false;
		for (int i = 0; i < colors.size(); i++)
		{
			QRgb rgb = Settings::isLedEnabled(i) ? pulsed.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		return changed;
	};
private:
	const size_t CyclePeriodFrames = 120; // ~4s at the default 33ms tick
);

// Full hue spectrum across the strip, cycling over time - ignores newColor entirely
// (unlike RGBLife, which rotates newColor's own hue by a fixed offset per LED).
DECLARE_LAMP(Rainbow, "Rainbow",
public:
	bool shine(const QColor&, QList<QRgb>& colors)
	{
		if (colors.isEmpty())
			return false;

		const int degrees = colors.size() > 1 ? 360 / colors.size() : 0;
		const int step = Speed * m_frames++;
		bool changed = false;
		for (int i = 0; i < colors.size(); i++)
		{
			const QColor color = QColor::fromHsv((degrees * i + step) % 360, 255, 255);
			const QRgb rgb = Settings::isLedEnabled(i) ? color.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		return changed;
	};
private:
	const double Speed = 1.5;
);

// A bright head with an exponentially-decaying trail, moving along the strip.
DECLARE_LAMP(Comet, "Comet",
public:
	bool shine(const QColor& newColor, QList<QRgb>& colors)
	{
		if (colors.isEmpty())
			return false;

		if (colors.size() > m_lightness.size()) {
			const int oldSize = m_lightness.size();
			m_lightness.reserve(colors.size());
			for (int i = oldSize; i < colors.size(); ++i)
				m_lightness << 0;
		}

		for (int i = 0; i < colors.size(); i++)
			m_lightness[i] = static_cast<quint8>(m_lightness[i] * FadeFactor);

		const int headPos = m_position % colors.size();
		m_lightness[headPos] = 255;
		m_position += Speed;

		bool changed = false;
		for (int i = 0; i < colors.size(); i++)
		{
			QColor color(newColor);
			color.setHsl(color.hue(), color.saturation(), m_lightness[i]);
			const QRgb rgb = Settings::isLedEnabled(i) ? color.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		return changed;
	};
private:
	QList<quint8> m_lightness;
	int m_position{ 0 };
	const int Speed = 1;
	const double FadeFactor = 0.75;
);

// Evenly-spaced dots marquee, offset advancing by one LED per tick.
DECLARE_LAMP(TheaterChase, "Theater Chase",
public:
	bool shine(const QColor& newColor, QList<QRgb>& colors)
	{
		bool changed = false;
		const int offset = m_frames++ % SpacingLeds;
		for (int i = 0; i < colors.size(); i++)
		{
			const QRgb rgb = (Settings::isLedEnabled(i) && (i + offset) % SpacingLeds == 0) ? newColor.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		return changed;
	};
private:
	const int SpacingLeds = 3;
);

// Random pixels briefly flash to full brightness then decay, like Fire but sparse/still.
DECLARE_LAMP(Twinkle, "Twinkle",
public:
	void init() {
		m_rnd.seed(QTime(0, 0, 0).secsTo(QTime::currentTime()));
	};

	bool shine(const QColor& newColor, QList<QRgb>& colors)
	{
		if (colors.isEmpty())
			return false;

		if (colors.size() > m_lightness.size()) {
			const int oldSize = m_lightness.size();
			m_lightness.reserve(colors.size());
			for (int i = oldSize; i < colors.size(); ++i)
				m_lightness << 0;
		}

		for (int i = 0; i < colors.size(); i++)
			m_lightness[i] = static_cast<quint8>(m_lightness[i] * FadeFactor);

		if (m_rnd.bounded(100) < SpawnChancePercent) {
			const int i = m_rnd.bounded(colors.size());
			m_lightness[i] = 255;
		}

		bool changed = false;
		for (int i = 0; i < colors.size(); i++)
		{
			QColor color(newColor);
			color.setHsl(color.hue(), color.saturation(), m_lightness[i]);
			const QRgb rgb = Settings::isLedEnabled(i) ? color.rgb() : 0;
			changed = changed || (colors[i] != rgb);
			colors[i] = rgb;
		}
		return changed;
	};
private:
	QRandomGeneratorShim m_rnd;
	QList<quint8> m_lightness;
	const double FadeFactor = 0.9;
	const int SpawnChancePercent = 30;
);
