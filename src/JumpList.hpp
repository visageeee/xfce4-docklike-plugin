// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AppInfos.hpp"
#include <gtk/gtk.h>
#include <vector>
namespace JumpList
{
struct File { std::string uri, label; time_t modified; };
std::vector<File> recentFiles(const AppInfo& app, GtkRecentManager* manager);
void append(GtkWidget* menu, std::shared_ptr<AppInfo> app);
}
