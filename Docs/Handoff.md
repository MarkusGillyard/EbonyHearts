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
| Combat feel | 2.2.0 applies the presentation changes ranked by the study in `Docs/Martial-arts.md` (strikes, hit-stop, reactions, styles, sound) and one rule change (counter-hit freeze on the victim); frame data otherwise unchanged. 2.3.0 to 2.5.0 bring in motion capture: strikes, footwork, the block, Daxion's evade, intros and wins. |
| Package | `MortalSlopbat-2.5.1-linux-x86_64.tar.gz` (498 MB, SHA-256 `fa8f66c71529af41f43982911c40daa4c6178b6246e3bd5fee85a46ab764aa7c`) in `/media/cinqic/Cinqic Storage/UnrealEngine/Builds/MortalSlopbat/release/`, Shipping build of commit `27c727f` (2.5.0 plus the Slopbat's fairer tar, wind and bite); unpack and run `./MortalSlopbat.sh`. Earlier archives (2.0.0 to 2.5.0) are kept beside it. Rebuild with `package_linux Shipping` then `Tools/make_release.sh Shipping`. |
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

- **More motion capture.** From ACCAD's Male 2 takes (CC BY 3.0): the shared strikes, Ashka's
  teep, walking and running (step cycles played by distance, so the feet keep pace), the standing
  block's guard, Daxion's evade, and the human fighters' intros and wins. Gorm, Mirela and Hollis
  keep their own walks (a sumo shuffle, circle walking, a diver's plod). The set has no hit
  reactions or falls, so those stay procedural. Unused: the bounce (the study prefers a rock for
  the idle) and the uppercut take (no clean contact).
- **Realism of the art.** Skin has subsurface scattering and sweat, cloth has folds and wear, hair
  is matte (`M_Character`, `M_CharacterSkin`); standing water ripples faintly so reflections waver,
  the Roost's slop lake is a lit liquid over its glow, and contact shadows ground feet and hands
  (`M_Water`, `ASlopArena`). The meshes and textures are the original CC0 builds. Real hair
  (grooms), cloth simulation and new props are not done.
- **The Slopbat after Markus's play (2.5.1).** He found it spammy, with too many unblockables: tar
  and wind now wait between uses, and the bite can be broken, comes slower and less often. The Storm
  Shroud is still unblockable (telegraphed, broken by a blow); making it blockable with heavy chip
  is the next step if it still feels unfair. Not yet played since the change.
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
