// SPDX-License-Identifier: GPL-3.0-or-later
#include "SwitcherShortcuts.hpp"
#include "Plugin.hpp"
#include "Switcher.hpp"
#include "Settings.hpp"
#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#endif
#include <xfconf/xfconf.h>
#include <glib/gstdio.h>
#include <map>
#include <set>
#include <vector>

namespace SwitcherShortcuts
{
namespace
{
XfconfChannel* channel = nullptr;
std::string backupPath;
struct Entry { std::string path, before, after; bool existed, install; };
std::vector<Entry> backup;
guint grabSource = 0;
#ifdef ENABLE_X11
GdkWindow* rootWindow = nullptr;
int tabCode = 0;
int windowCode = 0;
std::vector<unsigned int> modifiers;
bool grabbed = false;

GdkFilterReturn keyFilter(GdkXEvent* raw, GdkEvent*, gpointer)
{
	auto* event = static_cast<XEvent*>(raw);
	if (grabbed && event->type == KeyPress && (event->xkey.keycode == static_cast<unsigned>(tabCode)
		|| (windowCode && event->xkey.keycode == static_cast<unsigned>(windowCode)))
		&& (event->xkey.state & Mod1Mask))
	{
		if (event->xkey.keycode == static_cast<unsigned>(windowCode))
			Switcher::cycleWindows((event->xkey.state & ShiftMask) != 0, event->xkey.time);
		else Switcher::cycle((event->xkey.state & ShiftMask) != 0);
		return GDK_FILTER_REMOVE;
	}
	return GDK_FILTER_CONTINUE;
}

void ungrab()
{
	if (!rootWindow) return;
	GdkDisplay* display = gdk_window_get_display(rootWindow);
	gdk_x11_display_error_trap_push(display);
	for (int code : {tabCode, windowCode})
		if (code) for (auto mods : modifiers) XUngrabKey(GDK_DISPLAY_XDISPLAY(display), code, mods, GDK_WINDOW_XID(rootWindow));
	gdk_x11_display_error_trap_pop_ignored(display);
	grabbed = false;
}

bool tryGrab()
{
	if (!rootWindow) return false;
	GdkDisplay* display = gdk_window_get_display(rootWindow);
	gdk_x11_display_error_trap_push(display);
	for (int code : {tabCode, windowCode})
		if (code) for (auto mods : modifiers)
		XGrabKey(GDK_DISPLAY_XDISPLAY(display), code, mods, GDK_WINDOW_XID(rootWindow),
			false, GrabModeAsync, GrabModeAsync);
	if (gdk_x11_display_error_trap_pop(display)) { ungrab(); return false; }
	grabbed = true;
	return true;
}
#else
void ungrab() {}
bool tryGrab() { return false; }
#endif

bool currentMatches(const Entry& e)
{
	if (!e.install) return !xfconf_channel_has_property(channel, e.path.c_str());
	gchar* current = xfconf_channel_get_string(channel, e.path.c_str(), nullptr);
	bool matches = current && e.after == current;
	g_free(current);
	return matches;
}

bool persist(std::string& error)
{
	GKeyFile* file = g_key_file_new();
	int i = 0;
	for (const auto& e : backup)
	{
		std::string group = "shortcut-" + std::to_string(i++);
		g_key_file_set_string(file, group.c_str(), "path", e.path.c_str());
		g_key_file_set_boolean(file, group.c_str(), "existed", e.existed);
		g_key_file_set_string(file, group.c_str(), "before", e.before.c_str());
		g_key_file_set_boolean(file, group.c_str(), "install", e.install);
		g_key_file_set_string(file, group.c_str(), "after", e.after.c_str());
	}
	GError* failure = nullptr;
	bool ok = g_key_file_save_to_file(file, backupPath.c_str(), &failure);
	if (!ok) { error = failure->message; g_error_free(failure); }
	g_key_file_unref(file);
	return ok;
}

bool load(std::string& error)
{
	if (!g_file_test(backupPath.c_str(), G_FILE_TEST_EXISTS)) return true;
	GKeyFile* file = g_key_file_new();
	GError* failure = nullptr;
	if (!g_key_file_load_from_file(file, backupPath.c_str(), G_KEY_FILE_NONE, &failure))
	{
		error = failure->message; g_error_free(failure); g_key_file_unref(file); return false;
	}
	gsize count = 0;
	gchar** groups = g_key_file_get_groups(file, &count);
	for (gsize i = 0; i < count; ++i)
	{
		gchar* path = g_key_file_get_string(file, groups[i], "path", nullptr);
		gchar* before = g_key_file_get_string(file, groups[i], "before", nullptr);
		gchar* after = g_key_file_get_string(file, groups[i], "after", nullptr);
		if (path && before && after && (g_str_has_prefix(path, "/xfwm4/custom/") || g_str_has_prefix(path, "/commands/custom/")))
			backup.push_back({path, before, after,
				!!g_key_file_get_boolean(file, groups[i], "existed", nullptr),
				!!g_key_file_get_boolean(file, groups[i], "install", nullptr)});
		else error = "Invalid Alt+Tab shortcut backup; it has been kept for recovery.";
		g_free(path); g_free(before); g_free(after);
	}
	g_strfreev(groups); g_key_file_unref(file);
	return error.empty();
}

bool isAltTab(const char* accelerator)
{
	guint key = 0; GdkModifierType mods;
	gtk_accelerator_parse(accelerator, &key, &mods);
	const guint relevant = mods & gtk_accelerator_get_default_mod_mask();
	bool sameAppKey = false;
#ifdef ENABLE_X11
	if (windowCode) sameAppKey = XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), key) == windowCode;
#endif
	return (key == GDK_KEY_Tab || key == GDK_KEY_ISO_Left_Tab || sameAppKey)
		&& (relevant == GDK_MOD1_MASK || relevant == (GDK_MOD1_MASK | GDK_SHIFT_MASK));
}
}

bool init()
{
	GError* error = nullptr;
	if (!xfconf_init(&error)) { if(error) g_error_free(error); return false; }
	channel = xfconf_channel_get("xfce4-keyboard-shortcuts");
	gchar* path = xfce_panel_plugin_save_location(Plugin::mXfPlugin, true);
	if (!path) return false;
	backupPath = std::string(path) + ".alt-tab-backup";
	g_free(path);
	std::string message;
	if (!load(message)) { backup.clear(); g_warning("%s", message.c_str()); return false; }
	// Recover our own bindings left by an interrupted previous panel process.
	if (!restore(message)) return false;
#ifdef ENABLE_X11
	if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
	{
		rootWindow = gdk_get_default_root_window();
		Display* display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
		tabCode = XKeysymToKeycode(display, GDK_KEY_Tab);
		windowCode = XKeysymToKeycode(display, GDK_KEY_section);
		const unsigned numLock = XkbKeysymToModifiers(display, GDK_KEY_Num_Lock);
		std::set<unsigned> unique;
		for (unsigned shift : {0u, static_cast<unsigned>(ShiftMask)})
			for (unsigned locks : {0u, static_cast<unsigned>(LockMask), numLock, static_cast<unsigned>(LockMask) | numLock})
				unique.insert(Mod1Mask | shift | locks);
		modifiers.assign(unique.begin(), unique.end());
		gdk_window_add_filter(rootWindow, keyFilter, nullptr);
	}
#endif
	return true;
}

bool restore(std::string& error)
{
	if (grabSource) { g_source_remove(grabSource); grabSource = 0; }
	ungrab(); // XSync in the error trap completes this before the WM re-grabs.
	if (backup.empty()) return true;
	if (!channel) { error = "The Xfce shortcut service is unavailable."; return false; }
	std::vector<Entry> remaining;
	// Remove our command grabs first, then restore the window-manager bindings.
	for (auto it = backup.rbegin(); it != backup.rend(); ++it)
	{
		const auto& e = *it;
		if (!currentMatches(e)) continue; // Respect subsequent user changes.
		if (e.install) xfconf_channel_reset_property(channel, e.path.c_str(), false);
		if (e.existed && !xfconf_channel_set_string(channel, e.path.c_str(), e.before.c_str()))
		{
			Entry retry = e; retry.install = false; remaining.push_back(retry);
			error = "Could not restore an Alt+Tab binding. The backup was retained.";
		}
	}
	backup = remaining;
	if (backup.empty()) { g_unlink(backupPath.c_str()); return true; }
	std::string saveError; persist(saveError);
	return false;
}

bool enable(std::string& error)
{
	if (!channel) { error = "The Xfce shortcut service is unavailable."; return false; }
	if (!backup.empty()) { error = "An Alt+Tab backup is still pending restoration."; return false; }
	// Providers normally create custom bindings as soon as the Xfce session starts.
	if (!xfconf_channel_get_bool(channel, "/xfwm4/custom/override", false)
		|| !xfconf_channel_get_bool(channel, "/commands/custom/override", false))
	{
		error = "Start the Xfce window manager and settings daemon before enabling this shortcut.";
		return false;
	}
	std::map<std::string, std::string> changes;
	GHashTable* props = xfconf_channel_get_properties(channel, "/");
	if (props)
	{
		GHashTableIter iter; gpointer key, value;
		g_hash_table_iter_init(&iter, props);
		while (g_hash_table_iter_next(&iter, &key, &value))
		{
			const char* path = static_cast<char*>(key);
			const char* prefix = g_str_has_prefix(path, "/xfwm4/custom/") ? "/xfwm4/custom/"
				: g_str_has_prefix(path, "/commands/custom/") ? "/commands/custom/" : nullptr;
			if (prefix && G_VALUE_HOLDS_STRING(static_cast<GValue*>(value)) && isAltTab(path + strlen(prefix)))
				changes[path] = "";
		}
		g_hash_table_destroy(props);
	}
	for (const auto& change : changes)
	{
		gchar* old = xfconf_channel_get_string(channel, change.first.c_str(), nullptr);
		backup.push_back({change.first, old ? old : "", "", old != nullptr, false});
		g_free(old);
	}
	if (!persist(error)) { backup.clear(); return false; } // No bindings changed yet.
	for (const auto& e : backup)
	{
		xfconf_channel_reset_property(channel, e.path.c_str(), false);
		if (xfconf_channel_has_property(channel, e.path.c_str()))
		{
			error = "An existing Alt+Tab shortcut is locked.";
			std::string ignored; restore(ignored); return false;
		}
	}
	// Xfwm/xfsettingsd process their removals asynchronously. Wait for their
	// grabs to go away instead of racing another command-shortcut provider.
	auto* attempts = new unsigned(0);
	grabSource = g_timeout_add_full(G_PRIORITY_DEFAULT, 25, +[](gpointer data) -> gboolean {
		if (tryGrab()) { grabSource = 0; return G_SOURCE_REMOVE; }
		if (++*static_cast<unsigned*>(data) < 80) return G_SOURCE_CONTINUE;
		grabSource = 0;
		Settings::altTabSwitcher.set(false);
		GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", "Could not take over Alt+Tab. The previous shortcuts have been restored.");
		g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* d, gint, gpointer) { gtk_widget_destroy(GTK_WIDGET(d)); }), nullptr);
		gtk_widget_show(dialog);
		return G_SOURCE_REMOVE;
	}, attempts, +[](gpointer data) { delete static_cast<unsigned*>(data); });
	return true;
}

void finalize()
{
	std::string error;
	if (!restore(error)) g_warning("%s", error.c_str());
#ifdef ENABLE_X11
	if (rootWindow) gdk_window_remove_filter(rootWindow, keyFilter, nullptr);
	rootWindow = nullptr; modifiers.clear();
#endif
	if (channel) { channel = nullptr; xfconf_shutdown(); }
	backup.clear();
}
} // namespace SwitcherShortcuts
