#ifndef LEDDEVICEDDPTEST_HPP
#define LEDDEVICEDDPTEST_HPP

#include <QObject>

class LedDeviceDdpTest : public QObject
{
	Q_OBJECT
public:
	explicit LedDeviceDdpTest(QObject *parent = 0);

private slots:
	void testSinglePacketUnderLimit();
	void testExactly480Leds();
	void testMultiPacket481Leds();
	void testMultiPacketExactMultiple();
	void testZeroLeds();
	void testSequenceWrapsAcrossPackets();
	void testHeaderByteExactVector();
	void testIdenticalFramesAndOffFrameResendUnconditionally();
};

#endif // LEDDEVICEDDPTEST_HPP
