#ifndef SETTINGSMIGRATIONTEST_HPP
#define SETTINGSMIGRATIONTEST_HPP

#include <QObject>

class SettingsMigrationTest : public QObject
{
	Q_OBJECT
public:
	explicit SettingsMigrationTest(QObject *parent = nullptr);

private slots:
	void initTestCase();
	void testMigrateTempOnYields132();
	void testMigrateTempOffYields110();
	void testMigrateIdempotent();
	void testAlreadyV2Untouched();
	void testMultiProfileMigration();
};

#endif
