#ifndef COLORPIPELINEHYSTERESISTEST_HPP
#define COLORPIPELINEHYSTERESISTEST_HPP

#include <QObject>

class ColorPipelineHysteresisTest : public QObject
{
	Q_OBJECT
private slots:
	void testEnterExitThresholds();
	void testOneLsbOscillationProducesSingleLatch();
};

#endif
