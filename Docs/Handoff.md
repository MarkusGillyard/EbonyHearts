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
| Combat feel | 2.2.0 applies the presentation changes ranked by the study in `Docs/Martial-arts.md` (strikes, hit-stop, reactions, styles, sound) and one rule change (counter-hit freeze on the victim); frame data otherwise unchanged. |
| Package | `MortalSlopbat-2.3.0-linux-x86_64.tar.gz` (498 MB, SHA-256 `c2b400d647dd6c96a8294d27d83f26fb06153a7534b8047278b16a8420959f57`) in `/media/cinqic/Cinqic Storage/UnrealEngine/Builds/MortalSlopbat/release/`, Shipping build of commit `f156369`; unpack and run `./MortalSlopbat.sh`. Earlier archives (2.0.0 to 2.2.0) are kept beside it. Rebuild with `package_linux Shipping` then `Tools/make_release.sh Shipping`. |
| Saves | Markus's own settings and progress from playing 2.1.0 are in `~/.local/share/MortalSlopbat/UE5/` (settings version 1: its default cap of 60 becomes Match the display when 2.2.0 loads it). Test runs used separate profiles, cleared afterwards. The Godot game's own files in `~/.local/share/MortalSlopbat/` are untouched. |

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

- **More motion capture.** The shared strikes and Ashka's teep come from ACCAD's Male 2 takes
  (CC BY 3.0). The same set has a ready stance and bounce, advance and retreat footwork, blocks,
  dodges, victories and taunts that are not used yet; it has no hit reactions or falls. Unused:
  the uppercut take (no clean contact). A realism pass on the art (skin, cloth, props) is still to
  do; no art assets beyond the original CC0 ones have been taken.
- **The study's other rule changes.** Ashka's teep is in (forward + low kick; a combat test holds
  it), Gorm's blocked thrusts push further, and Bram throws with a chancery and a cross-buttock.
  Not done: a shin check for Ashka, optional leg damage. Each touches balance, so each needs a playtest.
- **Costume secondary motion** (coat tails, veil, ponytail, chain) and damage that builds up on the
  body: need rig bones and material work.
- **Physical controllers.** Markus played menus and fights on his DualShock 4 over Bluetooth with
  2.1.0. Rumble on it, reconnecting mid-fight, DualSense, USB and two physical pads are
  unconfirmed (`Docs/Validation.md`).
- **Audio by ear.** Mixing, levels and the layering of the original's sounds with the CC0 impacts
  have been checked in code and logs, not listened to.
- **Art rough spots** noted by the fighter builds (`SourceArt/Fighters/README.md`): Mirela's skirt
  lifts flat in high kicks, Nix's vest edges step in close-ups, Bram's cap peak floats slightly;
  fighter rebuilds of Ashka, Gorm and Nix vary by a centimetre or two between runs.
