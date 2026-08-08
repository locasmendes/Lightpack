#ifndef COLORPIPELINEGOLDENTEST_HPP
#define COLORPIPELINEGOLDENTEST_HPP

#include <QObject>

class ColorPipelineGoldenTest : public QObject
{
	Q_OBJECT
public:
	explicit ColorPipelineGoldenTest(QObject *parent = nullptr);

private slots:
	/*! Frozen legacy content path vs ColorPipeline::processContent (no overbrighten). */
	void testContentStageMatchesLegacy_data();
	void testContentStageMatchesLegacy();

	/*! Full chain: legacy content+device vs processContent+applyDeviceStage with paired OutputGamma. */
	void testFullChainMatchesLegacy_data();
	void testFullChainMatchesLegacy();

	/*! Device stage alone: frozen legacy vs ColorOps::applyDeviceStageFromEncoded. */
	void testDeviceStageMatchesLegacy_data();
	void testDeviceStageMatchesLegacy();

	/*! B4 overbrighten: linear peak gain is monotonic in the control (§2.11 exclude from golden). */
	void testOverbrightenMonotonic();
};

#endif
