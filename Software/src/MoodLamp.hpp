/*
 * MoodLamp.hpp
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

#pragma once

#include <QObject>
#include <QColor>

using namespace std::chrono_literals;
class MoodLampBase;

typedef MoodLampBase* (*LampFactory)();

struct MoodLampLampInfo {
	MoodLampLampInfo() { this->name = QLatin1String(""); this->id = -1; this->factory = nullptr; }
	MoodLampLampInfo(const QString& name, LampFactory factory, int id) { this->name = name; this->id = id; this->factory = factory; }
	QString name;
	LampFactory factory;
	int id;
};
Q_DECLARE_METATYPE(MoodLampLampInfo);
class MoodLampBase
{
public:
	MoodLampBase() { init(); };
	virtual ~MoodLampBase() = default;

	static const char* name() { return "NO_NAME"; };
	static MoodLampBase* create() { Q_ASSERT_X(false, "MoodLampBase::create()", "not implemented"); return nullptr; };
	static MoodLampBase* createWithID(const int id);
	static void populateNameList(QList<MoodLampLampInfo>& list, int& recommended);
	// Looks up a registered lamp's id by its exact name() string, or -1 if not found.
	static int idByName(const QString& name);
	// The recommended/first-registered lamp (Static) - the only effect with no
	// per-tick animation, so MoodLampManager falls back to it whenever
	// Constant color mode is active regardless of which lamp is persisted.
	static int defaultLampId();
	// The lamp MoodLampManager forces while Breathing color mode is active.
	static int breathingLampId();

	virtual void init() {};
	virtual std::chrono::milliseconds interval() const { return DefaultInterval; };
	virtual bool shine(const QColor& newColor, QList<QRgb>& colors) = 0;
protected:
	size_t m_frames{ 0 };
private:
	Q_DISABLE_COPY(MoodLampBase)
	const std::chrono::milliseconds DefaultInterval = 33ms;
};
