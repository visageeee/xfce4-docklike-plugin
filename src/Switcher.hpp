// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace Switcher
{
void init();
void finalize();
bool supported();
bool setEnabled(bool enabled);
void cycle(bool reverse);
void cycleWindows(bool reverse, unsigned int timestamp);
void refreshBadges();
}
