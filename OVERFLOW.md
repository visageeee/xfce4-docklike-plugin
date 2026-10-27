# Shrink icons when the panel fills up

Based on upstream master df9467b2b52604e20568f7b6436b6e02205ec081.

Docklike now opts into Xfce's shrinking-plugin allocation. It keeps a separate
preferred icon size and fits icons, padding and spacing to the space the panel
provides. Closing applications or freeing panel space restores the preferred
size. Horizontal and vertical panels are supported. Hidden application groups
do not consume space. The hidden dockSize preference becomes a preferred length,
so it cannot impose an unshrinkable minimum.

Button padding is normalized to 4 logical pixels at full size and scales down
with icons; button theme minimum sizes and borders are removed to permit fitting.
The minimum is one logical pixel per visible application. If other panel items
alone fill the screen, shrinking Docklike cannot make those other items fit.

## Build and install

Apply the patch from the repository root with `git apply --check PATCH` followed
by `git apply PATCH`. This patch targets current upstream, not older 0.4 releases.

Upstream requires GTK 3.24+, Xfce panel/UI/util/xfconf 4.18+, and
libxfce4windowing (including its UI and X11 development libraries) 4.19.4+.
Stock Ubuntu 24.04 may need libxfce4windowing installed separately. These are
upstream requirements; this patch adds no library dependencies.

For an X11 installation, with those development dependencies installed:

```sh
meson setup build-overflow --prefix=/usr \
  --libdir="$(pkg-config --variable=libdir libxfce4panel-2.0)" \
  -Dwayland=disabled
meson compile -C build-overflow
sudo meson install -C build-overflow
xfce4-panel -r
```

This build installs as **Docklike Taskbar (Test)** with module ID `docklike-test`
and library `libdocklike-test.so`. Add it through Panel Preferences > Items > +.
Xfce assigns its numeric instance ID automatically. Settings, shortcut commands,
translations, CSS configuration and embedded UI resources use a separate identity.
The original Docklike Taskbar remains installed and can be used alongside it.

Remove the test item from the panel before uninstalling with:

```sh
sudo ninja uninstall -C build-overflow
xfce4-panel -r
```

## Validation

Built successfully with GTK 3.24.41, Xfce panel 4.18.4 and libxfce4windowing 4.20.0.

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/dock-layout.cpp -o /tmp/dock-layout-test
/tmp/dock-layout-test
DOCK_BUILD_DIR=build-overflow python3 tests/build-gtk-test.py
xvfb-run -a ./build-overflow/dock-gtk-test
```

The GTK test links the actual plugin objects and checks allocated button bounds,
restoration of the preferred icon size, stable natural requests, hidden groups,
and both panel orientations. An isolated Xvfb session may report missing Xfconf,
D-Bus and window-manager services; these are not tested by this harness.

Still needs a live-panel check: open enough distinct applications to fill the
panel, confirm the clock/tray remain visible, close applications, and confirm
icons grow back. Also check dragging, previews, your theme, and display scaling.
