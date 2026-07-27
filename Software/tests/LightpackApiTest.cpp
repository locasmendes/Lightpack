/*
 * LightpackApiTest.cpp
 *
 *	Created on: 07.09.2011
 *		Project: Lightpack
 *
 *	Copyright (c) 2011 Mike Shatohin, mikeshatohin [at] gmail.com
 *
 *	Lightpack a USB content-driving ambient lighting system
 *
 *	Lightpack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Lightpack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <QString>
#include <QApplication>
#include <QTest>
#include <QJsonArray>
#include <QJsonObject>

#include "debug.h"
#include "ApiServer.hpp"
#include "LightpackPluginInterface.hpp"
#include "Settings.hpp"
#include "enums.hpp"
#include "SettingsWindowMockup.hpp"

#include <stdlib.h>
#include <iostream>
#include "LightpackApiTest.hpp"

using namespace std;
using namespace SettingsScope;

LightpackApiTest::LightpackApiTest()
{
	// Register QMetaType for Qt::QueuedConnection
	qRegisterMetaType< QList<QRgb> >("QList<QRgb>");
	qRegisterMetaType<Backlight::Status>("Backlight::Status");
}

void LightpackApiTest::initTestCase()
{
	Settings::Initialize(QDir::currentPath(), true);

	m_socket = NULL;

	// Start Api Server in separate thread for access by QTcpSocket-s
	m_apiServer = new ApiServer(3636);
	m_interfaceApi = new LightpackPluginInterface();
	m_apiServer->setInterface(m_interfaceApi);
	m_apiServerThread = new QThread();

	m_apiServer->moveToThread(m_apiServerThread);
	m_apiServerThread->start();

	m_little = new SettingsWindowMockup(this);

	connect(m_little, SIGNAL(enableApiServer(bool)), m_apiServer, SLOT(enableApiServer(bool)), Qt::DirectConnection);
	connect(m_little, SIGNAL(updateApiKey(QString)), m_apiServer, SLOT(updateApiKey(QString)), Qt::DirectConnection);
	connect(m_little, SIGNAL(updateApiPort(int)), m_apiServer, SLOT(updateApiPort(int)), Qt::DirectConnection);

	connect(m_interfaceApi, SIGNAL(requestBacklightStatus()), m_little, SLOT(requestBacklightStatus()), Qt::QueuedConnection);
	connect(m_little, SIGNAL(resultBacklightStatus(Backlight::Status)), m_interfaceApi, SLOT(resultBacklightStatus(Backlight::Status)));

	connect(m_interfaceApi, SIGNAL(updateLedsColors(QList<QRgb>)), m_little, SLOT(setLedColors(QList<QRgb>)), Qt::QueuedConnection);
	connect(m_interfaceApi, SIGNAL(updateGamma(double)), m_little, SLOT(setGamma(double)), Qt::QueuedConnection);
	connect(m_interfaceApi, SIGNAL(updateBrightness(int)), m_little, SLOT(setBrightness(int)), Qt::QueuedConnection);
	connect(m_interfaceApi, SIGNAL(updateSmooth(int)), m_little, SLOT(setSmooth(int)), Qt::QueuedConnection);
	connect(m_interfaceApi, SIGNAL(updateProfile(QString)), m_little, SLOT(setProfile(QString)), Qt::QueuedConnection);
	connect(m_interfaceApi, SIGNAL(updateStatus(Backlight::Status)), m_little, SLOT(setStatus(Backlight::Status)), Qt::QueuedConnection);

	m_little->setApiKey("");
}

void LightpackApiTest::init()
{
	if (m_socket == NULL)
		m_socket = new QTcpSocket();

	// Reconnect to host before each test case
	m_socket->connectToHost("127.0.0.1", 3636);

	// Wait 5 second for connected
	QVERIFY(m_socket->waitForConnected(5000));

	// Check version of API and version of API Tests on match
	QVERIFY(checkVersion(m_socket));

	m_sockReadLineOk = false;
}

void LightpackApiTest::cleanup()
{
	QVERIFY(m_socket != NULL);

	m_socket->abort();
}

//
// Test cases
//
void LightpackApiTest::testCase_ApiVersion()
{
	// see init() for version check
}

void LightpackApiTest::testCase_GetStatus()
{		
	// Test Backlight Off state:
	m_little->setStatus(Backlight::StatusOff);
	writeCommand(m_socket, ApiServer::CmdGetStatus);

	processEventsFromLittle();

	QByteArray result = readResult(m_socket);
	QVERIFY(result == ApiServer::CmdResultStatus_Off);

	// Test Backlight On state:
	m_little->setStatus(Backlight::StatusOn);
	writeCommand(m_socket, ApiServer::CmdGetStatus);

	processEventsFromLittle();
	processEventsFromLittle();

	result = readResult(m_socket);
	QVERIFY(result == ApiServer::CmdResultStatus_On);

	// Test Backlight DeviceError state:
	m_little->setStatus(Backlight::StatusDeviceError);
	writeCommand(m_socket, ApiServer::CmdGetStatus);

	processEventsFromLittle();
	processEventsFromLittle();

	result = readResult(m_socket);
	QVERIFY(result == ApiServer::CmdResultStatus_DeviceError);
}

void LightpackApiTest::testCase_GetStatusAPI()
{
	// Test idle state:
	writeCommand(m_socket, ApiServer::CmdGetStatusAPI);

	QByteArray result = readResult(m_socket);
	QVERIFY(result == ApiServer::CmdResultStatusAPI_Idle);

	// Test lock and busy state:
	QTcpSocket sockLock;
	sockLock.connectToHost("127.0.0.1", 3636);
	QVERIFY(checkVersion(&sockLock));

	writeCommand(&sockLock, ApiServer::CmdLock);
	QVERIFY(readResult(&sockLock) == ApiServer::CmdResultLock_Success);
	QVERIFY(m_sockReadLineOk);

	writeCommand(m_socket, ApiServer::CmdGetStatusAPI);
	QVERIFY(readResult(m_socket) == ApiServer::CmdResultStatusAPI_Busy);
	QVERIFY(m_sockReadLineOk);

	// Test unlock and return to idle state
	writeCommand(&sockLock, ApiServer::CmdUnlock);
	QVERIFY(readResult(&sockLock) == ApiServer::CmdResultUnlock_Success);
	QVERIFY(m_sockReadLineOk);

	writeCommand(m_socket, ApiServer::CmdGetStatusAPI);
	QVERIFY(readResult(m_socket) == ApiServer::CmdResultStatusAPI_Idle);
	QVERIFY(m_sockReadLineOk);
}

void LightpackApiTest::testCase_GetProfiles()
{
	QString cmdProfilesCheckResult = getProfilesResultString();

	// Test GetProfiles command:

	writeCommand(m_socket, ApiServer::CmdGetProfiles);

	QByteArray result = readResult(m_socket).trimmed();
	QVERIFY(m_sockReadLineOk);

	qDebug() << "res=" << result;
	qDebug() << "check=" << cmdProfilesCheckResult;

	QVERIFY(result == cmdProfilesCheckResult);

	// Test UTF-8 in profile name

	QString utf8Check = tr("\u041F\u0440\u043E\u0432\u0435\u0440\u043A\u0430"); // Russian word "Proverka"

	Settings::loadOrCreateProfile("ApiTestProfile");
	Settings::loadOrCreateProfile("ApiTestProfile-UTF-8-" + utf8Check);

	QString cmdProfilesCheckResultWithUtf8 = getProfilesResultString();

	writeCommand(m_socket, ApiServer::CmdGetProfiles);

	result = readResult(m_socket).trimmed();
	QVERIFY(m_sockReadLineOk);

	QVERIFY(result == cmdProfilesCheckResultWithUtf8);
}

void LightpackApiTest::testCase_GetProfile()
{
	QString cmdProfileCheckResult = ApiServer::CmdResultProfile +
			Settings::getCurrentProfileName().toUtf8();

	// Test GetProfile command:

	writeCommand(m_socket, ApiServer::CmdGetProfile);

	QByteArray result = readResult(m_socket).trimmed();
	QVERIFY(m_sockReadLineOk);

	if (result != cmdProfileCheckResult)
	{
		qDebug() << "result =" << result;
		qDebug() << "cmdProfileCheckResult =" << cmdProfileCheckResult;
	}

	QVERIFY(result == cmdProfileCheckResult);
}

void LightpackApiTest::testCase_Lock()
{
	QTcpSocket sockTryLock;
	sockTryLock.connectToHost("127.0.0.1", 3636);
	QVERIFY(checkVersion(&sockTryLock));

	// Test lock success
	QVERIFY(lock(m_socket));

	// Test lock busy
	writeCommand(&sockTryLock, ApiServer::CmdLock);
	QByteArray result = readResult(&sockTryLock);
	QVERIFY(m_sockReadLineOk);
	QVERIFY(result == ApiServer::CmdResultLock_Busy);

	// Test lock success after unlock
	QVERIFY(unlock(m_socket));

	writeCommand(&sockTryLock, ApiServer::CmdLock);
	result = readResult(&sockTryLock);
	QVERIFY(m_sockReadLineOk);
	QVERIFY(result == ApiServer::CmdResultLock_Success);
}

void LightpackApiTest::testCase_Unlock()
{
	// Test Unlock command:
	writeCommand(m_socket, ApiServer::CmdUnlock);
	QByteArray result = readResult(m_socket);
	QVERIFY(m_sockReadLineOk);
	QVERIFY(result == ApiServer::CmdResultUnlock_NotLocked);
}

void LightpackApiTest::testCase_SetColor()
{	
	QTcpSocket sockLock;
	sockLock.connectToHost("127.0.0.1", 3636);
	QVERIFY(checkVersion(&sockLock));

	// Test lock state in SetColor command:
	QByteArray setColorCmd = ApiServer::CmdSetColor;

	setColorCmd += "1-23,2,65;";
	int led = 1;
	QRgb rgb = qRgba(23, 2, 65, 0xff);

	QVERIFY(writeCommandWithCheck(m_socket, setColorCmd, ApiServer::CmdSetResult_NotLocked));

	// Test busy state in SetColor command:
	QVERIFY(lock(&sockLock));

	QVERIFY(writeCommandWithCheck(m_socket, setColorCmd, ApiServer::CmdSetResult_Busy));

	// Test in SetColor change to unlock state:
	QVERIFY(unlock(&sockLock));

	QVERIFY(lock(m_socket));

	// Test SetColor on valid command
	QVERIFY(writeCommandWithCheck(m_socket, setColorCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	if (m_little->m_colors[led-1] != rgb)
	{
		DEBUG_OUT_RGB(rgb);
		DEBUG_OUT_RGB(m_little->m_colors[led-1]);
	}

	QVERIFY(m_little->m_colors[led-1] == rgb);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetColorValid()
{
	QVERIFY(lock(m_socket));

	QByteArray setColorCmd = ApiServer::CmdSetColor;

	QFETCH(QString, cmd);
	QFETCH(int, led);
	QFETCH(int, r);
	QFETCH(int, g);
	QFETCH(int, b);

	QRgb rgb = qRgb(r, g, b);

	setColorCmd += cmd.toUtf8();

	// Test SetColor different valid strings
	QVERIFY(writeCommandWithCheck(m_socket, setColorCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	if (m_little->m_colors[led-1] != rgb)
	{
		DEBUG_OUT_RGB(rgb);
		DEBUG_OUT_RGB(m_little->m_colors[led-1]);
	}

	QVERIFY(m_little->m_colors[led-1] == rgb);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetColorValid_data()
{
	QTest::addColumn<QString>("cmd");
	QTest::addColumn<int>("led");
	QTest::addColumn<int>("r");
	QTest::addColumn<int>("g");
	QTest::addColumn<int>("b");

	QTest::newRow("0") << "1-1,1,1" << 1 << 1 << 1 << 1; // setcolor without semicolon is valid!
	QTest::newRow("1") << "1-1,1,1;" << 1 << 1 << 1 << 1;
	QTest::newRow("2") << "2-1,1,1;" << 2 << 1 << 1 << 1;
	QTest::newRow("3") << "10-1,1,1;" << 10 << 1 << 1 << 1;
	QTest::newRow("4") << "1-192,1,1;" << 1 << 192 << 1 << 1;
	QTest::newRow("5") << "1-1,185,1;" << 1 << 1 << 185 << 1;
	QTest::newRow("6") << "1-14,15,19;" << 1 << 14 << 15 << 19;
	QTest::newRow("7") << "1-255,255,255;" << 1 << 255 << 255 << 255;
	QTest::newRow("8") << "10-255,255,255;" << 10 << 255 << 255 << 255;
	QTest::newRow("9") << "10-0,0,1;" << 10 << 0 << 0 << 1;
	QTest::newRow("10") << "10-1,0,0;" << 10 << 1 << 0 << 0;
}

void LightpackApiTest::testCase_SetColorValid2()
{	
	QVERIFY(lock(m_socket));

	QByteArray setColorCmd = ApiServer::CmdSetColor;

	QFETCH(QString, colorsStr);

	setColorCmd += colorsStr.toUtf8();

	// Test SetColor different valid strings

	QVERIFY(writeCommandWithCheck(m_socket, setColorCmd, ApiServer::CmdSetResult_Ok));

	// Just process all pending events from m_apiServer
	processEventsFromLittle();

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetColorValid2_data()
{
	QTest::addColumn<QString>("colorsStr");

	QTest::newRow("1") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;6-6,6,6;7-7,7,7;8-8,8,8;9-9,9,9;10-10,10,10;";
	QTest::newRow("2") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;6-6,6,6;7-7,7,7;8-8,8,8;9-9,9,9;";
	QTest::newRow("3") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;6-6,6,6;7-7,7,7;8-8,8,8;";
	QTest::newRow("4") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;6-6,6,6;7-7,7,7;";
	QTest::newRow("5") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;6-6,6,6;";
	QTest::newRow("6") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;";
	QTest::newRow("7") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;";
	QTest::newRow("8") << "1-1,1,1;2-2,2,2;3-3,3,3;";
	QTest::newRow("9") << "1-1,1,1;2-2,2,2;";
	QTest::newRow("10") << "1-1,1,1;";

	// Set color to one led several times in one cmd is VALID!
	QTest::newRow("11") << "1-1,1,1;1-1,1,1;1-2,2,2;1-3,3,3;";
}


void LightpackApiTest::testCase_SetColorInvalid()
{
	QVERIFY(lock(m_socket));

	QByteArray setColorCmd = ApiServer::CmdSetColor;

	QFETCH(QString, colorsStr);

	setColorCmd += colorsStr.toUtf8();

	// Test SetColor different valid strings
	QVERIFY(writeCommandWithCheck(m_socket, setColorCmd, ApiServer::CmdSetResult_Error));

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetColorInvalid_data()
{
	QTest::addColumn<QString>("colorsStr");

	QTest::newRow("1") << "1--1,1,1;";
	QTest::newRow("2") << "1-1,,1,1;";
	QTest::newRow("3") << "1-1,1,,1;";
	QTest::newRow("4") << "1-1.1.1";
	QTest::newRow("5") << "1-1,1,1;2-";
	QTest::newRow("6") << "11-1,1,1;";
	QTest::newRow("7") << "0-1,1,1;";
	QTest::newRow("8") << "1-1,-1,1;";
	QTest::newRow("9") << "1-1,1111,1;";
	QTest::newRow("10") << "1-1,1,256;";
	QTest::newRow("11") << "!-1,1,1;";
	QTest::newRow("12") << "-1,1,1;";
	QTest::newRow("13") << "1-1,1;";
	QTest::newRow("14") << "1-1,100000000000000000000000;";
	QTest::newRow("15") << "1-1,1,1;2-4,5,,;";
	QTest::newRow("16") << "1-1,1,1;2-2,2,2;3-3,3,3;4-4,4,4;5-5,5,5;;6-6,6,6;7-7,7,7;8-8,8,8;9-9,9,9;";
	QTest::newRow("17") << "1-1,1,1;;";
}

void LightpackApiTest::testCase_SetGammaValid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, gammaStr);
	QFETCH(double, gammaValue);

	QByteArray setGammaCmd = ApiServer::CmdSetGamma;
	setGammaCmd += gammaStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setGammaCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	if (m_little->m_gamma != gammaValue)
	{
		qDebug() << "m_little->m_gamma =" << m_little->m_gamma << "gammaValue =" << gammaValue;
	}

	QVERIFY(m_little->m_gamma == gammaValue);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetGammaValid_data()
{
	QTest::addColumn<QString>("gammaStr");
	QTest::addColumn<double>("gammaValue");

	// Valid
	QTest::newRow("0") << ".01"		<< 0.01;
	QTest::newRow("1") << "1.0"		<< 1.0;
	QTest::newRow("2") << "2.0"		<< 2.0;
	QTest::newRow("3") << "3"		<< 3.0;
	QTest::newRow("4") << "10.00"	<< 10.0;
}

void LightpackApiTest::testCase_SetGammaInvalid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, gammaStr);

	QByteArray setGammaCmd = ApiServer::CmdSetGamma;
	setGammaCmd += gammaStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setGammaCmd, ApiServer::CmdSetResult_Error));

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetGammaInvalid_data()
{
	QTest::addColumn<QString>("gammaStr");

	QTest::newRow("0") << "0.00";
	QTest::newRow("1") << "0.0001";
	QTest::newRow("2") << "12.0";
	QTest::newRow("3") << ":12.0";
	QTest::newRow("4") << "2.0;";
	QTest::newRow("5") << "1.2.0";
	QTest::newRow("6") << "-1.2";
	QTest::newRow("7") << "10.01";
	QTest::newRow("8") << "4.56857";
	QTest::newRow("9") << "4.5685787384739473827432423489237985739487593745987349857938475";
	QTest::newRow("10") << "+100500";
	QTest::newRow("11") << "Galaxy in Danger!";
	QTest::newRow("12") << "reinterpret_cast<CodeMonkey*>(m_pSelf);";
	QTest::newRow("13") << "BSOD are coming soon!";
}

void LightpackApiTest::testCase_SetBrightnessValid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, brightnessStr);
	QFETCH(int, brightnessValue);

	QByteArray setBrightnessCmd = ApiServer::CmdSetBrightness;
	setBrightnessCmd += brightnessStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setBrightnessCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	if (m_little->m_brightness != brightnessValue)
	{
		qDebug() << "m_little->m_brightness =" << m_little->m_brightness << "brightnessValue =" << brightnessValue;
	}

	QVERIFY(m_little->m_brightness == brightnessValue);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetBrightnessValid_data()
{
	QTest::addColumn<QString>("brightnessStr");
	QTest::addColumn<int>("brightnessValue");

	// Valid
	QTest::newRow("0") << "0"	<< 0;
	QTest::newRow("1") << "1"	<< 1;
	QTest::newRow("2") << "55"	<< 55;
	QTest::newRow("3") << "100" << 100;
}

void LightpackApiTest::testCase_SetBrightnessInvalid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, brightnessStr);

	QByteArray setBrightnessCmd = ApiServer::CmdSetBrightness;
	setBrightnessCmd += brightnessStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setBrightnessCmd, ApiServer::CmdSetResult_Error));

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetBrightnessInvalid_data()
{
	QTest::addColumn<QString>("brightnessStr");

	QTest::newRow("0") << "0.0";
	QTest::newRow("1") << "0.0001";
	QTest::newRow("2") << "-12";
	QTest::newRow("3") << ":12.0";
	QTest::newRow("4") << "2.;";
	QTest::newRow("5") << ".2.";
	QTest::newRow("5") << "120";
	QTest::newRow("5") << "100500";
}

void LightpackApiTest::testCase_SetSmoothValid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, smoothStr);
	QFETCH(int, smoothValue);

	QByteArray setSmoothCmd = ApiServer::CmdSetSmooth;
	setSmoothCmd += smoothStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setSmoothCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	QVERIFY(m_little->m_smooth == smoothValue);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetSmoothValid_data()
{
	QTest::addColumn<QString>("smoothStr");
	QTest::addColumn<int>("smoothValue");

	QTest::newRow("1") << "0" << 0;
	QTest::newRow("2") << "255" << 255;
	QTest::newRow("3") << "43" << 43;
}

void LightpackApiTest::testCase_SetSmoothInvalid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, smoothStr);

	QByteArray setSmoothCmd = ApiServer::CmdSetSmooth;
	setSmoothCmd += smoothStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setSmoothCmd, ApiServer::CmdSetResult_Error));

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetSmoothInvalid_data()
{
	QTest::addColumn<QString>("smoothStr");

	QTest::newRow("1") << "-1";
	QTest::newRow("2") << "256";
	QTest::newRow("3") << "10.0";
	QTest::newRow("4") << "1.0";
	QTest::newRow("4") << ".0";
	QTest::newRow("4") << "..0";
	QTest::newRow("4") << "1.";
}

void LightpackApiTest::testCase_GetHostSmooth()
{
	QByteArray cmdResultCheck = ApiServer::CmdResultHostSmooth
			+ QString::number(Settings::getGrabHostSmoothingDuration()).toUtf8();

	writeCommand(m_socket, ApiServer::CmdGetHostSmooth);

	QByteArray result = readResult(m_socket).trimmed();
	QVERIFY(m_sockReadLineOk);

	QVERIFY(result == cmdResultCheck);
}

void LightpackApiTest::testCase_SetHostSmoothValid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, hostSmoothStr);
	QFETCH(int, hostSmoothValue);

	QByteArray setHostSmoothCmd = ApiServer::CmdSetHostSmooth;
	setHostSmoothCmd += hostSmoothStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setHostSmoothCmd, ApiServer::CmdSetResult_Ok));

	QVERIFY(Settings::getGrabHostSmoothingDuration() == hostSmoothValue);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetHostSmoothValid_data()
{
	QTest::addColumn<QString>("hostSmoothStr");
	QTest::addColumn<int>("hostSmoothValue");

	QTest::newRow("off") << "0" << 0;
	QTest::newRow("max") << "400" << 400;
	QTest::newRow("mid") << "150" << 150;
}

void LightpackApiTest::testCase_SetHostSmoothInvalid()
{
	QVERIFY(lock(m_socket));

	QFETCH(QString, hostSmoothStr);

	QByteArray setHostSmoothCmd = ApiServer::CmdSetHostSmooth;
	setHostSmoothCmd += hostSmoothStr.toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setHostSmoothCmd, ApiServer::CmdSetResult_Error));

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetHostSmoothInvalid_data()
{
	QTest::addColumn<QString>("hostSmoothStr");

	QTest::newRow("negative") << "-1";
	QTest::newRow("one over max") << "401";
	QTest::newRow("decimal") << "10.0";
	QTest::newRow("not a number") << "abc";
}

void LightpackApiTest::testCase_SetHostSmoothWithoutLock()
{
	QByteArray setHostSmoothCmd = ApiServer::CmdSetHostSmooth;
	setHostSmoothCmd += "150";

	QVERIFY(writeCommandWithCheck(m_socket, setHostSmoothCmd, ApiServer::CmdSetResult_NotLocked));
}

namespace
{
	QJsonArray sampleLayoutRecipe()
	{
		QJsonObject rect;
		rect["x"] = 0;
		rect["y"] = 0;
		rect["width"] = 1920;
		rect["height"] = 1080;

		QJsonObject monitor;
		monitor["startingLed"] = 0;
		monitor["topLeds"] = 4;
		monitor["sideLeds"] = 2;
		monitor["bottomLeds"] = 4;
		monitor["thicknessPercent"] = 15;
		monitor["standWidthPercent"] = 0;
		monitor["skipCorners"] = false;
		monitor["invertOrder"] = false;
		monitor["numberingOffset"] = 0;
		monitor["baseRect"] = rect;

		QJsonArray recipe;
		recipe.append(monitor);
		return recipe;
	}
}

void LightpackApiTest::testCase_ContentAspectSettingsDefaultAndRecipeRoundTrip()
{
	// A profile with no recipe defaults to "fill" and reports no recipe.
	Settings::setLayoutRecipe(QJsonArray());
	QVERIFY(!Settings::hasLayoutRecipe());
	QCOMPARE(Settings::getContentAspectPreset(), QStringLiteral("fill"));

	const QJsonArray recipe = sampleLayoutRecipe();
	Settings::setLayoutRecipe(recipe);
	QVERIFY(Settings::hasLayoutRecipe());
	QCOMPARE(Settings::getLayoutRecipe(), recipe);

	Settings::setContentAspectPreset(QStringLiteral("16:9"));
	QCOMPARE(Settings::getContentAspectPreset(), QStringLiteral("16:9"));

	// Invalid/unknown values are normalized to "fill" rather than stored as-is.
	Settings::setContentAspectPreset(QStringLiteral("bogus"));
	QCOMPARE(Settings::getContentAspectPreset(), QStringLiteral("fill"));

	// Cleanup so later test cases don't inherit this recipe.
	Settings::setLayoutRecipe(QJsonArray());
}

void LightpackApiTest::testCase_SetContentAspectRequiresRecipeAndLock()
{
	Settings::setLayoutRecipe(QJsonArray());

	QVERIFY(lock(m_socket));

	// No recipe yet: even a valid token and a valid lock must fail.
	QByteArray setContentAspectCmd = ApiServer::CmdSetContentAspect;
	setContentAspectCmd += "fill";
	QVERIFY(writeCommandWithCheck(m_socket, setContentAspectCmd, ApiServer::CmdSetResult_Error));

	// Invalid token, still no recipe.
	QByteArray badTokenCmd = ApiServer::CmdSetContentAspect;
	badTokenCmd += "bogus";
	QVERIFY(writeCommandWithCheck(m_socket, badTokenCmd, ApiServer::CmdSetResult_Error));

	QVERIFY(unlock(m_socket));

	// Now with a recipe present, valid tokens succeed under lock.
	Settings::setLayoutRecipe(sampleLayoutRecipe());
	QVERIFY(lock(m_socket));

	QByteArray setFillCmd = ApiServer::CmdSetContentAspect;
	setFillCmd += "fill";
	QVERIFY(writeCommandWithCheck(m_socket, setFillCmd, ApiServer::CmdSetResult_Ok));
	QCOMPARE(Settings::getContentAspectPreset(), QStringLiteral("fill"));

	QByteArray set16x9Cmd = ApiServer::CmdSetContentAspect;
	set16x9Cmd += "16:9";
	QVERIFY(writeCommandWithCheck(m_socket, set16x9Cmd, ApiServer::CmdSetResult_Ok));
	QCOMPARE(Settings::getContentAspectPreset(), QStringLiteral("16:9"));

	QVERIFY(unlock(m_socket));

	// Without lock, even a valid token/recipe is rejected.
	QByteArray set43Cmd = ApiServer::CmdSetContentAspect;
	set43Cmd += "4:3";
	QVERIFY(writeCommandWithCheck(m_socket, set43Cmd, ApiServer::CmdSetResult_NotLocked));

	Settings::setLayoutRecipe(QJsonArray());
}

void LightpackApiTest::testCase_SetProfile()
{
	QVERIFY(lock(m_socket));

	QStringList profiles = Settings::findAllProfiles();
	QVERIFY(profiles.count() > 0);

	QByteArray setProfileCmd = ApiServer::CmdSetProfile;
	setProfileCmd += profiles.at(0).toUtf8();

	QVERIFY(writeCommandWithCheck(m_socket, setProfileCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	QVERIFY(profiles.at(0) == m_little->m_profile);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_SetStatus()
{
	QVERIFY(lock(m_socket));

	QByteArray setStatusCmd = ApiServer::CmdSetStatus;
	setStatusCmd += ApiServer::CmdSetStatus_On;

	QVERIFY(writeCommandWithCheck(m_socket, setStatusCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	QVERIFY(Backlight::StatusOn == m_little->m_status);

	setStatusCmd = ApiServer::CmdSetStatus;
	setStatusCmd += ApiServer::CmdSetStatus_Off;

	QVERIFY(writeCommandWithCheck(m_socket, setStatusCmd, ApiServer::CmdSetResult_Ok));

	processEventsFromLittle();

	QVERIFY(Backlight::StatusOff == m_little->m_status);

	QVERIFY(unlock(m_socket));
}

void LightpackApiTest::testCase_ApiAuthorization()
{
	QString testKey = "test-key";

	m_little->setApiKey(testKey);

	QByteArray cmdApiKey = ApiServer::CmdApiKey;
	cmdApiKey += testKey.toUtf8();

	// Authorization
	QVERIFY(writeCommandWithCheck(m_socket, cmdApiKey, ApiServer::CmdApiKeyResult_Ok));

	// Try lock device after SUCCESS authorization
	QVERIFY(lock(m_socket));

	// Authorization FAIL check
	QVERIFY(writeCommandWithCheck(m_socket, cmdApiKey + "invalid", ApiServer::CmdApiKeyResult_Fail));

	// Try lock device after FAIL authorization
	QVERIFY(writeCommandWithCheck(m_socket, ApiServer::CmdLock, ApiServer::CmdApiCheck_AuthRequired));
}

// Private help functions

QByteArray LightpackApiTest::readResult(QTcpSocket * socket)
{
	m_sockReadLineOk = socket->waitForReadyRead(1000) && socket->canReadLine();
	return socket->readLine();
}

void LightpackApiTest::writeCommand(QTcpSocket * socket, const char * cmd)
{
	socket->write(cmd);
	socket->write("\n");
}

bool LightpackApiTest::writeCommandWithCheck(QTcpSocket * socket, const QByteArray & command, const QByteArray & result)
{
	writeCommand(socket, command);
	QByteArray read = readResult(socket);

	return (m_sockReadLineOk && read == result);
}

QString LightpackApiTest::getProfilesResultString()
{
	QStringList profiles = Settings::findAllProfiles();

	QString cmdProfilesCheckResult = ApiServer::CmdResultProfiles;
	for (int i = 0; i < profiles.count(); i++)
		cmdProfilesCheckResult += profiles[i].toUtf8() + ";";

	return cmdProfilesCheckResult;
}

void LightpackApiTest::processEventsFromLittle()
{
	QElapsedTimer timer;
	timer.start();
	m_little->m_isDone = false;

	while (m_little->m_isDone == false && timer.elapsed() < ApiServer::SignalWaitTimeoutMs)
	{
		QApplication::processEvents(QEventLoop::WaitForMoreEvents, ApiServer::SignalWaitTimeoutMs);
	}
}

bool LightpackApiTest::checkVersion(QTcpSocket * socket)
{
	// Check the version banner sent by ApiServer on connect matches what it
	// actually sends today, instead of a copy that can drift out of sync
	// (see ApiServer::ApiVersion, ApiServer.cpp).

	QString result = readResult(socket);

	QString versionTests = QString(ApiServer::ApiVersion).trimmed();

	return (m_sockReadLineOk && result.trimmed() == versionTests);
}

bool LightpackApiTest::lock(QTcpSocket * socket)
{
	return writeCommandWithCheck(socket, ApiServer::CmdLock, ApiServer::CmdResultLock_Success);
}

bool LightpackApiTest::unlock(QTcpSocket * socket)
{
	// Must be locked before unlock, else unlock() return false,
	// because result will be ApiServer::CmdResultUnlock_NotLocked
	return writeCommandWithCheck(socket, ApiServer::CmdUnlock, ApiServer::CmdResultUnlock_Success);
}
