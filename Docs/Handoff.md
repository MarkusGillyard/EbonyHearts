# Handoff and checkpoint

The working record of the Unreal Engine rebuild: where things stand, how to run them, what is
next. Update it whenever the state changes.

## State

| Area | State |
|---|---|
| Toolchain | UE 5.8.4 (CL 58902052) at `/media/cinqic/Cinqic Storage/UnrealEngine/UE_5.8.4`. Editor module builds; a minimal Development package built, ran outside the editor and saw the paired DualShock 4 (SDL 3.4.4, "PS4 Controller", Bluetooth). |
| Simulation | C++ port of the Godot 1.1 rules (`Source/MortalSlopbat/Combat`), same data files (`Content/Data`). Malformed data is rejected before a match. |
| AI | CPU brain (all opponent reads through its reaction delay), boss patterns, training dummy, the stranger's rules (`AI/`). |
| Animation | Pose library, IK rig and animator ported (`Anim/`), checked joint by joint against the original. Bodies are driven through `Anim/SlopMeshRig` on the MakeHuman game skeleton. |
| Game | Input router, saves, flow, match world, HUD, all screens (`Core/`, `View/`, `UI/`). Compiles; first live run in progress. |
| Art | MakeHuman CC0 bodies via MPFB 2.0.18 (`Tools/blender/`); costumed fighters being built (Ashka, Gorm first). CC0 stage surfaces, props, skies and Kenney sounds downloaded (`SourceArt/ThirdParty/ledger.json`). |
| Tests | 55 automation tests pass (combat, rounds, content, trials, AI, rig reference). |

## Commands

```bash
cd "$HOME/Documents/Unreal Projects/MortalSlopbat"
source Tools/ue.sh
build_editor                                   # compile the editor module
run_tests Slop                                 # all automation tests, headless
editor_py "$PROJ_DIR/Tools/import_assets.py"   # re-import audio, materials, fighters (-- --only=audio,...)
run_game -SlopMatch=ashka,gorm,watch,rendering_yards,1 -SlopShots=200,400 -SlopQuitAt=450
package_linux Development                      # or Shipping
```

Development flags (non-Shipping builds): `-SlopMatch=p1,p2,mode[,stage[,difficulty]]` (modes:
versus, cpu, training, watch, stranger, trials), `-SlopShots=ticks` and `-SlopShotPrefix=name`
(screenshots into `Saved/Screenshots`), `-SlopQuitAt=tick`, `-SlopQuitAfter=seconds`,
`-debugkeys` (F1 boxes, F2 frames, F3 states, F5 freeze, F6 step, F7 slow, F8 inputs in any mode),
`-profile=name` (separate settings and progress files).

## Next

1. First live run: fighters on screen, camera, HUD; fix what the screenshots show.
2. Stage sets for all four arenas from the CC0 surfaces and props.
3. Costumed fighters into the game; inspect from the gameplay camera, both sides.
4. Package, controller pass, tests for the clock, saves and flow; then the remaining fighters.
