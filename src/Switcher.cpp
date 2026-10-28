// SPDX-License-Identifier: GPL-3.0-or-later
#include "Switcher.hpp"
#include "IconHover.hpp"
#include "SwitcherShortcuts.hpp"
#include "Dock.hpp"
#include <map>
#include <set>
#include <vector>
#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

namespace Switcher
{
namespace
{
struct Entry { std::weak_ptr<Group> group; GtkWidget* card; std::string id; GtkWidget* badge; };
GtkWidget* popup = nullptr;
GtkWidget* grid = nullptr;
GtkWidget* scroller = nullptr;
GdkSeat* grabbedSeat = nullptr;
std::vector<Entry> entries;
std::map<std::string, guint64> recent;
guint64 serial = 0;
int selected = 0;
int columns = 1;
guint refreshSource = 0;
guint releaseSource = 0;
bool quitKeyDown = false;
bool enabled = false;
bool ready = false;
int signalTag;

std::string identity(Group* group) { return group->mAppInfo->mId + "\n" + group->mAppInfo->mPath + "\n" + group->mAppInfo->mName; }

void noteActive()
{
	auto w = Xfw::mGroupWindows.get(Xfw::getActiveWindow());
	if (w && w->mGroup) recent[identity(w->mGroup)] = ++serial;
}

void close(bool activate, guint32 timestamp = GDK_CURRENT_TIME)
{
	std::shared_ptr<Group> target;
	if (activate && !entries.empty()) target = entries[selected].group.lock();
	if (refreshSource) { g_source_remove(refreshSource); refreshSource = 0; }
	if (releaseSource) { g_source_remove(releaseSource); releaseSource = 0; }
	if (grabbedSeat) { gdk_seat_ungrab(grabbedSeat); grabbedSeat = nullptr; }
	GtkWidget* old = popup; popup = nullptr; grid = scroller = nullptr;
	entries.clear(); quitKeyDown = false;
	if (old) gtk_widget_destroy(old);
	if (target && target->mWindowsCount > 0) target->activate(timestamp);
}

void revealSelection()
{
	if (!popup || entries.empty()) return;
	gint x, y;
	if (!gtk_widget_translate_coordinates(entries[selected].card, grid, 0, 0, &x, &y)) return;
	GtkAdjustment* adjust = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller));
	const double current = gtk_adjustment_get_value(adjust);
	const double page = gtk_adjustment_get_page_size(adjust);
	const int height = gtk_widget_get_allocated_height(entries[selected].card);
	if (y < current) gtk_adjustment_set_value(adjust, y);
	else if (y + height > current + page) gtk_adjustment_set_value(adjust, y + height - page);
}

void select(int index)
{
	if (entries.empty()) return;
	selected = (index % static_cast<int>(entries.size()) + entries.size()) % entries.size();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		auto* context = gtk_widget_get_style_context(entries[i].card);
		if (static_cast<int>(i) == selected) gtk_style_context_add_class(context, "selected");
		else gtk_style_context_remove_class(context, "selected");
	}
	revealSelection();
}

GdkRectangle monitorArea()
{
	GdkDisplay* display = gdk_display_get_default();
	GdkWindow* anchor = gtk_widget_get_window(GTK_WIDGET(Plugin::mXfPlugin));
	GdkMonitor* monitor = anchor ? gdk_display_get_monitor_at_window(display, anchor) : gdk_display_get_primary_monitor(display);
	if (!monitor) monitor = gdk_display_get_monitor(display, 0);
	GdkRectangle area = {0, 0, 1024, 768};
	if (monitor) gdk_monitor_get_workarea(monitor, &area);
	return area;
}

void centre()
{
	if (!popup) return;
	const auto area = monitorArea();
	GtkRequisition minimum, natural;
	gtk_widget_get_preferred_size(popup, &minimum, &natural);
	// GtkWindow keeps its previous allocation when children disappear.
	// Explicitly request the new content size before re-centring it.
	if (gtk_widget_get_realized(popup))
		gtk_window_resize(GTK_WINDOW(popup), std::max(1, natural.width), std::max(1, natural.height));
	gtk_window_move(GTK_WINDOW(popup), area.x + (area.width - natural.width) / 2,
		area.y + (area.height - natural.height) / 2);
}

void rebuild(bool initial, bool reverse = false)
{
	std::string previous = !entries.empty() ? entries[selected].id : "";
	std::vector<std::shared_ptr<Group>> groups;
	GList* buttons = gtk_container_get_children(GTK_CONTAINER(Dock::mBox));
	for (GList* child = buttons; child; child = child->next)
	{
		auto* g = static_cast<Group*>(g_object_get_data(G_OBJECT(child->data), "group"));
		if (g && g->mWindowsCount > 0 && gtk_widget_get_visible(GTK_WIDGET(child->data)))
		{
			auto shared = Dock::mGroups.get(g->mAppInfo);
			if (shared) groups.push_back(shared);
		}
	}
	g_list_free(buttons);
	std::stable_sort(groups.begin(), groups.end(), [](const std::shared_ptr<Group>& a, const std::shared_ptr<Group>& b) {
		if (a->mActive != b->mActive) return a->mActive;
		return recent[identity(a.get())] > recent[identity(b.get())];
	});
	if (groups.empty()) { close(false); return; }
	buttons = gtk_container_get_children(GTK_CONTAINER(grid));
	for (GList* child = buttons; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
	g_list_free(buttons); entries.clear();
	const auto area = monitorArea();
	const bool listView = Settings::altTabListView;
	columns = listView ? 1 : std::max(1, std::min(6, (area.width - 64) / 116));
	int preserved = std::min(selected, static_cast<int>(groups.size()) - 1);
	for (size_t i = 0; i < groups.size(); ++i)
	{
		const auto& group = groups[i];
		GtkWidget* card = gtk_button_new();
		gtk_widget_set_name(card, "docklike-switcher-card");
		gtk_widget_set_can_focus(card, false);
		gtk_button_set_relief(GTK_BUTTON(card), GTK_RELIEF_NONE);
		GtkWidget* box = gtk_box_new(listView ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, listView ? 12 : 8);
		auto* first = group->mWindows.get(group->mTopWindowIndex);
		auto* app = xfw_window_get_application(first->mXfwWindow);
		GIcon* icon = app ? xfw_application_get_gicon(app) : nullptr;
		GtkWidget* image = icon ? gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DIALOG)
			: gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
		gtk_image_set_pixel_size(GTK_IMAGE(image), listView ? 28 : 48);
		IconHover::bind(image, card);
		GtkWidget* label = gtk_label_new(group->mAppInfo->mName.c_str());
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(label), listView ? 36 : 13);
		gtk_widget_set_size_request(label, listView ? std::min(280, std::max(86, area.width - 160)) : 86, -1);
		if (listView) gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
		GtkWidget* overlay = gtk_overlay_new();
		gtk_container_add(GTK_CONTAINER(overlay), image);
		GtkWidget* badge = gtk_label_new("");
		gtk_widget_set_name(badge, "docklike-switcher-badge");
		gtk_widget_set_no_show_all(badge, true);
		gtk_widget_set_halign(badge, GTK_ALIGN_END);
		gtk_widget_set_valign(badge, GTK_ALIGN_START);
		gtk_overlay_add_overlay(GTK_OVERLAY(overlay), badge);
		gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), badge, true);
		gtk_box_pack_start(GTK_BOX(box), overlay, false, false, 0);
		gtk_box_pack_start(GTK_BOX(box), label, listView, listView, 0);
		gtk_container_add(GTK_CONTAINER(card), box);
		gtk_grid_attach(GTK_GRID(grid), card, i % columns, i / columns, 1, 1);
		g_signal_connect(card, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer) {
			for (size_t index = 0; index < entries.size(); ++index)
				if (entries[index].card == GTK_WIDGET(button)) { select(index); close(true, gtk_get_current_event_time()); break; }
		}), nullptr);
		entries.push_back({group, card, identity(group.get()), badge});
		if (entries.back().id == previous) preserved = i;
	}
	gtk_widget_show_all(grid);
	refreshBadges();
	if (initial)
		select(groups[0]->mActive ? (reverse ? groups.size() - 1 : 1) : (reverse ? groups.size() - 1 : 0));
	else select(preserved);
	centre();
}

bool altPressed()
{
	GdkModifierType mask = static_cast<GdkModifierType>(0);
	GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
	gdk_window_get_device_position(gdk_get_default_root_window(), gdk_seat_get_pointer(seat), nullptr, nullptr, &mask);
	return (mask & GDK_MOD1_MASK) != 0;
}

void show(bool reverse)
{
	popup = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_name(popup, "docklike-app-switcher");
	gtk_window_set_title(GTK_WINDOW(popup), _("Docklike App Switcher"));
	gtk_window_set_decorated(GTK_WINDOW(popup), false);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(popup), true);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(popup), true);
	gtk_window_set_keep_above(GTK_WINDOW(popup), true);
	gtk_container_set_border_width(GTK_CONTAINER(popup), 20);
	gtk_widget_add_events(popup, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
	scroller = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroller), true);
	gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroller), true);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroller), std::max(100, monitorArea().height * 2 / 3 - 100));
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
	gtk_container_add(GTK_CONTAINER(scroller), grid);
	gtk_box_pack_start(GTK_BOX(outer), scroller, true, true, 0);
	gtk_container_add(GTK_CONTAINER(popup), outer);
	GtkCssProvider* css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css,
		"#docklike-app-switcher { background: #24262c; color: #f5f5f7; border: 1px solid #555965; border-radius: 0; }"
		"#docklike-app-switcher label { color: #f5f5f7; }"
		"#docklike-switcher-card { padding: 12px 8px; margin: 0; background-image: none; background-color: transparent; border: 2px solid transparent; border-radius: 0; box-shadow: none; }"
		"#docklike-app-switcher #docklike-switcher-badge { background: #cf3b46; color: white; font-size: 11px; font-weight: bold; padding: 1px 4px; min-width: 12px; border-radius: 0; }"
		"#docklike-switcher-card.selected { background-color: #3a4663; border-color: #91b3ff; }", -1, nullptr);
	// Scoped selectors keep the dock and the rest of the panel unchanged.
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(popup), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_set_data_full(G_OBJECT(popup), "switcher-css", css, +[](gpointer data) {
		gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(data)); g_object_unref(data);
	});
	g_signal_connect(popup, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
		switch (event->keyval)
		{
		case GDK_KEY_Tab: case GDK_KEY_ISO_Left_Tab: select(selected + ((event->keyval == GDK_KEY_ISO_Left_Tab || (event->state & GDK_SHIFT_MASK)) ? -1 : 1)); break;
		case GDK_KEY_Left: select(selected - 1); break;
		case GDK_KEY_Right: select(selected + 1); break;
		case GDK_KEY_Up: select(selected - columns); break;
		case GDK_KEY_Down: select(selected + columns); break;
		case GDK_KEY_Return: case GDK_KEY_KP_Enter: close(true, event->time); break;
		case GDK_KEY_Escape: close(false); break;
		case GDK_KEY_q: case GDK_KEY_Q:
			if (!quitKeyDown && !entries.empty())
			{
				quitKeyDown = true;
				auto group = entries[selected].group.lock();
				// Use the existing graceful close action. Applications may ask to save
				// or refuse to close; only window-closed removes them from the view.
				if (group) group->closeAll();
			}
			break;
		default: break;
		}
		return true;
	}), nullptr);
	g_signal_connect(popup, "key-release-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
		if (event->keyval == GDK_KEY_q || event->keyval == GDK_KEY_Q) quitKeyDown = false;
		if ((event->keyval == GDK_KEY_Alt_L || event->keyval == GDK_KEY_Alt_R) && !altPressed()) close(true, event->time);
		return true;
	}), nullptr);
	g_signal_connect(popup, "grab-broken-event", G_CALLBACK(+[](GtkWidget*, GdkEventGrabBroken*, gpointer) -> gboolean {
		close(false); return true;
	}), nullptr);
	g_signal_connect(grid, "size-allocate", G_CALLBACK(+[](GtkWidget*, GtkAllocation*, gpointer) { revealSelection(); }), nullptr);
	rebuild(true, reverse);
	if (!popup) return;
	// Lay out and position the complete contents before mapping the X window.
	// A new override-redirect popup otherwise exposes an unpainted surface
	// until GTK's first frame-clock paint, briefly revealing the desktop.
	gtk_widget_show_all(outer);
	centre();
	gtk_widget_realize(popup);
	GdkWindow* window = gtk_widget_get_window(popup);
	GdkRGBA background;
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_style_context_get_background_color(gtk_widget_get_style_context(popup), GTK_STATE_FLAG_NORMAL, &background);
	background.alpha = 1.0;
	gdk_window_set_background_rgba(window, &background);
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_widget_show(popup);
	// Finish the initial paint now, rather than waiting for the next frame.
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gdk_window_process_updates(window, true);
	G_GNUC_END_IGNORE_DEPRECATIONS
	GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
	if (gdk_seat_grab(seat, gtk_widget_get_window(popup), GDK_SEAT_CAPABILITY_KEYBOARD,
		false, nullptr, nullptr, nullptr, nullptr) != GDK_GRAB_SUCCESS)
	{
		close(false); return;
	}
	grabbedSeat = seat;
	// Quick Alt+Tab may release Alt before the popup acquires the keyboard.
	releaseSource = g_timeout_add(30, +[](gpointer) -> gboolean {
		if (!popup) { releaseSource = 0; return G_SOURCE_REMOVE; }
		if (!altPressed()) { releaseSource = 0; close(true); return G_SOURCE_REMOVE; }
		return G_SOURCE_CONTINUE;
	}, nullptr);
}

void report(const std::string& message)
{
	GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message.c_str());
	g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* d, gint, gpointer) { gtk_widget_destroy(GTK_WIDGET(d)); }), nullptr);
	gtk_widget_show(dialog);
}
}

bool supported()
{
#ifdef ENABLE_X11
	return GDK_IS_X11_DISPLAY(gdk_display_get_default());
#else
	return false;
#endif
}

bool setEnabled(bool value)
{
	if (value == enabled) return true;
	std::string error;
	if (value)
	{
		if (!supported() || !ready) { report(_("The app switcher requires an X11 Xfce session and its shortcut service.")); return false; }
		if (!SwitcherShortcuts::enable(error)) { report(error); return false; }
		enabled = true;
	}
	else
	{
		close(false);
		if (!SwitcherShortcuts::restore(error)) { report(error); return false; }
		enabled = false;
	}
	return true;
}

void cycle(bool reverse)
{
	if (!enabled) return;
	if (popup) select(selected + (reverse ? -1 : 1));
	else show(reverse);
}

void refreshBadges()
{
	for (const auto& entry : entries)
	{
		auto group = entry.group.lock();
		const bool visible = group && group->mLauncherCountVisible;
		if (visible)
		{
			const std::string text = group->mLauncherCount > 99 ? "99+" : std::to_string(group->mLauncherCount);
			gtk_label_set_text(GTK_LABEL(entry.badge), text.c_str());
		}
		gtk_widget_set_visible(entry.badge, visible);
	}
}

void cycleWindows(bool reverse, unsigned int timestamp)
{
	if (!enabled || popup) return;
	auto active = Xfw::mGroupWindows.get(Xfw::getActiveWindow());
	if (!active || !active->mGroup) return;
	std::vector<GroupWindow*> windows;
	// The dock's group list is filtered by workspace/monitor. Use the full
	// window registry so changing workspace cannot remove the next candidate.
	// Its insertion order also stays stable while filtered groups are rebuilt.
	Xfw::mGroupWindows.forEach([&](std::pair<XfwWindow* const, std::shared_ptr<GroupWindow>> item) {
		GroupWindow* window = item.second.get();
		if (window && window->mGroup == active->mGroup
			&& !(xfw_window_get_state(window->mXfwWindow) & XFW_WINDOW_STATE_SKIP_TASKLIST))
			windows.push_back(window);
	});
	if (windows.size() < 2) return;
	auto current = std::find(windows.begin(), windows.end(), active.get());
	if (current == windows.end()) return;
	const int index = std::distance(windows.begin(), current);
	const int count = windows.size();
	windows[(index + (reverse ? count - 1 : 1)) % count]->activate(timestamp);
}

void init()
{
	ready = SwitcherShortcuts::init();
	noteActive();
	g_signal_connect(Xfw::mXfwScreen, "active-window-changed", G_CALLBACK(+[](XfwScreen*, XfwWindow*, gpointer) { noteActive(); }), &signalTag);
	g_signal_connect(Xfw::mXfwScreen, "window-closed", G_CALLBACK(+[](XfwScreen*, XfwWindow*, gpointer) {
		if (popup && !refreshSource) refreshSource = g_idle_add(+[](gpointer) -> gboolean {
			refreshSource = 0; if (popup) rebuild(false); return G_SOURCE_REMOVE;
		}, nullptr);
	}), &signalTag);
	if (Settings::altTabSwitcher && !setEnabled(true)) Settings::altTabSwitcher.set(false);
}

void finalize()
{
	close(false);
	g_signal_handlers_disconnect_by_data(Xfw::mXfwScreen, &signalTag);
	SwitcherShortcuts::finalize();
	enabled = ready = false; recent.clear(); serial = 0;
}
} // namespace Switcher
