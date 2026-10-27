# Docklike app switcher

Enable **Use Docklike for Alt+Tab** in the test plugin's Keyboard hotkeys preferences. Off by default; requires X11 with xfwm4 and xfsettingsd running.

The centered overlay shows running, visible app groups from this dock in most-recently-used order. Dock workspace/monitor filters apply. Pinned launchers without windows are excluded. Each tile activates its app group, using the dock's existing window activation behavior.

- Alt+Tab: open and advance; hold Alt and press Tab again to continue.
- Alt+Shift+Tab: reverse.
- Release Alt or press Enter: activate selection.
- Escape: cancel.
- Arrow keys or clicking a tile also navigate/activate.

Existing matching Xfce shortcuts are backed up beside the plugin rc file with an `.alt-tab-backup` suffix before takeover. Disabling the preference, removing the plugin, or shutting down the panel restores them, preserving shortcut edits made while enabled. After a crash, loading this plugin recovers the backup before enabling again. Use this feature in only one dock instance.

The popup uses `#docklike-app-switcher`, `#docklike-switcher-card`, and `.selected` CSS selectors, which can be overridden in gtk.css.

Validation: built against GTK3 and libxfce4windowing 4.20; exercised in an isolated X11/Xfce 4.18 desktop with three independent GTK apps. Forward, reverse, repeated Tab, quick taps, Escape, and functional native shortcut restoration passed.

Optional list view: enable **Show Alt+Tab apps as a list** in Keyboard hotkeys. This uses a scrollable single column of compact icon/name rows; the grid remains the default.

Press **Q** on a highlighted app in either view to request normal closure of its taskbar windows. Unsaved-document prompts remain under the app's control; background/tray processes may remain running. Release Alt or cancel the switcher to interact with such prompts. Closed apps disappear, with the next remaining row selected. The switcher closes when no running apps remain.

Alt+§ cycles the active app's windows immediately; Alt+Shift+§ reverses. These shortcuts are enabled with Use Docklike for Alt+Tab. The current keyboard layout must contain § (as on Swedish keyboards). Existing Xfce bindings on these keys are backed up and restored with the other switcher keys.

Notification badges show app-provided unread counts on dock icons and on both Alt+Tab layouts. Hide notification badges disables both. Counts use Unity LauncherEntry D-Bus updates; ordinary desktop notification popups do not provide a reliable unread count and are not counted. Zero/hidden counts disappear, large counts display 99+, and counts clear when the publisher disconnects.
