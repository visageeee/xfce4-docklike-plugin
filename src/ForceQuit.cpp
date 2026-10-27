// SPDX-License-Identifier: GPL-3.0-or-later
#include "ForceQuit.hpp"
#include "ProcessStop.hpp"
#include <gtk/gtk.h>
#include <set>
#include <string>
#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <X11/Xutil.h>
#include <libxfce4windowing/xfw-x11.h>
#endif

namespace ForceQuit
{
void windows(const std::vector<XfwWindow*>& windows)
{
	std::set<int> handled;
	auto* pending = new std::vector<int>();
	bool failed = false;
	for (XfwWindow* window : windows)
	{
		auto* app = xfw_window_get_application(window);
		auto* instance = app ? xfw_application_get_instance(app, window) : nullptr;
		const int pid = instance ? xfw_application_instance_get_pid(instance) : 0;
		// Never terminate the panel itself, PID 1, or a process group.
		if (pid == getpid() || pid == 1) { failed = true; continue; }
		bool local = false;
#ifdef ENABLE_X11
		GdkDisplay* display = gdk_display_get_default();
		const bool x11 = GDK_IS_X11_DISPLAY(display);
		Window xid = x11 ? xfw_window_x11_get_xid(window) : None;
		if (x11 && xid != None)
		{
			XTextProperty machine = {};
			gdk_x11_display_error_trap_push(display);
			const bool found = XGetWMClientMachine(GDK_DISPLAY_XDISPLAY(display), xid, &machine);
			const int error = gdk_x11_display_error_trap_pop(display);
			if (found && !error && machine.value && machine.format == 8)
			{
				const std::string host(reinterpret_cast<char*>(machine.value), machine.nitems);
				local = host == g_get_host_name();
			}
			if (machine.value) XFree(machine.value);
		}
#endif
		if (local && pid > 1)
		{
			if (!handled.insert(pid).second) continue;
			const int fd = ProcessStop::open(pid);
			if (fd >= 0)
			{
				if (ProcessStop::signal(fd, SIGTERM))
				{
					pending->push_back(fd);
					continue;
				}
				const int error = errno;
				::close(fd);
				if (error == ESRCH) continue;
			}
			else if (errno == ESRCH) continue;
		}
#ifdef ENABLE_X11
		// Unknown/remote PIDs must not be interpreted as local PIDs. This is
		// the same targeted X11-client disconnection used by xkill.
		if (x11 && xid != None)
		{
			gdk_x11_display_error_trap_push(display);
			XKillClient(GDK_DISPLAY_XDISPLAY(display), xid);
			gdk_x11_display_error_trap_pop_ignored(display);
			continue;
		}
#endif
		failed = true;
	}
	if (pending->empty()) delete pending;
	else g_timeout_add_full(G_PRIORITY_DEFAULT, 2000,
		+[](gpointer data) -> gboolean {
			for (int fd : *static_cast<std::vector<int>*>(data))
			{
				// ESRCH means the process already exited. A reused PID cannot
				// be affected because we signal through the retained handle.
				if (!ProcessStop::signal(fd, SIGKILL) && errno != ESRCH)
					g_warning("Force Quit: could not stop process: %s", g_strerror(errno));
			}
			return G_SOURCE_REMOVE;
		}, pending, +[](gpointer data) {
			auto* fds = static_cast<std::vector<int>*>(data);
			for (int fd : *fds) ::close(fd);
			delete fds;
		});
	if (failed)
	{
		GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", g_dgettext(GETTEXT_PACKAGE,
			"Could not force quit one or more application instances."));
		g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* d, gint, gpointer) {
			gtk_widget_destroy(GTK_WIDGET(d));
		}), nullptr);
		gtk_widget_show(dialog);
	}
}
} // namespace ForceQuit
