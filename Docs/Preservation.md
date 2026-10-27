# Preservation and completion matrix

What the original game (Godot 4.7.2, version 1.1.0, `/home/cinqic/Documents/Mortal Slopbat`, commit
`e2ec152`) contains, and what the Unreal rebuild does with each part. Evidence paths are in the
original repository unless marked "UE". Disposition: **preserve** (same behaviour), **improve**
(same intent, better), **replace** (different means, reason given), **open** (not done yet).
The Verified column says how it was checked; an empty cell means it has not been.

## Fighters

| Item | Evidence | Disposition | Rebuild | Verified |
|---|---|---|---|---|
| Ashka Rook, Muay Thai and lamp-fire | `data/fighters/ashka.json`, `data/looks/ashka.json`, DESIGN.md roster | Preserve moves and data; improve body (MakeHuman CC0 + scripted costume replaces SDF sculpt) | Same JSON in UE `Content/Data`; body `SourceArt/Fighters/ashka` | Moves: tests + move tour (UE `Tools`); body: open |
| Gorm Tallow, grappler | `gorm.json` | Same | Same | Rig/moves: tests; body: open |
| Nix Hollow, mix-up and teleports | `nix.json` | Same | Same | Sinkstep test |
| Mirela Sorrow, zoner | `mirela.json` | Same | Same | |
| Bram Ketch, rushdown | `bram.json` | Same | Same | Parry test |
| Hollis Fathom, heavy zoner | `hollis.json` | Same | Same | |
| The Slopbat, boss (1400/1650/1900 health by tower) | `slopbat.json`, `scripts/ai/boss_brain.gd` | Preserve patterns | `AI/SlopAI.cpp` FBossBrain | Boss tests (storm gate, unthrowable, fights end at all levels) |
| Daxion, the stranger in white | `daxion.json`, `scripts/ai/stranger_rules.gd`, DESIGN.md | Preserve, incl. Markus's line verbatim; fix: original clamped his memory to 5 on load, so the recognition scene replayed every session | `FStrangerRules`; memory 0–6 kept | Stranger test (memory only moves forward, bowing out) |
| Alternate colours, mirror-match colours, the Reflection (slop-dark tint) | `fighter_view.gd`, `match.gd` | Preserve | `ASlopFighterActor::SetCostume/SetTint` | |

## Stages

| Item | Evidence | Disposition | Rebuild | Verified |
|---|---|---|---|---|
| The Rendering Yards (+ stage finisher "Into the Vat") | `scripts/view/arenas/yards.gd`, `basics.json` `stage_vat` | Preserve composition; improve with modelled Blender set + CC0 surfaces | `View/SlopArena`, `SourceArt/Stages/rendering_yards` | open |
| The Drowned Chapel | `arenas/chapel.gd` | Same | Same | open |
| Lowmere Stilt Market | `arenas/market.gd` | Same | Same | open |
| The Slopbat's Roost (boss stage; versus once unlocked) | `arenas/roost.gd` | Same | Same | open |

## Modes and flow

| Item | Evidence | Disposition | Rebuild | Verified |
|---|---|---|---|---|
| Arcade: three towers (3/4/4 fights), rival, the Reflection in Master, boss, endings, continue with countdown, scoring | `scripts/core/game.gd`, `results_screen.gd` | Preserve | `Core/SlopFlow`, `UI/SlopScreens*.cpp` | open |
| Survival: one round a fight, carried health (+30 %, +45 % flawless), difficulty every 3 waves, boss every 5th | `game.gd` | Preserve | `FGameFlow::SurvivalAdvance` | open |
| Versus with explicit device assignment | `devices_screen.gd`, `versus_screen.gd` | Preserve | `FDevices` screen, router assignment | open |
| Training: dummy modes (Stand, Crouch, Block All, Block Random, Crouch Block, Jump, CPU), reset, hitboxes, frame data, states, input history | `training_dummy.gd`, `match.gd` | Preserve; improve: works on a controller (Create/View resets, R1 changes the dummy) | `FTrainingDummy`, HUD readouts | Dummy in tests; UI open |
| Tutorial (movement, defence, strikes, special, combo, throw, finisher) | `scripts/ui/tutorial.gd` | Preserve | open | |
| Trials (every combo, checked as it lands) | `data/trials.json`, `trial_check.gd`, `trials.gd` | Preserve | `FTrialCheck`; in-match runner open | Every trial performed headless (test) |
| Extras: endings, records, move lists, the stranger, credits | `extras_screen.gd` | Preserve | `FExtras` | open |
| Title, main menu with descriptions, pause, results, face-off with Loading note | `ui/*.gd` | Preserve | `UI/` | Title/menu built; open |

## Mechanics

| Item | Evidence | Disposition | Verified |
|---|---|---|---|
| 60 Hz fixed simulation, inputs buffered 6 ticks, hit-stop ticks don't count, motions with lenient diagonals and crossover reading, chords | `fight_sim.gd`, `input_buffer.gd`, `command.gd` | Preserve | Tests (commands, buffer, crossover, opposite directions, one press one move) |
| Block button (high/mid/overhead standing, low/mid crouching), works from behind | `fight_sim.gd` | Preserve | Tests |
| Run meter, dial-a-combos costing meter per link, chain opener and combo push-back | `fighter.gd`, `fight_sim.gd` | Preserve | Tests; every trial |
| Throws, throw breaks, simultaneous throws break, command grabs beat block | `fight_sim.gd` | Preserve | Tests |
| Counter hits (+15 %, +4 stun, +2 hit-stop) | `fight_sim.gd` | Preserve | Test |
| Juggle points (limit 7), damage scaling from the third hit to 40 % | `fight_sim.gd` | Preserve | Tests |
| Chip only from specials and projectiles; one projectile each; clashes | `fight_sim.gd` | Preserve | Tests |
| Armour, parry, buff, teleport, pull, crumple, daze, air-to-air knockdown | `fight_sim.gd`, fighter data | Preserve | Parry/armour/teleport tests |
| KO freeze (14/18 ticks), slow motion, camera punch-in; FINISH dizzy 7 s; Sloptalities by range; stage finisher | `match.gd`, `fight_sim.gd`, `finisher_director.gd` | Preserve | Round tests; presentation open |
| Post-KO input | `fighter.gd` | Improve: the original let dial-a-combo links keep chaining through the round end; now nothing chains after a knockout | Test |
| CPU reaction delay | `cpu_brain.gd` | Improve: the original read some opponent states and all distances live; now everything is seen through the delay | Identical-seed test |
| Impact presentation (hit-stop, body flash, ring, sparks, light, blood by gore setting, dust, camera shake and kick) | `vfx.gd`, `fight_camera.gd`, `fighter_view.gd` | Preserve | Seen in UE screenshots; tuning open |

## Progression and settings

| Item | Evidence | Disposition | Verified |
|---|---|---|---|
| Records per fighter and tower, survival best, trials done, endings, unlocks (costumes, Roost), stats, tutorial flag, stranger memory | `scripts/core/progress.gd` | Preserve format (version 1); stored in `~/.local/share/MortalSlopbat/UE5/`; the original's `progress.json` is read once and never written | open |
| Settings: full screen, VSync, graphics, volumes, shake, flashes, effects, gore, input display, smoothing, post effects, difficulty, key rebinding, device assignment | `scripts/core/settings.gd` | Preserve and improve: resolution, frame cap, UI size, dead zone, vibration, button names, pad rebinding, voice volume, display changes revert unless kept | open |

## Platform

| Item | Evidence | Disposition |
|---|---|---|
| Linux x86_64 build, self-contained archive | `tools/export_linux.sh` | Preserve (UE BuildCookRun package) |
| Vulkan with OpenGL 3 fallback | README | Replace: Unreal 5.8 on Linux renders with Vulkan only (SM6); there is no OpenGL fallback |
| Gamepads (untested in the original) | README | Improve: PlayStation pads are first-class; physical tests listed in `Docs/Validation.md` |
