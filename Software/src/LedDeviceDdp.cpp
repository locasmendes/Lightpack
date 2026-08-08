/*
 * LedDeviceDdp.cpp
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

#include "LedDeviceDdp.hpp"
#include "enums.hpp"

namespace
{
	// DDP header byte values, confirmed against two independent sources (see
	// docs/plans/suporte-protocolo-ddp.md §1): WLED's real receiver
	// (wled00/e131.cpp, src/dependencies/e131/ESPAsyncE131.h) and the ddp-rs
	// reference sender (coral/ddp-rs, src/protocol/frame.rs). Deliberately not
	// reusing UdpDevice::Protocol (enums.hpp): that enum carries the unrelated
	// single-byte WLED protocol codes used by WARLS/DRGB/DNRGB's own headers,
	// not DDP's 10-byte header format.
	constexpr char DdpFlagsVersion1 = 0x40;
	constexpr char DdpFlagsPush = 0x01;
	constexpr char DdpTypeRgb24 = 0x0B;
	constexpr char DdpIdDisplay = 0x01;
	constexpr int DdpHeaderLength = 10;
}

LedDeviceDdp::LedDeviceDdp(const QString& address, const QString& port, const uint8_t timeout, QObject * parent) : AbstractLedDeviceUdp(address, port, timeout, parent)
{
}

QString LedDeviceDdp::name() const
{
	return QStringLiteral("ddp");
}

int LedDeviceDdp::maxLedsCount()
{
	return MaximumNumberOfLeds::Ddp;
}

QList<QByteArray> LedDeviceDdp::buildPackets(const QList<StructRgb>& colors, uint8_t sequenceStart)
{
	QList<QByteArray> packets;

	const int totalColors = colors.count();
	// At least one packet even for zero LEDs, mirroring DRGB's unconditional send.
	const int totalPackets = qMax(1, (totalColors + LedsPerPacket - 1) / LedsPerPacket);

	uint8_t sequenceNumber = sequenceStart;
	int ledIndex = 0;
	for (int packetIndex = 0; packetIndex < totalPackets; packetIndex++)
	{
		const int ledsInPacket = qMin(LedsPerPacket, totalColors - ledIndex);
		const quint32 byteOffset = static_cast<quint32>(ledIndex) * 3;
		const quint16 payloadLength = static_cast<quint16>(ledsInPacket * 3);
		const bool isLastPacket = (packetIndex == totalPackets - 1);

		QByteArray packet;
		packet.reserve(DdpHeaderLength + payloadLength);

		packet.append((char)(DdpFlagsVersion1 | (isLastPacket ? DdpFlagsPush : 0)));
		packet.append((char)sequenceNumber);
		packet.append(DdpTypeRgb24);
		packet.append(DdpIdDisplay);
		packet.append((char)((byteOffset >> 24) & 0xFF));
		packet.append((char)((byteOffset >> 16) & 0xFF));
		packet.append((char)((byteOffset >> 8) & 0xFF));
		packet.append((char)(byteOffset & 0xFF));
		packet.append((char)((payloadLength >> 8) & 0xFF));
		packet.append((char)(payloadLength & 0xFF));

		for (int i = 0; i < ledsInPacket; i++)
		{
			const StructRgb color = colors[ledIndex + i];
			packet.append(color.r);
			packet.append(color.g);
			packet.append(color.b);
		}

		packets << packet;

		// Rolling sequence counter: 1..15, wraps to 1. 0 means "unused" per spec,
		// so it is never emitted once a device is open (see reinitBufferHeader()).
		sequenceNumber = (sequenceNumber >= 15) ? 1 : (sequenceNumber + 1);
		ledIndex += ledsInPacket;
	}

	return packets;
}

void LedDeviceDdp::setColors(const QList<LinearRgbF> & colors)
{
	m_colorsSaved = colors;

	resizeColorsBuffer(colors.count());

	applyColorModifications(colors, m_colorsBuffer);
	applyDithering(m_colorsBuffer, 8);

	const QList<QByteArray> packets = buildPackets(m_colorsBuffer, m_sequenceNumber);

	bool ok = true;
	for (const QByteArray& packet : packets)
	{
		m_writeBuffer = packet;
		ok &= writeBuffer(m_writeBuffer);

		m_sequenceNumber = (m_sequenceNumber >= 15) ? 1 : (m_sequenceNumber + 1);
	}

	emit commandCompleted(ok);
}

void LedDeviceDdp::reinitBufferHeader()
{
	// DDP has no static header prefix - every byte (push flag, sequence, offset,
	// length) varies per packet, unlike WARLS/DRGB/DNRGB's header + timeout byte.
	// Reinit just resets the rolling sequence counter on (re)connect, matching
	// AbstractLedDeviceUdp::open()'s call site.
	m_sequenceNumber = 1;
}
