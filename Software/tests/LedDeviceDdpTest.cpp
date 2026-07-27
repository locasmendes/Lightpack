#include "LedDeviceDdpTest.hpp"
#include "LedDeviceDdp.hpp"
#include "colorspace_types.h"
#include <QtTest>

namespace
{
	StructRgb rgb(unsigned r, unsigned g, unsigned b)
	{
		StructRgb color;
		color.r = r;
		color.g = g;
		color.b = b;
		return color;
	}

	QList<StructRgb> makeColors(int count, unsigned r = 1, unsigned g = 2, unsigned b = 3)
	{
		QList<StructRgb> colors;
		colors.reserve(count);
		for (int i = 0; i < count; i++)
			colors << rgb(r, g, b);
		return colors;
	}

	unsigned char byteAt(const QByteArray& packet, int index)
	{
		return static_cast<unsigned char>(packet.at(index));
	}

	quint32 readOffset(const QByteArray& packet)
	{
		return (quint32(byteAt(packet, 4)) << 24)
			| (quint32(byteAt(packet, 5)) << 16)
			| (quint32(byteAt(packet, 6)) << 8)
			| quint32(byteAt(packet, 7));
	}

	quint16 readLength(const QByteArray& packet)
	{
		return (quint16(byteAt(packet, 8)) << 8) | quint16(byteAt(packet, 9));
	}
}

LedDeviceDdpTest::LedDeviceDdpTest(QObject *parent) :
	QObject(parent)
{
}

void LedDeviceDdpTest::testSinglePacketUnderLimit()
{
	const QList<StructRgb> colors = { rgb(10, 20, 30), rgb(40, 50, 60), rgb(70, 80, 90) };
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(colors, 1);

	QCOMPARE(packets.count(), 1);
	const QByteArray& packet = packets[0];
	QCOMPARE(packet.count(), 10 + 3 * 3);

	QCOMPARE(byteAt(packet, 0), (unsigned char)0x41); // version 1 (0x40) | push (0x01), only packet
	QCOMPARE(byteAt(packet, 1), (unsigned char)0x01); // sequence start
	QCOMPARE(byteAt(packet, 2), (unsigned char)0x0B); // DDP_TYPE_RGB24
	QCOMPARE(byteAt(packet, 3), (unsigned char)0x01); // DDP_ID_DISPLAY
	QCOMPARE(readOffset(packet), (quint32)0);
	QCOMPARE(readLength(packet), (quint16)9);

	QCOMPARE(byteAt(packet, 10), (unsigned char)10);
	QCOMPARE(byteAt(packet, 11), (unsigned char)20);
	QCOMPARE(byteAt(packet, 12), (unsigned char)30);
	QCOMPARE(byteAt(packet, 13), (unsigned char)40);
	QCOMPARE(byteAt(packet, 14), (unsigned char)50);
	QCOMPARE(byteAt(packet, 15), (unsigned char)60);
	QCOMPARE(byteAt(packet, 16), (unsigned char)70);
	QCOMPARE(byteAt(packet, 17), (unsigned char)80);
	QCOMPARE(byteAt(packet, 18), (unsigned char)90);
}

void LedDeviceDdpTest::testExactly480Leds()
{
	const QList<StructRgb> colors = makeColors(480);
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(colors, 1);

	QCOMPARE(packets.count(), 1);
	const QByteArray& packet = packets[0];
	QCOMPARE(packet.count(), 10 + 1440);
	QCOMPARE(readLength(packet), (quint16)1440);
	QCOMPARE(readOffset(packet), (quint32)0);
	QCOMPARE(byteAt(packet, 0), (unsigned char)0x41); // push: only packet
}

void LedDeviceDdpTest::testMultiPacket481Leds()
{
	QList<StructRgb> colors = makeColors(480);
	colors << rgb(200, 201, 202); // LED #481 (index 480)
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(colors, 1);

	QCOMPARE(packets.count(), 2);

	const QByteArray& first = packets[0];
	QCOMPARE(readLength(first), (quint16)1440);
	QCOMPARE(readOffset(first), (quint32)0);
	QCOMPARE(byteAt(first, 0), (unsigned char)0x40); // no push: not the last packet
	QCOMPARE(byteAt(first, 1), (unsigned char)1); // sequence

	const QByteArray& second = packets[1];
	QCOMPARE(second.count(), 10 + 3);
	QCOMPARE(readLength(second), (quint16)3);
	QCOMPARE(readOffset(second), (quint32)1440); // 480 LEDs * 3 bytes
	QCOMPARE(byteAt(second, 0), (unsigned char)0x41); // push: last packet
	QCOMPARE(byteAt(second, 1), (unsigned char)2); // sequence incremented
	QCOMPARE(byteAt(second, 10), (unsigned char)200);
	QCOMPARE(byteAt(second, 11), (unsigned char)201);
	QCOMPARE(byteAt(second, 12), (unsigned char)202);
}

void LedDeviceDdpTest::testMultiPacketExactMultiple()
{
	const QList<StructRgb> colors = makeColors(960); // exactly 2 * 480, no leftover chunk
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(colors, 1);

	QCOMPARE(packets.count(), 2);
	QCOMPARE(readLength(packets[0]), (quint16)1440);
	QCOMPARE(readOffset(packets[0]), (quint32)0);
	QCOMPARE(byteAt(packets[0], 0), (unsigned char)0x40);

	QCOMPARE(readLength(packets[1]), (quint16)1440);
	QCOMPARE(readOffset(packets[1]), (quint32)1440);
	QCOMPARE(byteAt(packets[1], 0), (unsigned char)0x41);
}

void LedDeviceDdpTest::testZeroLeds()
{
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(QList<StructRgb>(), 1);

	QCOMPARE(packets.count(), 1); // header-only keepalive, mirrors DRGB's unconditional send
	const QByteArray& packet = packets[0];
	QCOMPARE(packet.count(), 10);
	QCOMPARE(readLength(packet), (quint16)0);
	QCOMPARE(byteAt(packet, 0), (unsigned char)0x41); // push: only (and last) packet
}

void LedDeviceDdpTest::testSequenceWrapsAcrossPackets()
{
	const QList<StructRgb> colors = makeColors(480 * 10); // exactly 10 packets
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(colors, 10);

	QCOMPARE(packets.count(), 10);
	const unsigned char expectedSequence[10] = { 10, 11, 12, 13, 14, 15, 1, 2, 3, 4 };
	for (int i = 0; i < packets.count(); i++)
		QCOMPARE(byteAt(packets[i], 1), expectedSequence[i]);
}

void LedDeviceDdpTest::testHeaderByteExactVector()
{
	const QList<StructRgb> colors = { rgb(10, 20, 30), rgb(40, 50, 60), rgb(70, 80, 90) };
	const QList<QByteArray> packets = LedDeviceDdp::buildPackets(colors, 1);

	QByteArray expected;
	expected.append((char)0x41).append((char)0x01).append((char)0x0B).append((char)0x01);
	expected.append((char)0x00).append((char)0x00).append((char)0x00).append((char)0x00); // offset = 0
	expected.append((char)0x00).append((char)0x09); // length = 9
	expected.append((char)10).append((char)20).append((char)30);
	expected.append((char)40).append((char)50).append((char)60);
	expected.append((char)70).append((char)80).append((char)90);

	QCOMPARE(packets.count(), 1);
	QCOMPARE(packets[0], expected);
}

void LedDeviceDdpTest::testIdenticalFramesAndOffFrameResendUnconditionally()
{
	// DDP has no diff/keep-alive logic like DNRGB (Software/src/LedDeviceDnrgb.cpp) -
	// every setColors() call resends the full frame. A black frame, as produced by
	// AbstractLedDeviceUdp::switchOffLeds(), is not special-cased or stripped: it is
	// just a normal all-zero payload.
	const QList<StructRgb> blackFrame = makeColors(5, 0, 0, 0);
	const QList<QByteArray> offPackets = LedDeviceDdp::buildPackets(blackFrame, 1);
	QCOMPARE(offPackets.count(), 1);
	QCOMPARE(offPackets[0].count(), 10 + 5 * 3);
	QCOMPARE(readLength(offPackets[0]), (quint16)(5 * 3));
	for (int i = 10; i < offPackets[0].count(); i++)
		QCOMPARE(byteAt(offPackets[0], i), (unsigned char)0);

	// Two consecutive identical frames (same colors, as two separate setColors()
	// calls would produce) are each sent in full - only the sequence number differs,
	// nothing is deduplicated or omitted.
	const QList<StructRgb> frame = makeColors(5, 100, 150, 200);
	const QList<QByteArray> firstSend = LedDeviceDdp::buildPackets(frame, 1);
	const QList<QByteArray> secondSend = LedDeviceDdp::buildPackets(frame, 2);

	QCOMPARE(firstSend.count(), 1);
	QCOMPARE(secondSend.count(), 1);
	QCOMPARE(firstSend[0].mid(2), secondSend[0].mid(2)); // identical except the sequence byte
	QVERIFY(byteAt(firstSend[0], 1) != byteAt(secondSend[0], 1));
}
