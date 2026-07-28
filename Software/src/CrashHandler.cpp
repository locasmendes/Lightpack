/*
 * CrashHandler.cpp
 *
 *	Project: Lightpack
 */

#include "CrashHandler.hpp"

#include <QtCore>
#include <exception>

#ifdef Q_OS_WIN
#if !defined NOMINMAX
#define NOMINMAX
#endif
#if !defined WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#endif

// Must come after <dbghelp.h> - version.h #defines API_VERSION as a string literal, which
// collides with dbghelp.h's own API_VERSION struct type if included first.
#include "version.h"
#include "Settings.hpp"

namespace CrashHandler
{
namespace
{
	QString g_logsDir;
	std::function<void()> g_shutdownCallback;

	QString timestampForFilenames()
	{
		return QDateTime::currentDateTime().toString(QStringLiteral("yyyy_MM_dd_hh_mm_ss_zzz"));
	}

	// Best-effort: a failure in here must never prevent the rest of the crash handling
	// (log/minidump/message box) from completing.
	void runShutdownCallback()
	{
		if (!g_shutdownCallback)
			return;
		if (SettingsScope::Settings::isKeepLightsOnAfterExit())
			return;
#ifdef Q_OS_WIN
		__try {
			g_shutdownCallback();
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			// Swallow - we're already handling a fatal error, this is a last-ditch attempt.
		}
#else
		try {
			g_shutdownCallback();
		} catch (...) {
		}
#endif
	}

	void writeCrashLog(const QString& logPath, const QString& reason, const QString& detail)
	{
		QDir().mkpath(g_logsDir);
		QFile file(logPath);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		out << QStringLiteral("Prismatik ") << QStringLiteral(VERSION_STR) << QStringLiteral(" crashed\n");
		out << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
		out << reason << "\n";
		if (!detail.isEmpty())
			out << detail << "\n";
	}

#ifdef Q_OS_WIN
	void writeMinidump(const QString& dumpPath, EXCEPTION_POINTERS* exceptionPointers)
	{
		HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(dumpPath.utf16()), GENERIC_WRITE, 0,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return;

		MINIDUMP_EXCEPTION_INFORMATION mdei{};
		MINIDUMP_EXCEPTION_INFORMATION* mdeiPtr = nullptr;
		if (exceptionPointers) {
			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers = exceptionPointers;
			mdei.ClientPointers = FALSE;
			mdeiPtr = &mdei;
		}

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
			MiniDumpNormal, mdeiPtr, nullptr, nullptr);
		CloseHandle(file);
	}

	QString exceptionCodeDescription(DWORD code)
	{
		switch (code) {
		case EXCEPTION_ACCESS_VIOLATION: return QStringLiteral("Access violation (invalid memory read/write)");
		case EXCEPTION_STACK_OVERFLOW: return QStringLiteral("Stack overflow");
		case EXCEPTION_ILLEGAL_INSTRUCTION: return QStringLiteral("Illegal instruction");
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return QStringLiteral("Integer division by zero");
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return QStringLiteral("Array bounds exceeded");
		case EXCEPTION_PRIV_INSTRUCTION: return QStringLiteral("Privileged instruction");
		// The MSVC C++ runtime implements exception propagation on top of SEH using this
		// code - an uncaught C++ exception surfaces here (as an "unhandled exception")
		// before std::terminate() ever gets a chance to run, in practice on Windows.
		case 0xE06D7363: return QStringLiteral("Uncaught C++ exception (propagated as an unhandled Windows exception)");
		default: return QStringLiteral("Unhandled exception (code 0x%1)").arg(static_cast<uint>(code), 8, 16, QLatin1Char('0'));
		}
	}

	void showCrashMessageBox(const QString& reason, const QString& logPath)
	{
		const QString text = QStringLiteral(
			"Prismatik crashed unexpectedly and will now close.\n\n"
			"Reason: %1\n\n"
			"The LEDs were switched off (best effort, unless \"Keep lights ON after exit\" is "
			"enabled) and a crash report was saved to:\n%2\n\n"
			"Please attach that file (and the matching .dmp next to it) if you report this issue."
		).arg(reason, QDir::toNativeSeparators(logPath));
		MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(text.utf16()), L"Prismatik - Fatal Error",
			MB_OK | MB_ICONERROR | MB_TOPMOST);
	}

	LONG WINAPI sehFilter(EXCEPTION_POINTERS* exceptionPointers)
	{
		const QString stamp = timestampForFilenames();
		const QString logPath = g_logsDir + QStringLiteral("/crash_%1.log").arg(stamp);
		const QString dumpPath = g_logsDir + QStringLiteral("/crash_%1.dmp").arg(stamp);
		const QString reason = exceptionCodeDescription(
			exceptionPointers ? exceptionPointers->ExceptionRecord->ExceptionCode : 0);

		writeCrashLog(logPath, reason, QStringLiteral("Unhandled SEH exception."));
		writeMinidump(dumpPath, exceptionPointers);
		runShutdownCallback();
		showCrashMessageBox(reason, logPath);

		ExitProcess(1);
		return EXCEPTION_EXECUTE_HANDLER; // unreachable - ExitProcess() doesn't return
	}
#endif

	void terminateHandler()
	{
		QString detail;
		try {
			if (std::exception_ptr eptr = std::current_exception())
				std::rethrow_exception(eptr);
		} catch (const std::exception& e) {
			detail = QStringLiteral("Uncaught C++ exception: %1").arg(QString::fromUtf8(e.what()));
		} catch (...) {
			detail = QStringLiteral("Uncaught C++ exception of unknown type.");
		}
		if (detail.isEmpty())
			detail = QStringLiteral("std::terminate() called with no active exception "
				"(likely a noexcept violation or a pure virtual call).");

		const QString stamp = timestampForFilenames();
		const QString logPath = g_logsDir + QStringLiteral("/crash_%1.log").arg(stamp);
		writeCrashLog(logPath, detail, QString());
#ifdef Q_OS_WIN
		const QString dumpPath = g_logsDir + QStringLiteral("/crash_%1.dmp").arg(stamp);
		writeMinidump(dumpPath, nullptr);
#endif
		runShutdownCallback();
#ifdef Q_OS_WIN
		showCrashMessageBox(detail, logPath);
		ExitProcess(1);
#else
		std::abort();
#endif
	}
}

void install(const QString& logsDir)
{
	g_logsDir = logsDir;
#ifdef Q_OS_WIN
	SetUnhandledExceptionFilter(sehFilter);
#endif
	std::set_terminate(terminateHandler);
}

void setShutdownCallback(std::function<void()> callback)
{
	g_shutdownCallback = std::move(callback);
}

}
