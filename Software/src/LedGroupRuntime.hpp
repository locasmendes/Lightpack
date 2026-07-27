/*
 * LedGroupRuntime.hpp
 *
 *  Created on: 27.07.2026
 *		Author: Lightpack Team / Personal Fork
 *		Project: Lightpack
 */

#pragma once

#include "Settings.hpp"

class LedGroupRuntime {
public:
	/*!
	 * Applies a single LedGroup override directly to Settings.
	 * Returns true if successful. If group is disabled, returns true as no-op.
	 * Invalid memberIds (outside [0, maxLeds)) are silently skipped.
	 */
	static bool applyGroup(const SettingsScope::LedGroup& group);

	/*!
	 * Applies all enabled LedGroups defined in the current profile in creation order.
	 * If memberIds overlap across groups, the last group applied wins for that LED.
	 * If no LedGroups are defined, this function returns true without changing any settings.
	 */
	static bool applyAll();
};
