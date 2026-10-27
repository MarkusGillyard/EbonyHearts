# Handoff

Where the Unreal rebuild stands, how to pick it up, and what is left. `Docs/Validation.md` has the
evidence behind every claim here.

## State

| Area | State |
|---|---|
| Source | `~/Documents/Unreal Projects/MortalSlopbat`, branch `ue5`; pushed to the private `MarkusGillyard/Slopbat` as branch `ue5` (its own history; `main` untouched). The Godot original at `~/Documents/Mortal Slopbat` is unchanged. |
| Engine | Unreal Engine 5.8.4 at `/media/cinqic/Cinqic Storage/UnrealEngine/UE_5.8.4`; Vulkan SM6; Linux x86_64. |
| Game | Complete: six fighters, the Slopbat and the stranger; four stages; Arcade, Survival, Versus, Training, Tutorial, Trials, Extras, Settings; saves and settings; controllers and keyboard. |
| Art | All eight fighters built from CC0 MakeHuman bodies with scripted costumes (`Tools/blender`); four stage sets built in Blender (`Tools/blender/stages`); CC0 surfaces, props, skies and sounds. |
| Tests | Automation tests (`run_tests`), scripted flows for every mode (`Tools/playtest`), unattended packaged sessions (`Tools/perf/soak.sh`). |
| Package | `Tools/make_release.sh Shipping` after `package_linux Shipping`; the archive and its checksum are under `/media/cinqic/Cinqic Storage/UnrealEngine/Builds/MortalSlopbat/release/`. |

## Picking it up

```bash
cd "$HOME/Documents/Unreal Projects/MortalSlopbat"
git lfs pull
source Tools/ue.sh
build_editor && run_tests
run_game                                   # or -SlopMatch=ashka,gorm,watch,roost
Tools/playtest/run_flow.sh survival        # any flow in Tools/playtest/flows
package_linux Shipping && Tools/make_release.sh Shipping
```

Rebuilding art: `Tools/blender/build_fighters.sh <ids>` (fighters; needs MPFB 2.0.18 installed into
`Tools/blender/.blender_user`, see `SourceArt/Fighters/README.md`), `Tools/blender/stages/run.sh
build_<stage>.py` (stages), `Tools/prep_surfaces.py` (surfaces, from the downloads in the ledger),
then `editor_py Tools/import_assets.py` with `SLOP_IMPORT_ONLY=fighters` or `stages`, followed by a
second run with `SLOP_IMPORT_ONLY=slots`. Portraits: `run_game -SlopPortraits`, then
`Tools/portraits.py`, then the `portraits` import step.

## Open

- **Physical controllers.** A DualShock 4 is paired over Bluetooth with FLOWBOX and the game
  detects it, but no button on a real pad has been pressed in this work. DualSense, USB, rumble
  and real two-pad play are untested (`Docs/Validation.md`).
- **Windowed performance.** The monitor was switched off for the later part of the work, so the
  final figures were taken rendering offscreen on the same GPU (no presentation, no VSync). A
  windowed run with the monitor on is worth repeating once (`Tools/perf/soak.sh`).
- **Audio by ear.** Mixing, levels and the layering of the original's sounds with the CC0 impacts
  have been checked in code and logs, not listened to.
- **Art rough spots** noted by the fighter builds (`SourceArt/Fighters/README.md`): Mirela's skirt
  lifts flat in high kicks, Nix's vest edges step in close-ups, Bram's cap peak floats slightly;
  fighter rebuilds of Ashka, Gorm and Nix vary by a centimetre or two between runs.
