/*
 * Copyright (c) 2019-2020 Nicolas Szabo <nszabo@vivaldi.net>
 * Copyright (c) 2020-2021 David Keogh <davidtkeogh@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Dock.hpp"
#include "LauncherEntry.hpp"
#include "DockLayout.hpp"

// A GtkBox whose minimum length is independent of its full-size icons.
// Xfce can then reserve space for the other plugins before allocating this one.
struct DocklikeTestBox { GtkBox parent; };
struct DocklikeTestBoxClass { GtkBoxClass parent; };
static GType docklike_test_box_get_type();
G_DEFINE_TYPE(DocklikeTestBox, docklike_test_box, GTK_TYPE_BOX)

static void docklike_test_box_init(DocklikeTestBox*) {}

namespace Dock
{
	GtkWidget* mBox;
	Store::KeyStore<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> mGroups;

	int mPanelSize;
	int mIconSize;
	static int preferredIconSize = 16;
	static GtkCssProvider* buttonSizing;
	static std::string appliedSizing;

	static int visibleCount()
	{
		int count = 0;
		GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));
		for (GList* child = children; child; child = child->next)
			if (gtk_widget_get_visible(GTK_WIDGET(child->data)))
				++count;
		g_list_free(children);
		return count;
	}

	static void setButtonPadding(int padding, int margin = 0)
	{
		const bool horizontal = gtk_orientable_get_orientation(GTK_ORIENTABLE(mBox)) == GTK_ORIENTATION_HORIZONTAL;
		// Widget margins are layout space, independent of CSS shorthand and themes.
		GList* buttons = gtk_container_get_children(GTK_CONTAINER(mBox));
		for (GList* child = buttons; child; child = child->next)
		{
			GtkWidget* button = GTK_WIDGET(child->data);
			gtk_widget_set_margin_top(button, horizontal ? 0 : margin);
			gtk_widget_set_margin_bottom(button, horizontal ? 0 : margin);
			gtk_widget_set_margin_start(button, horizontal ? margin : 0);
			gtk_widget_set_margin_end(button, horizontal ? margin : 0);
		}
		g_list_free(buttons);
		gchar* css = g_strdup_printf("button.group { min-width: 0; min-height: 0; margin: 0; border-width: 0; padding: %dpx; }", padding);
		if (appliedSizing != css)
		{
			appliedSizing = css;
			gtk_css_provider_load_from_data(buttonSizing, css, -1, nullptr);
			// Margins can change without changing the icon size. Invalidate each
			// button's cached request before GtkBox allocates this same frame.
			GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));
			for (GList* child = children; child; child = child->next)
				gtk_widget_queue_resize(GTK_WIDGET(child->data));
			g_list_free(children);
		}
		g_free(css);
	}

	void styleButton(GtkWidget* button)
	{
		gtk_style_context_add_provider(gtk_widget_get_style_context(button),
			GTK_STYLE_PROVIDER(buttonSizing), GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
	}

	void init()
	{
		mBox = GTK_WIDGET(g_object_new(docklike_test_box_get_type(), "orientation", GTK_ORIENTATION_HORIZONTAL, nullptr));
		buttonSizing = gtk_css_provider_new();
		appliedSizing.clear();
		setButtonPadding(4);
		// Contexts retain the provider; release our reference with the dock.
		g_object_set_data_full(G_OBJECT(mBox), "button-sizing", buttonSizing, g_object_unref);
		gtk_widget_set_name(mBox, "docklike-test-plugin");


		gtk_widget_show(mBox);
		drawGroups();
	}

	Group* prepareGroup(std::shared_ptr<AppInfo> appInfo)
	{
		std::shared_ptr<Group> group = mGroups.get(appInfo);

		if (!group)
		{
			group = std::make_shared<Group>(appInfo, false);
			mGroups.push(appInfo, group);
			gtk_container_add(GTK_CONTAINER(mBox), group->mButton);
			LauncherEntry::refreshGroups();
		}

		return group.get();
	}

	void moveButton(Group* moving, Group* dest)
	{
		int startpos = Help::Gtk::getChildPosition(GTK_CONTAINER(mBox), moving->mButton);
		int destpos = Help::Gtk::getChildPosition(GTK_CONTAINER(mBox), dest->mButton);

		if (startpos == destpos)
			return;

		gtk_box_reorder_child(GTK_BOX(mBox), moving->mButton, destpos);

		savePinned();
	}

	void savePinned()
	{
		std::list<std::string> pinnedList;
		GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));

		for (GList* child = children; child != nullptr; child = child->next)
		{
			GtkWidget* widget = (GtkWidget*)child->data;
			Group* group = (Group*)g_object_get_data(G_OBJECT(widget), "group");

			if (group->mPinned && g_file_test(group->mAppInfo->mPath.c_str(), G_FILE_TEST_IS_REGULAR))
				pinnedList.push_back(group->mAppInfo->mId);
		}

		Settings::pinnedAppList.set(pinnedList);
		g_list_free(children);
	}

	void drawGroups()
	{
		// Remove old groups
		Xfw::mGroupWindows.clear();
		mGroups.clear();

		// Add pinned groups
		std::list<std::string> pinnedApps = Settings::pinnedAppList;
		std::list<std::string>::iterator it = pinnedApps.begin();

		while (it != pinnedApps.end())
		{
			std::shared_ptr<AppInfo> appInfo = AppInfos::search(Help::String::toLowercase(*it));
			std::shared_ptr<Group> group = std::make_shared<Group>(appInfo, true);

			mGroups.push(appInfo, group);
			gtk_container_add(GTK_CONTAINER(mBox), group->mButton);
			++it;
		}

		// Add open windows
		for (GList* window_l = xfw_screen_get_windows(Xfw::mXfwScreen);
			 window_l != nullptr;
			 window_l = window_l->next)
		{
			XfwWindow* xfwWindow = XFW_WINDOW(window_l->data);
			std::shared_ptr<GroupWindow> groupWindow = Xfw::mGroupWindows.get(xfwWindow);

			if (!groupWindow)
				groupWindow = std::make_shared<GroupWindow>(xfwWindow);
			else
				gtk_container_add(GTK_CONTAINER(mBox), groupWindow->mGroup->mButton);

			Xfw::mGroupWindows.push(xfwWindow, groupWindow);
			groupWindow->updateState();
		}

		gtk_widget_queue_draw(mBox);
		LauncherEntry::refreshGroups();
	}

	static void activateGroup(Group* group)
	{
		if (group->mActive)
			group->scrollWindows(GDK_CURRENT_TIME, GDK_SCROLL_DOWN);
		else if (group->mWindowsCount > 0)
			group->activate(GDK_CURRENT_TIME);
		else
			group->mAppInfo->launch();
	}

	void activateGroup(int nb)
	{
		int i = 0;
		GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));

		for (GList* child = children; child != nullptr; child = child->next)
		{
			GtkWidget* widget = (GtkWidget*)child->data;

			if (gtk_widget_get_visible(widget))
			{
				if (i == nb)
				{
					Group* group = (Group*)g_object_get_data(G_OBJECT(widget), "group");
					activateGroup(group);
					return;
				}
				else
					++i;
			}
		}

		g_list_free(children);
	}

	void activateGroup(const std::string& appId)
	{
		std::shared_ptr<Group> group =
			mGroups.findIf([appId](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> g) -> bool {
				return g.first->mId == appId;
			});
		if (group)
			activateGroup(group.get());
	}

	void onPanelResize(int size)
	{
		if (size != -1)
			mPanelSize = size;


		if (Settings::forceIconSize)
			preferredIconSize = Settings::iconSize;
		else
		{
			if (mPanelSize <= 20)
				preferredIconSize = mPanelSize - 6;
			else if (mPanelSize <= 28)
				preferredIconSize = 16;
			else if (mPanelSize <= 38)
				preferredIconSize = 24;
			else if (mPanelSize <= 41)
				preferredIconSize = 32;
			else
				preferredIconSize = mPanelSize * 0.8;
		}

		preferredIconSize = std::max(1, preferredIconSize);
		gtk_widget_queue_resize(mBox);
	}

	void onPanelOrientationChange(GtkOrientation orientation)
	{
		gtk_orientable_set_orientation(GTK_ORIENTABLE(mBox), orientation);

		gtk_widget_queue_resize(mBox);
	}
} // namespace Dock

static void docklike_test_box_measure(GtkWidget* widget, GtkOrientation axis, gint* minimum, gint* natural)
{
	const GtkOrientation orientation = gtk_orientable_get_orientation(GTK_ORIENTABLE(widget));
	if (axis == orientation)
	{
		const int count = Dock::visibleCount();
		*minimum = std::max(1, count);
		// Never base the natural size on the currently compressed icons: doing so
		// prevents the dock growing back after another panel item disappears.
		*natural = std::max(*minimum, std::max(static_cast<int>(Settings::dockSize),
			count * (Dock::preferredIconSize + 8 + 2 * static_cast<int>(Settings::iconMargin)) + std::max(0, count - 1) * (Dock::mPanelSize / 10)));
	}
	else
		*minimum = *natural = std::max(1, Dock::mPanelSize);
}

static void docklike_test_box_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
	const bool horizontal = gtk_orientable_get_orientation(GTK_ORIENTABLE(widget)) == GTK_ORIENTATION_HORIZONTAL;
	const auto sizes = DockLayout::fit(horizontal ? allocation->width : allocation->height,
		horizontal ? allocation->height : allocation->width, Dock::visibleCount(),
		Dock::preferredIconSize, Dock::mPanelSize / 10, Settings::iconMargin);
	Dock::setButtonPadding(sizes.padding, sizes.margin);
	gtk_box_set_spacing(GTK_BOX(widget), sizes.spacing);
	if (Dock::mIconSize != sizes.icon)
	{
		Dock::mIconSize = sizes.icon;
		Dock::mGroups.forEach([](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> g) {
			g.second->resize();
		});
	}
	GTK_WIDGET_CLASS(docklike_test_box_parent_class)->size_allocate(widget, allocation);
}

static void docklike_test_box_class_init(DocklikeTestBoxClass* klass)
{
	GtkWidgetClass* widget = GTK_WIDGET_CLASS(klass);
	widget->get_request_mode = +[](GtkWidget*) { return GTK_SIZE_REQUEST_CONSTANT_SIZE; };
	widget->get_preferred_width = +[](GtkWidget* w, gint* m, gint* n) {
		docklike_test_box_measure(w, GTK_ORIENTATION_HORIZONTAL, m, n);
	};
	widget->get_preferred_height = +[](GtkWidget* w, gint* m, gint* n) {
		docklike_test_box_measure(w, GTK_ORIENTATION_VERTICAL, m, n);
	};
	widget->get_preferred_width_for_height = +[](GtkWidget* w, gint, gint* m, gint* n) {
		docklike_test_box_measure(w, GTK_ORIENTATION_HORIZONTAL, m, n);
	};
	widget->get_preferred_height_for_width = +[](GtkWidget* w, gint, gint* m, gint* n) {
		docklike_test_box_measure(w, GTK_ORIENTATION_VERTICAL, m, n);
	};
	widget->size_allocate = docklike_test_box_allocate;
}
