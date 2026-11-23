# Docklike Taskbar for Xfce

Docklike Taskbar groups an application's windows under one panel icon. Pin an app to keep its launcher in the same place after you close its windows.

This fork adds icons that shrink to fit the panel, an app-based Alt+Tab switcher, unread badges, and file jump lists. It appears in the panel as **Docklike Taskbar (Test)** and uses its own plugin ID, `docklike-test`, so you can install it alongside the [upstream Docklike Taskbar](https://gitlab.xfce.org/panel-plugins/xfce4-docklike-plugin).

## What’s different in this fork?

- **Icons adapt to the available space.** Open more apps and the dock shrinks its icons to leave room for other panel items. Close apps and the icons grow back to their preferred size. This works on horizontal and vertical panels. [Layout details](OVERFLOW.md)
- **Alt+Tab can switch by app.** On X11 with xfwm4 and xfsettingsd, enable **Use Docklike for Alt+Tab** in **Keyboard hotkeys** on one dock instance. Hold Alt and press Tab to cycle through running apps in recent-use order. Release Alt to activate the selection. On layouts with a § key, Alt+§ cycles through the active app’s windows. Disabling the switcher restores the previous shortcuts. [Switcher controls and shortcut handling](APP_SWITCHER.md)
- **Unread counts appear on app icons.** Apps that publish counts through the Unity LauncherEntry protocol can show badges in both the dock and the switcher. Use **Hide notification badges** to turn them off.
- **Recent and pinned files are one right-click away.** Right-click a launcher that supports opening files to see its recent files or choose **Pin a file…**. Selecting a file opens it with that app. You can also drag a file onto its dock icon. [File jump lists](JUMP_LISTS.md)
- **Hover behavior is configurable.** Adjust icon brightness on hover and the delay before window lists or previews appear.

For the standard taskbar controls and screenshots, see the [Xfce Docklike Taskbar guide](https://docs.xfce.org/panel-plugins/xfce4-docklike-plugin/start).

## Build and install

Install a C and C++ toolchain, Meson, Ninja, pkg-config, gettext, and the development packages for GLib/GIO Unix 2.66+, GTK 3.24+, Cairo 1.16+, Xfce Panel, libxfce4ui, libxfce4util and Xfconf 4.18+, and libxfce4windowing and its UI library 4.19.4+.

Meson detects the available display backends. X11 support requires GDK X11, libX11, libXi, and the X11 library for libxfce4windowing. Wayland support requires GDK Wayland and gtk-layer-shell 0.7+. At least one backend must be available.

Build from this fork using the same install prefix and library directory as your panel so Xfce can find the plugin.

```sh
git clone https://github.com/visageeee/xfce4-docklike-plugin.git
cd xfce4-docklike-plugin
meson setup build \
  --prefix="$(pkg-config --variable=prefix libxfce4panel-2.0)" \
  --libdir="$(pkg-config --variable=libdir libxfce4panel-2.0)"
meson compile -C build
sudo meson install -C build
xfce4-panel -r
```

Open **Panel Preferences → Items → +**, select **Docklike Taskbar (Test)**, and click **Add**.

## Uninstall

Remove **Docklike Taskbar (Test)** from the panel. From the same source directory, run the following commands.

```sh
sudo ninja -C build uninstall
xfce4-panel -r
```

## Report a problem

Report fork-specific problems in [GitHub Issues](https://github.com/visageeee/xfce4-docklike-plugin/issues). Include your Xfce Panel version, session type (X11 or Wayland), and the steps that reproduce the problem.

To capture debug output, quit the panel and relaunch it from a terminal.

```sh
xfce4-panel -q
G_MESSAGES_PREFIXED= G_MESSAGES_DEBUG=xfce4-docklike-test-plugin xfce4-panel
```

Reproduce the problem and copy the relevant output. Press Ctrl+C, then run `xfce4-panel &` to start the panel again. [Xfce panel debugging guide](https://docs.xfce.org/xfce/xfce4-panel/debugging)
