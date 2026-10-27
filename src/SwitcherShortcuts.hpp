// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace SwitcherShortcuts
{
bool init();
bool enable(std::string& error);
bool restore(std::string& error);
void finalize();
}
