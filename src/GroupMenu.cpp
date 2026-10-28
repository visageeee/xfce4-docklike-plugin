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

#ifdef ENABLE_WAYLAND
#include <gtk-layer-shell.h>
#endif

#include "Group.hpp"
#include "GroupMenu.hpp"
#include "GroupMenuItem.hpp"
#include "Plugin.hpp"

static GtkWidget*
create_window()
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_add_events(window, GDK_SCROLL_MASK);
	gtk_window_set_default_size(GTK_WINDOW(window), 1, 1);
	gtk_window_set_title(GTK_WINDOW(window), "Docklike Hover");

	gtk_widget_set_app_paintable(window, true);
	GdkScreen* screen = gtk_widget_get_screen(window);
	GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
	if (visual != nullptr)
	{
		gtk_widget_set_visual(window, visual);
	}

#ifdef ENABLE_WAYLAND
	if (gtk_layer_is_supported())
	{
		gtk_layer_init_for_window(GTK_WINDOW(window));
		gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);
		gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, true);
		gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, true);
	}
#endif

	return window;
}

GroupMenu::GroupMenu(Group* dockButton)
{
	mGroup = dockButton;
	mVisible = false;
	mMouseHover = false;
	mWindow = create_window();
	mBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	Help::Gtk::cssClassAdd(mWindow, "xfce-docklike-test-window");
	Help::Gtk::cssClassAdd(mBox, "menu");

	mContent = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	mTooltipLabel = gtk_label_new("");
	gtk_widget_set_margin_start(mTooltipLabel, 8);
	gtk_widget_set_margin_end(mTooltipLabel, 8);
	gtk_widget_set_margin_top(mTooltipLabel, 5);
	gtk_widget_set_margin_bottom(mTooltipLabel, 5);
	gtk_label_set_ellipsize(GTK_LABEL(mTooltipLabel), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(mTooltipLabel), 48);
	Help::Gtk::cssClassAdd(mContent, "menu");
	gtk_box_pack_start(GTK_BOX(mContent), mBox, true, true, 0);
	gtk_box_pack_start(GTK_BOX(mContent), mTooltipLabel, false, false, 0);
	gtk_container_add(GTK_CONTAINER(mWindow), mContent);
	gtk_widget_show(mContent);
	gtk_widget_show(mBox);

	mPopupIdle.setup([this]() {
		popup();
		return false;
	});

	//--------------------------------------------------

	g_signal_connect(G_OBJECT(mWindow), "enter-notify-event",
		G_CALLBACK(+[](GtkWidget* widget, GdkEvent* event, GroupMenu* me) {
			me->mMouseHover = true;

			if (Settings::showPreviews)
				me->mGroup->mWindows.forEach([](GroupWindow* w) -> void {
					w->mGroupMenuItem->updatePreview();
				});

			return true;
		}),
		this);

	g_signal_connect(G_OBJECT(mWindow), "leave-notify-event",
		G_CALLBACK(+[](GtkWidget* widget, GdkEventCrossing* event, GroupMenu* me) {
			if (event->detail == GDK_NOTIFY_INFERIOR)
			{
				return false;
			}
			me->mGroup->setMouseLeaveTimeout();
			me->mMouseHover = false;

			return true;
		}),
		this);

	g_signal_connect(G_OBJECT(mWindow), "scroll-event",
		G_CALLBACK(+[](GtkWidget* widget, GdkEventScroll* event, GroupMenu* me) {
			((Group*)me->mGroup)->scrollWindows(event->time, event->direction);
			return true;
		}),
		this);

	g_signal_connect(G_OBJECT(Plugin::mXfPlugin), "notify::scale-factor",
		G_CALLBACK(+[](GtkWidget* widget, GParamSpec* pspec, GroupMenu* me) {
			g_object_ref(me->mContent);
			gtk_container_remove(GTK_CONTAINER(me->mWindow), me->mContent);
			gtk_widget_destroy(me->mWindow);
			me->mWindow = create_window();
			gtk_container_add(GTK_CONTAINER(me->mWindow), me->mContent);
			g_object_unref(me->mContent);
		}),
		this);
}

GroupMenu::~GroupMenu()
{
	mPopupIdle.stop();
	gtk_widget_destroy(mWindow);
}

void GroupMenu::add(GroupMenuItem* menuItem)
{
	gtk_box_pack_end(GTK_BOX(mBox), GTK_WIDGET(menuItem->mItem), false, true, 0);

	if (mVisible)
		mPopupIdle.start();
}

void GroupMenu::remove(GroupMenuItem* menuItem)
{
	gtk_container_remove(GTK_CONTAINER(mBox), GTK_WIDGET(menuItem->mItem));
	gtk_window_resize(GTK_WINDOW(mWindow), 1, 1);

	if (mGroup->mWindowsCount < (Settings::noWindowsListIfSingle ? 2 : 1))
		gtk_widget_hide(mWindow);

	if (mVisible)
		mPopupIdle.start();
}

void GroupMenu::popup()
{
	if (!Settings::iconTooltipsEnabled || !gtk_widget_get_mapped(mGroup->mButton)) return;
	const bool list = mGroup->mWindowsCount >= (Settings::noWindowsListIfSingle ? 2 : 1);
	// Use the same icon anchor for short app-name tooltips and window lists.
	// GTK's default tooltip is pointer-positioned and tends to sit below it.
	gtk_widget_set_visible(mBox, list);
	gtk_widget_set_visible(mTooltipLabel, !list);
	if (!list) gtk_label_set_text(GTK_LABEL(mTooltipLabel), mGroup->mAppInfo->mName.c_str());
	updateOrientation();
	if (list && Settings::showPreviews)
		mGroup->mWindows.forEach([](GroupWindow* w) { w->mGroupMenuItem->updatePreview(); });
	gint wx, wy;
	xfce_panel_plugin_position_widget(Plugin::mXfPlugin, mWindow, mGroup->mButton, &wx, &wy);
	updatePosition(wx, wy);
	mVisible = true;
	gtk_widget_show(mWindow);
}

void GroupMenu::updateOrientation()
{
	XfcePanelPluginMode panelMode = xfce_panel_plugin_get_mode(Plugin::mXfPlugin);

	if (Settings::showPreviews && panelMode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
		gtk_orientable_set_orientation(GTK_ORIENTABLE(mBox), GTK_ORIENTATION_HORIZONTAL);
	else
		gtk_orientable_set_orientation(GTK_ORIENTABLE(mBox), GTK_ORIENTATION_VERTICAL);
}

void GroupMenu::updatePosition(gint wx, gint wy)
{
	GdkScreen* screen;
	GdkRectangle geometry;
	GdkDisplay* display;
	GdkMonitor* monitor;

	screen = gtk_widget_get_screen(mGroup->mButton);
	display = gdk_screen_get_display(screen);
	monitor = gdk_display_get_monitor_at_window(display, gtk_widget_get_window(mGroup->mButton));
	gdk_monitor_get_geometry(monitor, &geometry);

	// Measure the current contents, not the old popup allocation (or its
	// initial 1x1 size), before centring it against the button's root position.
	GtkRequisition minimum, natural;
	gtk_widget_get_preferred_size(mWindow, &minimum, &natural);
	const int window_width = std::max(1, natural.width);
	const int window_height = std::max(1, natural.height);
	GtkWidget* top = gtk_widget_get_toplevel(mGroup->mButton);
	gint bx = 0, by = 0, rootX = 0, rootY = 0;
	gtk_widget_translate_coordinates(mGroup->mButton, top, 0, 0, &bx, &by);
	gdk_window_get_origin(gtk_widget_get_window(top), &rootX, &rootY);
	bx += rootX; by += rootY;
	const int button_width = gtk_widget_get_allocated_width(mGroup->mButton);
	const int button_height = gtk_widget_get_allocated_height(mGroup->mButton);
	const auto mode = xfce_panel_plugin_get_mode(Plugin::mXfPlugin);
	const auto position = xfce_panel_plugin_get_screen_position(Plugin::mXfPlugin);
	const int gap = 6;
	if (mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
	{
		wx = bx + (button_width - window_width) / 2;
		const bool bottom = xfce_screen_position_is_bottom(position)
			|| (!xfce_screen_position_is_top(position) && by + button_height / 2 > geometry.y + geometry.height / 2);
		wy = bottom ? by - window_height - gap : by + button_height + gap;
	}
	else
	{
		wy = by + (button_height - window_height) / 2;
		const bool right = xfce_screen_position_is_right(position)
			|| (!xfce_screen_position_is_left(position) && bx + button_width / 2 > geometry.x + geometry.width / 2);
		wx = right ? bx - window_width - gap : bx + button_width + gap;
	}
	wx = std::max(geometry.x, std::min(wx, geometry.x + geometry.width - window_width));
	wy = std::max(geometry.y, std::min(wy, geometry.y + geometry.height - window_height));
	gtk_window_resize(GTK_WINDOW(mWindow), window_width, window_height);

#ifdef ENABLE_WAYLAND
	if (gtk_layer_is_supported())
	{
		gtk_layer_set_monitor(GTK_WINDOW(mWindow), monitor);
		gtk_layer_set_margin(GTK_WINDOW(mWindow), GTK_LAYER_SHELL_EDGE_LEFT, wx - geometry.x);
		gtk_layer_set_margin(GTK_WINDOW(mWindow), GTK_LAYER_SHELL_EDGE_TOP, wy - geometry.y);
	}
	else
#endif
	{
		gtk_window_move(GTK_WINDOW(mWindow), wx, wy);
	}
}

void GroupMenu::hide()
{
	mVisible = false;
	gtk_widget_hide(mWindow);
}

void GroupMenu::showPreviewsChanged()
{
	mGroup->mWindows.forEach([](GroupWindow* w) -> void {
		gtk_widget_set_visible(GTK_WIDGET(w->mGroupMenuItem->mPreview), Settings::showPreviews);
		w->mGroupMenuItem->mPreviewTimeout.stop();
	});
	gtk_window_resize(GTK_WINDOW(mWindow), 1, 1);
}

uint GroupMenu::getPointerDistance()
{
	gint wx, wy, ww, wh, px, py;
	guint dx, dy;
	dx = dy = 0;

	gtk_window_get_position(GTK_WINDOW(mWindow), &wx, &wy);
	gtk_window_get_size(GTK_WINDOW(mWindow), &ww, &wh);
	gdk_device_get_position(Plugin::mPointer, nullptr, &px, &py);

	if (px < wx)
		dx = wx - px;
	else if (px > wx + ww)
		dx = px - (wx + ww);

	if (py < wy)
		dy = wy - py;
	else if (py > wy + wh)
		dy = py - (wy + wh);

	return std::max(dx, dy);
}
