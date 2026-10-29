# Changelog

## 1.0.3

- Follows the program language (ten languages). Affliction names are the game's own names in each language.
- Hotkey names show correctly with a non-English keyboard layout.
- Long floor, timer and map label texts are no longer cut off.
- The affliction search finds an affliction by its shown name or by its English name.
- Weight lists sort by the shown name, and the history table columns fit their headers.
- In the Overlays and Profiles settings a control that does not fit on its line moves to the next line instead of being cut off at the window edge.
- The shipped profiles "Default" and "No-Hit" show their translated names until you change them.
- The bronze, silver and gold caches in Traditional Chinese and the Grand Spectrum in Russian, Portuguese, Spanish and Korean use the game's own names.

## 1.0.2

- Works with the separate menu window (POEFixer 366): the host hands the plugin the right drawing context for its settings tab and for its overlay.
- The settings tab also sets its drawing context itself, so it draws in the right window with the menu in a separate window.
- Its windows (the dashboard and the timers) now have the POEFixer gear menu (POEFixer 366): lock them, take them out of the game onto any screen, reset their position or close them; their places are remembered separately in and out of the game.
- Closing the dashboard or the timers (title-bar X or gear menu) turns "Show dashboard" / "Timer overlay" off and saves it at once.

## 1.0.1

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.0.0

- First versioned release.
