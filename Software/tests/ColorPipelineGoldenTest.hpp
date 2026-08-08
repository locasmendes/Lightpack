#ifndef COLORPIPELINEGOLDENTEST_HPP
#define COLORPIPELINEGOLDENTEST_HPP

#include <QObject>

class ColorPipelineGoldenTest : public QObject
{
	Q_OBJECT
public:
	explicit ColorPipelineGoldenTest(QObject *parent = nullptr);

private slots:
	void testLegacyOracleMatchesLive_data();
	void testLegacyOracleMatchesLive();
	/*! Step 5: new WireRgbF device stage vs frozen legacy (paired OutputGamma). */
	void testDeviceStageMatchesLegacy_data();
	void testDeviceStageMatchesLegacy();
};

#endif
