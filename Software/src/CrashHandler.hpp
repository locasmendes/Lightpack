/*
 * CrashHandler.hpp
 *
 *	Project: Lightpack
 */

#pragma once

#include <functional>
#include <QString>

// Installs process-wide handlers for fatal errors that today just make Prismatik vanish
// with zero trace: Windows SEH exceptions (access violations, stack overflows, ...) and
// uncaught C++ exceptions from any thread. On a fatal error, this: writes a crash log and a
// minidump to logsDir (unconditionally, regardless of the user's debug log level - a crash
// is exceptional, not routine verbosity), best-effort turns the LEDs off via the registered
// shutdown callback, shows a native message box explaining what happened and where the
// report was saved, then terminates the process. It never tries to keep the app running
// after a real crash - by that point the process state may be corrupted, so failing loudly
// and safely beats limping on.
namespace CrashHandler
{
	// Call once, as early as possible after LightpackApplication is constructed (so
	// configDir()-based paths are available).
	void install(const QString& logsDir);

	// Registered by LightpackApplication once its LED device manager exists. Invoked on a
	// best-effort basis while handling a fatal error - guarded so a failure here can't
	// prevent the rest of the crash handling (log/minidump/message box) from completing.
	void setShutdownCallback(std::function<void()> callback);
}
