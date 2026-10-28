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
| Package | `MortalSlopbat-2.6.1-linux-x86_64.tar.gz` (498 MB, SHA-256 `a79c9e3031438ebe8a530aae588d4762a093a7c9260a45a842b337d23a14fd6d`) in `/media/cinqic/Cinqic Storage/UnrealEngine/Builds/MortalSlopbat/release/`, Shipping build of commit `35dbc6e` (2.6.0's bruises and blood, limp, shin check and parries, plus the camera's aspect fix, marks healing in Training and the frame view measuring parries); unpack and run `./MortalSlopbat.sh`. Checked unpacked from the archive, offscreen at 3440 x 1440 (60 FPS, render scale 63-68 %, no hitches after the first 5 s). Earlier archives (2.0.0 to 2.6.0) are kept beside it. Rebuild with `package_linux Shipping` then `Tools/make_release.sh Shipping`. |
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
  the idle) and the uppercut take (no clean contact). Since 2.5.2 a captured cycle's foot stays
  where it touched down (it slid up to a quarter of the way walking back), and a walk held against
  a wall or the distance limit settles into the stance instead of freezing mid-stride.
- **Measured fit of the animation to the rules (2.5.2 tests).** Every ground strike has a hand,
  foot, elbow, knee or the head within 12 cm of its hitbox on its first active frame; the crouching
  head now sits within 15 cm of the crouch hurtbox (it stood 25-29 cm above, so high attacks passed
  through it). Still off, and left because the boxes are rules: standing, Gorm's sumo stance, Nix,
  Mirela and the Slopbat sit 16-39 cm under a hurtbox drawn for upright fighters, so a high attack
  passing just over those heads still hits.
- **Bodies against the floor and each other (2.5.3 tests).** Knocked down, the spine now rests
  9-11 cm up (it floated at 15-18); thrown, the two bodies' spines and heads stay 11 cm or more
  apart (the chancery had the victim's head inside Bram, the back throws clipped heads); fighters
  pushed together stay 31 cm clear. Bram's cross-buttock is the closest, at 11 cm, as it rolls the
  victim over his back.
- **Steadiness and cloth (2.5.6).** The fight camera swung wildly after any long frame (its
  springs were stepped whole); they now step at 240 Hz, as do the animator's lean, recoil and
  tumble springs (a test held after a stall shows joints jumping 1.5 m a frame with the old ones).
  Coats, gowns, the stranger's robe and Gorm's apron sway with the hips' motion through the
  material (no cloth bones), up to 10 cm at the hem. The Storm Shroud stays unblockable, now with
  its telegraph back.
- **Restored from the original (2.5.5).** Comparing the port's presentation with the Godot
  scripts found features lost in the port: projectiles were all glowing spheres (now the sickle's
  blade on its chain, the harpoon on its line, the anchor, the tar glob, the faint bubble, dust for
  the ground slap and the gust, as the original drew them); the Storm Shroud's telegraph (the
  green light swelling over its charge, thunder and lightning when it fires) was missing, which
  matters for the one unblockable left; Nix's chain was not drawn whipping out to Undertow's reach;
  throw breaks, the deciding blow and projectiles breaking up had lost their effects. Not restored:
  the original's simulated cloth and hair sheets (`look.cloth`), which the port's meshes have no
  bones for.
- **Smaller fixes (2.5.4).** Slate's font cache no longer flushes mid-fight (it did every few
  seconds: animated text made new glyph sizes every frame and the cache allowed one page); a
  standing fighter kicked in the legs buckles at the knee instead of flinching at the chest; the
  crouch holds its guard in front of the face (the 2.5.2 crouch had the hands in the chest); the
  dummy can block after a hit (to show gaps in combos) and break throws; the stranger's moves
  have swing sounds. Measured and left: TSR's anti-flicker and two-level ambient occlusion cost
  about 1.7 ms at 3440 x 1440, maybe 3 % of render scale at the monitor's usual 58 %, at a cost in
  shimmer and contact shading.
- **The mix, measured (2.5.3).** `Tools/audio_levels.py` reads every sound's peak and loudness:
  the Kenney files are peak-normalised, so the short weapon contacts played 4-10 dB under the
  fists. They were raised as far as their peaks allow; nobody has listened since.
- **Training (2.5.2).** Training Options in the pause menu (dummy, boxes, frame data, states, input
  display, slow motion) for the controller; the frame view shows the measured advantage of the last
  exchange; the dummy can record ten seconds of the player's control and play it back. The listed
  advantages are now a frame lower than the original's, matching what is measured.
- **Realism of the art.** Skin has subsurface scattering and sweat, cloth has folds and wear, hair
  is matte (`M_Character`, `M_CharacterSkin`); standing water ripples faintly so reflections waver,
  the Roost's slop lake is a lit liquid over its glow, and contact shadows ground feet and hands
  (`M_Water`, `ASlopArena`). The meshes and textures are the original CC0 builds. Real hair
  (grooms), cloth simulation and new props are not done.
- **The Slopbat after Markus's play (2.5.1).** He found it spammy, with too many unblockables: tar
  and wind now wait between uses, and the bite can be broken, comes slower and less often. The Storm
  Shroud is still unblockable (telegraphed again since 2.5.5, broken by a blow); if it still feels
  unfair in play, the change is to make it blockable with heavy chip. Not yet played since.
- **The fight shows on the bodies (2.6.0).** Each blow marks the part of the body it lands on
  (head, torso or legs, by the nearest joint of the struck pose): the skin flushes, then bruises in
  patches that spread and darken, and a beaten head (less the torso) opens a few cuts that run
  down (the gore setting scales the blood; Off shows none; since 2.6.2 cuts, not the rash of
  specks 2.6.0 drew on pale skin). The marks last the match (`M_CharacterSkin`'s Damage,
  `ASlopFighterActor::TakeBlow`). Legs beaten past a third limp: standing, the weight sits back off
  the lead leg; walking, the hips drop as it takes the weight. The limp is drawn only, with walk
  speed unchanged, as the study advised for leg damage. `-SlopMarks=0.8` (development) starts a
  match with both fighters marked, to look at it.
- **Markus's playthrough of 2.6.1 (2.6.2).** He cleared the Easy tower as Ashka in four
  minutes, the Slopbat flawless in 24 s, on an Elbow Rush loop: it cancelled into itself inside
  its own hitstun (29 hits for all of Nix's health). Closed, with Mirela's Keen dazing into
  itself, and tested for every special (`Docs/Design.md`, "Loops closed"). He wants that string
  on another fighter: `self_cancels` on a move gives it a bounded number of repeats; which
  fighter is still his call. Also from the session: the counter-hit flash washed Hollis's dark
  suit white (it now brightens the body's colours); the title asked for Enter while he held a pad
  (a pad connected before launch is unknown until it sends input, so it now says "Press any
  button"), and a back there quit the game. Two 29-30 ms frames mid-fight had the game, render
  and GPU all at their usual times, so the stall was outside the game. The session's frame
  record is `Saved/Perf/play-2.6.1-markus/`. Not yet played since: the Easy tower and the
  Slopbat without the loop.
- **The camera on other screens (2.6.1).** The fight view is 30 degrees tall on every shape of
  screen now; before, Unreal held a 16:9 camera's vertical angle, so 4:3 cut the fighters off and
  Markus's 3440 x 1440 drew them a quarter smaller than the framing meant. On his display the
  fighters are therefore bigger in 2.6.1 than in any build he has played, with the stage still
  wider around them than at 16:9. If that reads too close, the lever is `ASlopFightCamera::VFov`
  for wide screens only.
- **The study's other rule changes.** Ashka's teep is in (forward + low kick; a combat test holds
  it), Gorm's blocked thrusts push further, and Bram throws with a chancery and a cross-buttock.
  Since 2.6.0 Ashka has the shin check (back, down + low kick): a parry for lows only, frames 2-14
  of 22; it stings the kicker for 30 (never the last of a round) and leaves Ashka 2 to 14 frames
  ahead, and a high or mid goes straight through it. The CPU uses parries as reads, not reactions
  (through its delay a blow is already out when seen): hit that way before, it bets the attacker
  repeats the blow the moment they can and times the parry's window to meet it (fighter AI
  "parries"; `FCpuBrain::ParryRead`). Against a fighter sweeping on every chance Ashka is swept
  27 % of the time with the check, 32 % without; since 2.6.2 the CPU Bram uses Stand Down too
  (it never had), hit by 24 % of a jabber's jabs with it and 23 % without, its catches answered
  with the baton (AI tests). Parries now show and sound (a pale ring, a flash, the attacker thrown back off the
  guard; until 2.6.0 Bram's Stand Down caught blows silently, as in the original), and the move
  list gives a parry's window and what it catches instead of meaningless frame data. Not played
  yet: the check's numbers are a first guess.
- **Costume secondary motion** (coat tails, veil, ponytail, chain): needs rig bones. The original
  simulated cloth and hair as sheets driven from `look.cloth`; a port would need those sheets as
  skinned meshes with bones of their own. Cloth sways through the material since 2.5.6.
- **Physical controllers.** Markus played menus and fights on his DualShock 4 over Bluetooth with
  2.1.0. Rumble on it, reconnecting mid-fight, DualSense, USB and two physical pads are
  unconfirmed (`Docs/Validation.md`).
- **Audio by ear.** Mixing, levels and the layering of the original's sounds with the CC0 impacts
  have been checked in code and logs, not listened to.
- **Art rough spots** noted by the fighter builds (`SourceArt/Fighters/README.md`): Mirela's skirt
  lifts flat in high kicks, Nix's vest edges step in close-ups, Bram's cap peak floats slightly;
  fighter rebuilds of Ashka, Gorm and Nix vary by a centimetre or two between runs.
