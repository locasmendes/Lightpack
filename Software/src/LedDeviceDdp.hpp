/*
 * LedDeviceDdp.hpp
 *
 *	Created on: 27.07.2026
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

#include "AbstractLedDeviceUdp.hpp"
#include "colorspace_types.h"

class LedDeviceDdp : public AbstractLedDeviceUdp
{
	Q_OBJECT
public:
	LedDeviceDdp(const QString& address, const QString& port, const uint8_t timeout, QObject * parent = 0);
	QString name() const;
	int maxLedsCount();

	// Pure packet-building logic, exposed for unit testing without any network I/O:
	// segments already color-modified/dithered pixels into DDP datagrams (10-byte
	// header + up to 1440 bytes RGB payload each), exactly as setColors() sends them.
	// sequenceStart is the sequence number for the first packet; each subsequent
	// packet in the returned list increments it (wrapping 15 -> 1). Push flag (bit 0
	// of byte 0) is set only on the last packet.
	static QList<QByteArray> buildPackets(const QList<StructRgb>& colors, uint8_t sequenceStart);

	constexpr static const int LedsPerPacket = 480;

public slots:
	void setColors(const QList<LinearRgbF> & colors) override;

protected:
	virtual void reinitBufferHeader();

	uint8_t m_sequenceNumber { 1 };
};
