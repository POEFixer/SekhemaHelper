# SekhemaHelper — POE2Fixer Plugin

A Trial of the Sekhemas helper for Path of Exile 2, built on the POE2Fixer
v6 Plugin SDK. It scores every revealed room, recommends the optimal route
to the boss, shows your live resources, and overlays the best path + hazard
objects directly on the in-game map.

## What It Does

A modern, card-based dashboard plus on-map overlays:

- **Best-route scoring** — scores each room from its type, the affliction it
  imposes, and its reward, then runs a longest-weighted-path search to the
  boss. The recommended door is highlighted; alternatives are ranked by their
  path value.
- **Build-aware afflictions** — stat-removal curses are weighted by your live
  Armour / Evasion / Energy Shield / Life, so "no Armour" reads severe for an
  armour build and mild for an evasion build. Risk is colour-coded.
- **Two editable weight profiles** (Default / No-Hit) with grouped, searchable
  sliders and soft resource suppression (avoid Merchant when low on Sacred
  Water, avoid Honour shrines when already high).
- **Live resources** — Honour %, Sacred Water and Bronze/Silver/Gold keys.
- **Map overlay** — best-path frames + risk dots on the trial floor map; and on
  the large map / minimap: a walkable A* crystal-collection route (numbered),
  every reward-cache type (per-type colours, tier-coloured labels with
  Superior "+" / Prime "++" marks, white highlight ring drawn on top), and
  ritual portals / sanctum levers. Markers project with correct terrain
  height, so raised platforms line up with the map.
- Configurable show/hide hotkey, themed to the host's active UI theme.

## Building

Open `SekhemaHelper.sln` in Visual Studio 2022. Build Release|x64. Output:

    bin/Release/SekhemaHelper.dll

Or from the command line:
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SekhemaHelper.sln -p:Configuration=Release -p:Platform=x64
```

## Installing

Copy the built DLL into POE2Fixer's plugin directory:

    POE2Fixer/Plugins/SekhemaHelper/SekhemaHelper.dll

Restart POE2Fixer and enable **SekhemaHelper** in the Plugins tab. Open a
Trial of the Sekhemas to use it.

## Settings

In the host Plugins settings tab:

- **Display** — best-path drawing, frame thickness/colour, dashboard toggle,
  and the rebindable show/hide hotkey (default F6).
- **Profiles** — pick/edit the weight profile (afflictions / room types /
  rewards) + suppression thresholds.
- **Overlays** — portal/lever/crystal toggles + colours, marker sizes, the
  "Room radius" (all Sekhema floors share one map, so this keeps markers/route
  to the room you're in), and a compact per-cache-type table covering every
  Sekhema chest (Spectrum, Time-Lost, Relics, Jewels, Currency, Maps, armour
  and weapon caches, pots/urns, ...): show, circle colour, and Ring — every
  chest of a Ring-checked type gets a white ring drawn on top.

## SDK Version

Targets **v6** of the Plugin SDK. The bundled `sdk/` headers must match the
host POE2Fixer's expected SDK version (a run-time check refuses mismatched
plugins with a clear log entry).

## Source

The same source ships inside the main POE2Fixer repo at
`Plugins/SekhemaHelper/`. This standalone repo is for plugin authors who want
to read or fork it.

## Project Structure

```
sdk/                Plugin SDK headers (PluginAbi.h, PluginSDK.h)
imgui/              ImGui library (headers + sources, compiled into the DLL)
lib/nlohmann/       Vendored nlohmann/json (settings persistence)
*.h / *.cpp         Plugin sources (dashboard, engine, readers, overlays)
SekhemaHelper.cpp   Plugin entry point + lifecycle
```
