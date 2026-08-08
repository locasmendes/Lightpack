#-------------------------------------------------
#
# Project created by QtCreator 2011-09-09T12:09:24
#
#-------------------------------------------------

QT         += widgets network testlib

TARGET      = LightpackTests
DESTDIR     = bin

CONFIG     += console c++17
CONFIG     -= app_bundle

include(../build-config.prf)

CONFIG(clang) {
    QMAKE_CXXFLAGS += -stdlib=libc++
    LIBS += -stdlib=libc++
}

# QMake and GCC produce a lot of stuff
OBJECTS_DIR = stuff
MOC_DIR     = stuff
UI_DIR      = stuff
RCC_DIR     = stuff


DEFINES += SRCDIR=\\\"$$PWD/\\\"

LIBS += -L../lib -lprismatik-math -lgrab

win32 {
    CONFIG(msvc):DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
    LIBS += -ladvapi32 -luser32
}

INCLUDEPATH += . \
               ../src \
               ../hooks \
               ../grab/include \
               ../math/include \
               ..


HEADERS += \
    ../common/defs.h \
    ../src/enums.hpp \
    ../src/ApiServerSetColorTask.hpp \
    ../src/ApiServer.hpp \
    ../src/debug.h \
    ../src/Settings.hpp \
    ../src/Plugin.hpp \
    ../src/LightpackPluginInterface.hpp \
    ../src/LightpackCommandLineParser.hpp \
    ../src/AbstractLedDevice.hpp \
    ../src/AbstractLedDeviceUdp.hpp \
    ../src/LedDeviceDdp.hpp \
    ../src/HostColorSmoothing.hpp \
    ../src/ColorPipeline.hpp \
    ../src/SmoothingDriver.hpp \
    ../src/wizard/AreaDistributor.hpp \
    ../src/wizard/CustomDistributor.hpp \
    ../src/wizard/ContentAspectPreset.hpp \
    ../src/wizard/LayoutRecipeGenerator.hpp \
    ../src/wizard/DeviceDiscoveryDefault.hpp \
    ../src/wizard/BulkResize.hpp \
    ../src/LedGroupRuntime.hpp \
    ../src/ScreenTopology.hpp \
    ../src/ZoneLayoutRuntime.hpp \
    ../src/MoodLamp.hpp \
    ../src/LiquidColorGenerator.hpp \
    ../src/MoodLampManager.hpp \
    ../grab/include/calculations.hpp \
    ../math/include/PrismatikMath.hpp \
    ../math/include/ColorF.h \
    ../math/include/ColorOps.hpp \
    ../math/include/CalibrationPatterns.hpp \
    ../math/include/CalibrationSolver.hpp \
    SettingsWindowMockup.hpp \
    GrabCalculationTest.hpp \
    LightpackApiTest.hpp \
    lightpackmathtest.hpp \
    AppVersionTest.hpp \
    ../src/UpdatesProcessor.hpp \
    LightpackCommandLineParserTest.hpp \
    LedDeviceDdpTest.hpp \
    HostColorSmoothingTest.hpp \
    ContentAspectPresetTest.hpp \
    LayoutRecipeGeneratorTest.hpp \
    DeviceDiscoveryDefaultTest.hpp \
    MoodLampManagerTest.hpp \
    BulkResizeTest.hpp \
    LedGroupRuntimeTest.hpp \
    ScreenTopologyTest.hpp \
    ColorOpsTest.hpp \
    ColorPipelineGoldenTest.hpp \
    ColorPipelineHysteresisTest.hpp \
    SettingsMigrationTest.hpp \
    CalibrationTest.hpp

SOURCES += \
    ../src/ApiServerSetColorTask.cpp \
    ../src/ApiServer.cpp \
    ../src/Settings.cpp \
    ../src/Plugin.cpp \
    ../src/LightpackPluginInterface.cpp \
    ../src/LightpackCommandLineParser.cpp \
    ../src/AbstractLedDevice.cpp \
    ../src/AbstractLedDeviceUdp.cpp \
    ../src/LedDeviceDdp.cpp \
    ../src/HostColorSmoothing.cpp \
    ../src/ColorPipeline.cpp \
    ../src/SmoothingDriver.cpp \
    ../src/wizard/CustomDistributor.cpp \
    ../src/wizard/ContentAspectPreset.cpp \
    ../src/wizard/LayoutRecipeGenerator.cpp \
    ../src/wizard/DeviceDiscoveryDefault.cpp \
    ../src/wizard/BulkResize.cpp \
    ../src/ZoneLayoutRuntime.cpp \
    ../src/LedGroupRuntime.cpp \
    ../src/ScreenTopology.cpp \
    ../src/MoodLamp.cpp \
    ../src/LiquidColorGenerator.cpp \
    ../src/MoodLampManager.cpp \
    LightpackApiTest.cpp \
    SettingsWindowMockup.cpp \
    GrabCalculationTest.cpp \
    lightpackmathtest.cpp \
    TestsMain.cpp \
    AppVersionTest.cpp \
    ../src/UpdatesProcessor.cpp \
    LightpackCommandLineParserTest.cpp \
    LedDeviceDdpTest.cpp \
    HostColorSmoothingTest.cpp \
    ContentAspectPresetTest.cpp \
    LayoutRecipeGeneratorTest.cpp \
    DeviceDiscoveryDefaultTest.cpp \
    MoodLampManagerTest.cpp \
    BulkResizeTest.cpp \
    LedGroupRuntimeTest.cpp \
    ScreenTopologyTest.cpp \
    ColorOpsTest.cpp \
    ColorPipelineGoldenTest.cpp \
    ColorPipelineHysteresisTest.cpp \
    SettingsMigrationTest.cpp \
    CalibrationTest.cpp

win32{
    HEADERS += \
        HooksTest.h \
        ../hooks/ProxyFuncJmp.hpp \
        ../hooks/ProxyFunc.hpp \
        ../hooks/hooksutils.h \
        ../hooks/ProxyFuncVFTable.hpp \
        ../hooks/Logger.hpp

    SOURCES += \
        HooksTest.cpp \
        ../hooks/ProxyFuncJmp.cpp \
        ../hooks/hooksutils.cpp \
        ../hooks/ProxyFuncVFTable.cpp \
        ../hooks/Logger.cpp
}
