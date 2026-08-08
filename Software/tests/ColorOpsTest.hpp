#ifndef COLOROPSTEST_HPP
#define COLOROPSTEST_HPP

#include <QObject>

class ColorOpsTest : public QObject
{
	Q_OBJECT
public:
	explicit ColorOpsTest(QObject *parent = nullptr);

private slots:
	void testSrgbRoundTripAndLut();
	void testSrgbKnownPoints();
	void testRenderToWire();
	void testLinearWhitePoint();
	void testPerceptualOpsIdentityAtDefault();
	void testPerceptualOpsNoIntermediateClamp();
	void testPowerLimiter();
	void testQuantize();
	void testLuminosityThresholdFloat();
};

#endif // COLOROPSTEST_HPP
