# Weather Blocker

SKSE plugin (Skyrim SE/AE) that stops chosen weathers from being used, optionally only during a time window
(e.g. block `SkyrimOvercastRain` from 18:00 to 05:00). Useful when an ENB preset and a weather mod do not
get along and one specific weather looks broken.

Configured in game through [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352).

## Requirements

- Skyrim SE / AE (the code uses CommonLibSSE-NG, so it targets SE, AE and VR)
- SKSE
- Address Library for SKSE Plugins
- SKSE Menu Framework (for the in-game menu; without it the plugin still reads the JSON file)

## How it works (v0.1)

1. After the game's data is loaded the plugin lists every `TESWeather` in the load order.
2. A small timer checks the current weather every ~100 ms (configurable). If the current weather matches an
   active rule (always, or inside one of its time windows), it switches to a replacement weather.
3. Replacement order: the weather chosen in the menu (if "Always use this weather" is set) -> the previous
   weather -> the last allowed weather seen -> the sky's default weather.

The check only runs outdoors (full sky) and, by default, leaves alone weathers forced by quests/scripts.

## Using the menu

Open the SKSE Menu Framework (its hotkey), then **Weather Blocker > Settings**.

- **Add a weather rule**: filter the list, click a weather, press *Block selected weather*
  (tick *Only during a time window* first for a timed rule).
- **Rules**: the checkbox on the left enables/disables the rule without deleting it (= allow that weather again).
  Each rule can be "all day" or have one or more time windows. A window like 18:00 -> 05:00 crosses midnight.
- **Replacement weather**: automatic, or pick a fixed weather.
- **Save all** writes `Data/SKSE/Plugins/WeatherBlocker.json` and applies it immediately.
  Nothing is applied until you press it.

Rules are stored by plugin name + local FormID, so they survive load-order changes. If a plugin is removed, its
rules stay in the list marked *not in current load order*.

## Config file

`Data/SKSE/Plugins/WeatherBlocker.json` (comments allowed):

```json
{
  "enabled": true,
  "respectOverrides": true,
  "pollMs": 100,
  "fallback": { "mode": 0, "plugin": "", "formID": "0x000000", "label": "" },
  "rules": [
    {
      "plugin": "Skyrim.esm",
      "formID": "0x0010A234",
      "label": "SkyrimOvercastRain  [Skyrim.esm | 10A234]",
      "enabled": true,
      "allDay": false,
      "windows": [ { "start": "18:00", "end": "05:00" } ]
    }
  ]
}
```

## Testing tips

- Log: `Documents/My Games/Skyrim Special Edition/SKSE/WeatherBlocker.log`
  (look for "Scanned N weathers", "Menu registered", and "Blocked ... -> switching to ...").
- Force a weather from the console with `sw <weather ID>` (smooth) while testing. If you use `fw` and nothing
  happens, untick *Respect weather forced by quests / scripts / console*.
- Rules only act in exteriors.

## Building

Requirements: Visual Studio 2022 (Desktop C++), CMake 3.21+, [vcpkg](https://github.com/microsoft/vcpkg)
with the `VCPKG_ROOT` environment variable set.

```
cmake --preset release
cmake --build build/release
```

The DLL ends up in `build/release/WeatherBlocker.dll`. Set `SKYRIM_MODS_FOLDER` (MO2/Vortex mods folder) or
`SKYRIM_FOLDER` to have it copied automatically after each build. The GitHub Actions workflow in
`.github/workflows/build.yml` builds the same way and uploads the DLL as an artifact.

`src/SKSEMenuFramework.h` is the header from the SKSE Menu Framework API; keep it in sync with the
framework version you run.

## Known limitations / ideas

- Detection is polling-based, so a blocked weather may be visible for a fraction of a second. A hook on the
  engine's weather change would avoid that.
- The automatic replacement is not region-aware (it can pick a weather that came from another region).
  Choosing a fixed replacement avoids that; a future version could pick from the current region's weather list.
- Weather EditorIDs depend on what the game keeps in memory; the list always shows plugin + FormID too.
