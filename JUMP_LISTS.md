# File jump lists

Right-click an app icon to open its recent or pinned files. Pin the app to the dock to keep its menu available while it is closed. Selecting a file launches the chosen app with that file, rather than using the default file association.

Recent files come from the desktop's GTK/XBEL recent-file history. The menu shows up to ten existing local files associated with that app, newest first, excluding private entries and files already pinned. The desktop's recent-file-history preference is respected. Apps that do not publish history, including many browsers and non-GTK apps, may have no automatic recent list.

Use **Pin a file…** to select a file manually, even when the recent list is empty. Pins persist across panel restarts. **Unpin file** removes a pin without deleting its file. Pinned files remain listed if temporarily unavailable, so an offline mount does not erase a pin.

File lists are available for launchers whose desktop entry declares file or URI support. Existing desktop actions remain in the menu. Menu matching uses recorded app names/IDs or executable identity; it does not infer past usage from MIME compatibility or execute commands stored in the history.

Validation: app-specific and private-history filtering, missing files, history preference, opening a file from a closed pinned app, filenames containing spaces/apostrophes, pin persistence, duplicate suppression and unpinning.
