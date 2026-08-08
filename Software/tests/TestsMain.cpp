#include <QtTest>
#include "ColorF.h"
#include "LightpackApiTest.hpp"
#include "GrabCalculationTest.hpp"
#include "lightpackmathtest.hpp"
#include "AppVersionTest.hpp"
#ifdef Q_OS_WIN
#include "HooksTest.h"
#endif
#include "LightpackCommandLineParserTest.hpp"
#include "LedDeviceDdpTest.hpp"
#include "HostColorSmoothingTest.hpp"
#include "ContentAspectPresetTest.hpp"
#include "LayoutRecipeGeneratorTest.hpp"
#include "DeviceDiscoveryDefaultTest.hpp"
#include "MoodLampManagerTest.hpp"
#include "BulkResizeTest.hpp"
#include "LedGroupRuntimeTest.hpp"
#include "ScreenTopologyTest.hpp"
#include "ColorOpsTest.hpp"
#include "ColorPipelineGoldenTest.hpp"
#include "ColorPipelineHysteresisTest.hpp"
#include "SettingsMigrationTest.hpp"
#include "CalibrationTest.hpp"
#include "debug.h"

#include <iostream>

using namespace std;

std::atomic<unsigned> g_debugLevel{Debug::LowLevel};

int main(int argc, char *argv[])
{
	QTEST_DISABLE_KEYPAD_NAVIGATION
	qRegisterMetaType<LinearRgbF>("LinearRgbF");
	qRegisterMetaType<QList<LinearRgbF>>("QList<LinearRgbF>");
	QApplication app(argc, argv);

	QList<QObject *> tests;
	QStringList summary;

	tests.append(new GrabCalculationTest());
	tests.append(new ColorOpsTest());
	tests.append(new LightpackMathTest());
	tests.append(new ColorPipelineGoldenTest());
	tests.append(new ColorPipelineHysteresisTest());
	tests.append(new SettingsMigrationTest());
	tests.append(new LightpackApiTest());
	tests.append(new AppVersionTest());
	tests.append(new LightpackCommandLineParserTest());
	tests.append(new LedDeviceDdpTest());
	tests.append(new HostColorSmoothingTest());
	tests.append(new ContentAspectPresetTest());
	tests.append(new LayoutRecipeGeneratorTest());
	tests.append(new DeviceDiscoveryDefaultTest());
	tests.append(new MoodLampManagerTest());
	tests.append(new BulkResizeTest());
	tests.append(new LedGroupRuntimeTest());
	tests.append(new ScreenTopologyTest());
	tests.append(new CalibrationTest());

	int failedSuites = 0;
	for(int i=0; i < tests.size(); i++) {
		if (QTest::qExec(tests[i], QStringList())) {
			summary << QString(tests[i]->metaObject()->className()).append("\tFAILED");
			++failedSuites;
		} else {
			summary << QString(tests[i]->metaObject()->className()).append("\tPASSED");
		}
		delete tests[i];
	}

	for (int i = 0; i < summary.size(); ++i)
		cout << endl << summary.at(i).toLocal8Bit().constData() << endl;

	return failedSuites > 0 ? 1 : 0;
}
