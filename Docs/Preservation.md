# Preservation and completion matrix

What the original game (Godot 4.7.2, version 1.1.0, `/home/cinqic/Documents/Mortal Slopbat`, commit
`e2ec152`) contains, and what the Unreal rebuild does with each part. Evidence paths are in the
original repository unless marked "UE". Disposition: **preserve** (same behaviour), **improve**
(same intent, better), **replace** (different means, reason given), **open** (not done yet).
The Verified column says how it was checked; an empty cell means it has not been.

## Fighters

| Item | Evidence | Disposition | Rebuild | Verified |
|---|---|---|---|---|
| Ashka Rook, Muay Thai and lamp-fire | `data/fighters/ashka.json`, `data/looks/ashka.json`, DESIGN.md roster | Preserve moves and data; improve body (MakeHuman CC0 + scripted costume replaces SDF sculpt) | Same JSON in UE `Content/Data`; body `SourceArt/Fighters/ashka` | Moves: tests + move tour; body built, seen in matches, portrait |
| Gorm Tallow, grappler | `gorm.json` | Same | Same | Rig/moves: tests + move tour; body built, seen in matches |
| Nix Hollow, mix-up and teleports | `nix.json` | Same | Same | Sinkstep test; body built, seen in matches |
| Mirela Sorrow, zoner | `mirela.json` | Same | Same | Trials and CPU pairings in tests; body built, seen in matches |
| Bram Ketch, rushdown | `bram.json` | Same | Same | Parry test; body built (grey-brown hair, mutton chops as in the look file), seen in matches |
| Hollis Fathom, heavy zoner | `hollis.json` | Same | Same | Trials and CPU pairings in tests; body built, seen in matches |
| The Slopbat, boss (1400/1650/1900 health by tower) | `slopbat.json`, `scripts/ai/boss_brain.gd` | Preserve patterns; changes from play (2.5.1), as Markus found the boss fired tar and wind nonstop and had too many unblockables: tar and wind are followed by a wait (150/120/95 ticks by level) and never come twice running; the Bite (a grab) can be broken like a throw, starts in 11 ticks instead of 8, does 130 instead of 170 and waits 300/240/180 ticks between bites. The Storm Shroud stays unblockable (below half health, a 50-tick charge that a blow breaks) | `AI/SlopAI.cpp` FBossBrain | Boss tests (storm gate, unthrowable, fights end at all levels, ranged waits and no repeats against a CPU and a blocker, waits between bites, the bite broken by a punch); body built with wings; fought and beaten in an unattended arcade run |
| Daxion, the stranger in white | `daxion.json`, `scripts/ai/stranger_rules.gd`, DESIGN.md | Preserve, incl. Markus's line verbatim; fix: original clamped his memory to 5 on load, so the recognition scene replayed every session | `FStrangerRules`; memory 0–6 kept | Stranger test (memory only moves forward, bowing out) |
| Alternate colours, mirror-match colours, the Reflection (slop-dark tint) | `fighter_view.gd`, `match.gd` | Preserve | `ASlopFighterActor::SetCostume/SetTint` | Code path; a mirror match played (Survival, Gorm v Gorm); not inspected on screen |

## Stages

| Item | Evidence | Disposition | Rebuild | Verified |
|---|---|---|---|---|
| The Rendering Yards (+ stage finisher "Into the Vat") | `scripts/view/arenas/yards.gd`, `basics.json` `stage_vat` | Preserve composition; improve with modelled Blender set + CC0 surfaces | `View/SlopArena`, `SourceArt/Stages/rendering_yards` | Built, lit, seen from the fight camera; every piece's materials checked (test) |
| The Drowned Chapel | `arenas/chapel.gd` | Same | Same | Same |
| Lowmere Stilt Market | `arenas/market.gd` | Same | Same | Same |
| The Slopbat's Roost (boss stage; versus once unlocked) | `arenas/roost.gd` | Same | Same | Same |

## Modes and flow

| Item | Evidence | Disposition | Rebuild | Verified |
|---|---|---|---|---|
| Arcade: three towers (3/4/4 fights), rival, the Reflection in Master, boss, endings, continue with countdown, scoring | `scripts/core/game.gd`, `results_screen.gd` | Preserve | `Core/SlopFlow`, `UI/SlopScreens*.cpp` | Ladder and scoring tests; the Warrior tower played to the ending and credits unattended |
| Survival: one round a fight, carried health (+30 %, +45 % flawless), difficulty every 3 waves, boss every 5th | `game.gd` | Preserve | `FGameFlow::SurvivalAdvance` | Test; played unattended through waves, a step in difficulty, a loss and a new run |
| Versus with explicit device assignment | `devices_screen.gd`, `versus_screen.gd` | Preserve | `FDevices` screen, router assignment | Scripted flow with both sides picking; two physical pads untested |
| Training: dummy modes (Stand, Crouch, Block All, Block Random, Crouch Block, Jump, CPU), reset, hitboxes, frame data, states, input history | `training_dummy.gd`, `match.gd` | Preserve; improve: works on a controller (Create/View resets, the right stick's click changes the dummy); boxes drawn by the HUD so they work in any build | `FTrainingDummy`, HUD readouts | Dummy in tests; scripted flow (modes, reset, move list) |
| Tutorial (movement, defence, strikes, special, combo, throw, finisher) | `scripts/ui/tutorial.gd` | Preserve | `View/SlopGuides` (`FTutorial`) | Scripted flow through every step (skipped) to the results; the steps' own checks not played by hand |
| Trials (every combo, checked as it lands) | `data/trials.json`, `trial_check.gd`, `trials.gd` | Preserve | `FTrialCheck`, `FTrialsRunner` | Every trial performed headless (test); scripted flow through the runner |
| Extras: endings, records, move lists, the stranger, credits | `extras_screen.gd` | Preserve | `FExtras` | Scripted flow |
| Title, main menu with descriptions, pause, results, face-off, select with portraits and live figures | `ui/*.gd` | Preserve | `UI/`, `View/SlopShowroom` | Seen in screenshots; every screen reached in the flows |

## Mechanics

| Item | Evidence | Disposition | Verified |
|---|---|---|---|
| 60 Hz fixed simulation, inputs buffered 6 ticks, hit-stop ticks don't count, motions with lenient diagonals and crossover reading, chords | `fight_sim.gd`, `input_buffer.gd`, `command.gd` | Preserve | Tests (commands, buffer, crossover, opposite directions, one press one move) |
| Block button (high/mid/overhead standing, low/mid crouching), works from behind | `fight_sim.gd` | Preserve | Tests |
| Run meter, dial-a-combos costing meter per link, chain opener and combo push-back | `fighter.gd`, `fight_sim.gd` | Preserve | Tests; every trial |
| Throws, throw breaks, simultaneous throws break, command grabs beat block | `fight_sim.gd` | Preserve | Tests |
| Counter hits (+15 %, +4 stun, +2 hit-stop) | `fight_sim.gd` | Improve: the extra freeze is the victim's alone (+4) in place of +4 stun and +2 freeze for both, so the blow reads bigger and frame advantage is unchanged | Test |
| Ashka's moves | `data/fighters/ashka.json` | Improve: a teep (forward + low kick), the push kick Muay Thai keeps distance with; low damage, a long push, minus 4 on block | Test; every trial and CPU pairing still pass |
| Gorm's thrusts | `data/fighters/gorm.json` | Improve: his palm strikes and belly bump drive a blocking opponent further back (0.6 m and 1.4 m), sumo's push-out; on hit, and so his combos, unchanged | Every trial and CPU pairing still pass |
| Bram's throws | `data/fighters/bram.json` | Improve: the forward throw is a chancery (head locked under the arm, punched twice, shoved off) and the back throw a cross-buttock (the hip turned in, the opponent levered over it), both legal in the London Prize Ring; frames, damage and release as the shared throws | Seen in recordings; every trial and CPU pairing still pass |
| Juggle points (limit 7), damage scaling from the third hit to 40 % | `fight_sim.gd` | Preserve | Tests |
| Chip only from specials and projectiles; one projectile each; clashes | `fight_sim.gd` | Preserve | Tests |
| Armour, parry, buff, teleport, pull, crumple, daze, air-to-air knockdown | `fight_sim.gd`, fighter data | Preserve | Parry/armour/teleport tests |
| KO freeze (14/18 ticks), slow motion, camera punch-in; FINISH dizzy 7 s; Sloptalities by range; stage finisher | `match.gd`, `fight_sim.gd`, `finisher_director.gd` | Preserve | Round tests; seen in screenshots |
| Post-KO input | `fighter.gd` | Improve: the original let dial-a-combo links keep chaining through the round end; now nothing chains after a knockout | Test |
| CPU reaction delay | `cpu_brain.gd` | Improve: the original read some opponent states and all distances live; now everything is seen through the delay | Identical-seed test |
| Impact presentation (hit-stop, body flash, ring, sparks, light, blood by gore setting, dust, camera shake and kick) | `vfx.gd`, `fight_camera.gd`, `fighter_view.gd` | Improve: the victim's shake fades over the freeze (vertical in the air), bodies keep moving slightly through it, the flash only on counters and heavy blows, the camera still on light and medium blows; sound by where a blow lands and the weapon's material | Seen in recordings |
| Animation: key poses, eases, crossfades, mirroring, planted feet, hit springs | `fighter_view.gd`, `pose_library.gd` | Preserve (the rig matches joint by joint) and improve: follow-through, strikes that drive through contact, reactions by the blow's direction (the hit springs had pitched the head toward the attacker), stance and walk rhythm by style, hands carried with the chest, a turn-round on cross-over, head-shot knockouts with the fencing response, a technical stand-up; per-fighter stance refinements in `poses/<id>_style.json` (Thai guard, Prize Ring guard, sumo squat, bagua twist, ichimonji, a fixed diving helmet) | Tests (attacking arm on its pose, blow directions, continuous turn); seen in recordings |
| The shared basics' animation (jab, body jab, cross, body cross, low and high kicks, roundhouse) | `data/basics.json` keys, `data/poses_common.json` | Improve: the poses come from a martial artist's motion capture (ACCAD, CC BY 3.0), retimed to the same frames; the uppercut, sweep, crouching and jumping attacks and Gorm's own kicks keep the original's key poses | Every trial and CPU pairing still pass; seen in recordings |

## Progression and settings

| Item | Evidence | Disposition | Verified |
|---|---|---|---|
| Records per fighter and tower, survival best, trials done, endings, unlocks (costumes, Roost), stats, tutorial flag, stranger memory | `scripts/core/progress.gd` | Preserve format (version 1); stored in `~/.local/share/MortalSlopbat/UE5/`; the original's `progress.json` is read once and never written | Tests (records, damage, interrupted writes, later versions, migration) |
| Settings: full screen, VSync, graphics, volumes, shake, flashes, effects, gore, input display, smoothing, post effects, difficulty, key rebinding, device assignment | `scripts/core/settings.gd` | Preserve and improve: resolution, frame cap (following the display by default), dynamic render resolution, an Epic graphics level, text size, dead zone, vibration, button names, pad rebinding, voice volume, display changes revert unless kept | Tests (round trip, bad values, reserved inputs); a change saved in a scripted flow; the revert timer, the display modes and F11 checked on a 3440 x 1440 display at 165 Hz |

## Platform

| Item | Evidence | Disposition |
|---|---|---|
| Linux x86_64 build, self-contained archive | `tools/export_linux.sh` | Preserve (UE BuildCookRun package) |
| Vulkan with OpenGL 3 fallback | README | Replace: Unreal 5.8 on Linux renders with Vulkan only (SM6); there is no OpenGL fallback |
| Gamepads (untested in the original) | README | Improve: PlayStation pads are first-class; physical tests listed in `Docs/Validation.md` |
