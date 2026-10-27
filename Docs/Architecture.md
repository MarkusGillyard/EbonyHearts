# Architecture and build

## Shape of the game

One C++ module (`Source/MortalSlopbat`), no Blueprints and no level content: the game runs on the
engine's empty `Entry` map and spawns everything from code. The rules are plain C++ with no engine
types beyond containers and maths, so they run headless in tests exactly as they run in a match.

| Folder | What it holds |
|---|---|
| `Combat/` | The simulation, ported from the original's `FightSim`: fighters, moves, hit and hurt boxes, blocking, throws, juggles, counter hits, projectiles, rounds, finishers. Fixed 60 Hz ticks, integer frame counts. `SlopData` loads and validates `Content/Data` (the original's JSON, unchanged); malformed data stops the game at a data-error screen rather than playing wrong. `SlopInput` is the command parser and its six-tick buffer. |
| `AI/` | The CPU brain (every read of the opponent passes through its reaction delay, so it never answers a move it could not yet have seen), the Slopbat's patterns, the training dummy, the stranger's rules. |
| `Anim/` | The original's pose library, IK rig and animator, ported and checked joint by joint against a dump from the Godot build (`Tools/reference`). `SlopMeshRig` maps the solved rig onto the 53-bone skeleton the fighters are skinned to. |
| `Core/` | Game instance (screens, matches, settings, saves), the input router (keyboard halves and pads, menus and fights, rebinding, device assignment, unplug), audio, the arcade and survival flow, save files, the frame-time recorder. |
| `View/` | What is drawn: the match actor (clock, events, presentation), fighters, the fight camera, stages, effects, finisher choreography, tutorial and trials guides, the menu showroom. |
| `UI/` | Slate screens and the HUD, drawn on a 1920 x 1080 canvas scaled to the window. Menus are driven by the input router, never by Slate focus. |
| `Tests/` | Automation tests (`run_tests`), grouped as combat, rounds, AI, animation and game flow. |

**Timing.** `FFixedClock` turns rendered frames of any length into whole 60 Hz ticks (at most four
a frame; a longer stall drops time rather than spiralling). Drawing interpolates between the last
two ticks. Hit-stop freezes the simulation's clock, not the renderer.

**Input.** `FSlopInputRouter` is a Slate input pre-processor: it sees every key, button and axis
before any widget, turns them into per-device state and menu actions, and owns rebinding and the
assignment of devices to players. Pads are identified by their platform device id, so a pad that is
unplugged and plugged back keeps its slot. Triggers are read as axes with hysteresis.

**Presentation.** The simulation emits events (hit, block, throw, KO, line spoken...); the match
turns them into sound, camera shake and kick, hit-stop, impact effects and finisher scenes. Stages
are data: `Content/Data/Stages/<id>.json` places the imported meshes and lights and drives fog,
flicker, sway, glows and weather.

**Rendering.** Deferred, Vulkan SM6, no Lumen, no Nanite, no virtual shadow maps, no ray tracing,
no static lighting (all lights are dynamic and few cast shadows), TAA, screen-space reflections,
manual exposure per stage, volumetric fog on the High preset.

## Content pipeline

Everything the game imports is built from versioned sources by scripts; nothing is hand-edited in
the editor.

| Step | Source | Tool | Lands in |
|---|---|---|---|
| Fighter bodies and costumes | MakeHuman CC0 assets via MPFB, CC0 textures | `Tools/blender/build_fighters.sh <ids>` (Blender 5.2, isolated user config, memory-capped) | `SourceArt/Fighters/<id>/` (FBX, textures, review renders, report) |
| Stage sets | Scripted geometry, CC0 surfaces and Poly Haven props | `Tools/blender/stages/run.sh build_<id>.py` | `SourceArt/Stages/<id>/` (FBX, JSON, review renders) |
| Surface textures | ambientCG and Poly Haven downloads (see the ledger) | `Tools/prep_surfaces.py` | `SourceArt/Surfaces/<set>/` |
| Import | All of the above, audio, skies | `editor_py Tools/import_assets.py` with `SLOP_IMPORT_ONLY=` steps | `Content/Slop/` |
| Material slots | The imported meshes | the `slots` step, in a second editor session | `Content/Slop/` |
| Portraits | The game itself | `run_game -SlopPortraits`, then `Tools/portraits.py`, then the `portraits` step | `SourceArt/Portraits/`, `Content/Slop/Portraits/` |

Third-party downloads are kept out of Git (`SourceArt/ThirdParty/*/`); `SourceArt/ThirdParty/ledger.json`
records each one's source URL, licence and file hashes so they can be fetched again.

## Commands

```bash
cd "$HOME/Documents/Unreal Projects/MortalSlopbat"
source Tools/ue.sh
build_editor                         # compile the editor module
run_tests                            # every automation test, headless; summary in Saved/Logs/tests.log.summary
run_game -SlopMatch=ashka,gorm,watch,rendering_yards
package_linux Shipping               # or Development; output under $BUILDS/<config>/Linux
```

All heavy jobs run inside `systemd-run --user --scope -p MemoryMax=...` (`capped`), so a runaway
import or cook cannot take the desktop down. `run_game` renders offscreen when no monitor is
attached (Vulkan presentation would block without one); offscreen runs have no pads or sound.

Development flags (not in Shipping builds): `-SlopMatch=p1,p2,mode[,stage[,difficulty]]`,
`-SlopShots=ticks`, `-SlopShotEvery=seconds`, `-SlopShotPrefix=name`, `-SlopQuitAt=tick`,
`-SlopQuitAfter=seconds`, `-SlopTour=fighter [-SlopBoxes]` (every ground move from both sides
with boxes and screenshots), `-SlopPortraits[=ids]`, `-debugkeys` (F1 boxes, F2 frames, F3
states, F5 freeze, F6 step, F7 slow, F8 inputs in any mode; in Training and Trials they work
without it).

Test automation flags, in any build so the Shipping package itself can be qualified:
`-SlopUi=delay:action,...` (menu actions as if pressed: up, down, left, right, confirm, back,
pause, next, prev, reset, alt; `@1` for the second player), `-SlopAutoPlay[=0..2]` (the CPU plays
the first side, idle menus are confirmed, fighters are picked at random), `-SlopFrameStats`
(match frame times to a CSV in the save folder, for `Tools/perf/summarize.py`), and
`-profile=name` (separate settings and progress files). `Tools/playtest/run_flow.sh <flow>` plays
one of the scripted flows in `Tools/playtest/flows/` in the `autotest` profile.

`Tools/playtest/vpad.py` drives the game from a virtual pad (uinput) from a script; it needs a
monitor attached. SDL reads Sony pads through hidraw, which a uinput device lacks, so the script's
pad should be the Xbox one (`--xbox`); it exercises the same input path.

## Saves

Settings and progress live in `~/.local/share/MortalSlopbat/UE5/` (`$XDG_DATA_HOME` if set):
`settings.json` and `progress.json`, written atomically, versioned, validated on load; a corrupt
file is set aside as `.bad` and replaced with defaults. On first launch the original game's
`~/.local/share/MortalSlopbat/progress.json` is read once and imported; it is never written.
