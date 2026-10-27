// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <libxfce4windowing/libxfce4windowing.h>
#include <vector>
namespace ForceQuit
{
void windows(const std::vector<XfwWindow*>& windows);
}
