/*
 * SettingsMigrationTest.cpp — General/ProfileVersion + Device/OutputGamma (§2.10 / §2.11).
 */

#include "SettingsMigrationTest.hpp"
#include "Settings.hpp"
#include "SettingsDefaults.hpp"
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QDir>
#include <cmath>

using namespace SettingsScope;

namespace {

QTemporaryDir *g_tempDir = nullptr;

void writeProfileIni(const QString &name, double grabGamma, double deviceGamma, bool tempOn,
	const QString &profileVersion = QString(), double outputGamma = -1.0)
{
	const QString path = Settings::getProfilesPath() + name + QStringLiteral(".ini");
	QSettings ini(path, QSettings::IniFormat);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	ini.setIniCodec("UTF-8");
#endif
	ini.setValue(QStringLiteral("Grab/Gamma"), grabGamma);
	ini.setValue(QStringLiteral("Device/Gamma"), deviceGamma);
	ini.setValue(QStringLiteral("Grab/IsApplyColorTemperatureEnabled"), tempOn);
	if (!profileVersion.isEmpty())
		ini.setValue(QStringLiteral("General/ProfileVersion"), profileVersion);
	if (outputGamma >= 0.0)
		ini.setValue(QStringLiteral("Device/OutputGamma"), outputGamma);
	ini.sync();
}

bool nearlyEqual(double a, double b, double eps = 1e-9)
{
	return std::fabs(a - b) <= eps;
}

} // namespace

SettingsMigrationTest::SettingsMigrationTest(QObject *parent)
	: QObject(parent)
{
}

void SettingsMigrationTest::initTestCase()
{
	g_tempDir = new QTemporaryDir;
	QVERIFY(g_tempDir->isValid());
	const QString root = g_tempDir->path() + QStringLiteral("/");
	// Initialize returns whether a main config already existed (false on first create).
	Settings::Initialize(root, false);
	QVERIFY(Settings::isProfileLoaded() || !Settings::getProfilesPath().isEmpty());
	QVERIFY(QDir().mkpath(Settings::getProfilesPath()));
}

void SettingsMigrationTest::testMigrateTempOnYields132()
{
	writeProfileIni(QStringLiteral("MigTempOn"), 1.2, 2.0, true);
	Settings::loadOrCreateProfile(QStringLiteral("MigTempOn"));
	const double got = Settings::getDeviceOutputGamma();
	const QString ver = Settings::value(QStringLiteral("General/ProfileVersion")).toString();
	if (!nearlyEqual(got, 1.32))
		QFAIL(qPrintable(QStringLiteral("OutputGamma got %1 expected 1.32; ProfileVersion=%2; GrabGamma=%3 DeviceGamma=%4 temp=%5")
			.arg(got).arg(ver).arg(Settings::getGrabGamma()).arg(Settings::getDeviceGamma())
			.arg(Settings::isGrabApplyColorTemperatureEnabled())));
	QCOMPARE(ver, QStringLiteral("2"));
	QVERIFY(nearlyEqual(Settings::getGrabGamma(), 1.2));
	QVERIFY(nearlyEqual(Settings::getDeviceGamma(), 2.0));
}

void SettingsMigrationTest::testMigrateTempOffYields110()
{
	writeProfileIni(QStringLiteral("MigTempOff"), 1.2, 2.0, false);
	Settings::loadOrCreateProfile(QStringLiteral("MigTempOff"));
	QVERIFY2(nearlyEqual(Settings::getDeviceOutputGamma(), 1.10),
		qPrintable(QStringLiteral("got %1").arg(Settings::getDeviceOutputGamma())));
	QCOMPARE(Settings::value(QStringLiteral("General/ProfileVersion")).toString(),
		QStringLiteral("2"));
}

void SettingsMigrationTest::testMigrateIdempotent()
{
	writeProfileIni(QStringLiteral("MigIdem"), 1.2, 2.0, true);
	Settings::loadOrCreateProfile(QStringLiteral("MigIdem"));
	const double first = Settings::getDeviceOutputGamma();
	Settings::migrateCurrentProfile();
	QVERIFY(nearlyEqual(Settings::getDeviceOutputGamma(), first));
	QCOMPARE(Settings::value(QStringLiteral("General/ProfileVersion")).toString(),
		QStringLiteral("2"));
}

void SettingsMigrationTest::testAlreadyV2Untouched()
{
	writeProfileIni(QStringLiteral("MigV2"), 1.2, 2.0, true, QStringLiteral("2"), 1.55);
	Settings::loadOrCreateProfile(QStringLiteral("MigV2"));
	QVERIFY2(nearlyEqual(Settings::getDeviceOutputGamma(), 1.55),
		"v2 profile OutputGamma must not be overwritten");
}

void SettingsMigrationTest::testMultiProfileMigration()
{
	writeProfileIni(QStringLiteral("ProfileA"), 1.2, 2.0, true);
	writeProfileIni(QStringLiteral("ProfileB"), 1.2, 2.0, false);

	Settings::loadOrCreateProfile(QStringLiteral("ProfileA"));
	QVERIFY2(nearlyEqual(Settings::getDeviceOutputGamma(), 1.32), "A should migrate to 1.32");
	QCOMPARE(Settings::value(QStringLiteral("General/ProfileVersion")).toString(),
		QStringLiteral("2"));

	Settings::loadOrCreateProfile(QStringLiteral("ProfileB"));
	QVERIFY2(nearlyEqual(Settings::getDeviceOutputGamma(), 1.10), "B should migrate to 1.10");
	QCOMPARE(Settings::value(QStringLiteral("General/ProfileVersion")).toString(),
		QStringLiteral("2"));

	// Reloading A still has its migrated value.
	Settings::loadOrCreateProfile(QStringLiteral("ProfileA"));
	QVERIFY2(nearlyEqual(Settings::getDeviceOutputGamma(), 1.32), "A still 1.32 after B");
}
