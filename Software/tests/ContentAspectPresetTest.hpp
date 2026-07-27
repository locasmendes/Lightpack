#ifndef CONTENTASPECTPRESETTEST_HPP
#define CONTENTASPECTPRESETTEST_HPP

#include <QObject>

class ContentAspectPresetTest : public QObject
{
	Q_OBJECT
public:
	explicit ContentAspectPresetTest(QObject *parent = 0);

private slots:
	void testFillReturnsMonitorUnchanged();
	void test16x9OnWiderMonitor();
	void test4x3OnWiderMonitor();
	void testExactRatioMatchIsUnchanged();
	void testNarrowerMonitorLetterboxes();
	void testNonZeroMonitorOrigin();
	void testRotatedResolutionStaysCenteredWithinOnePixel();
	void testNormalizeDefaultsUnknownToFill();
};

#endif // CONTENTASPECTPRESETTEST_HPP
