# Validation

What was checked, how, and what was not. FLOWBOX: AMD Ryzen 7 5700G (8 cores, 16 threads),
14 GiB RAM, NVIDIA GeForce RTX 2060 6 GB, driver 595.99.02, Linux 7.0 (Linux Mint 22.3), X11,
Unreal Engine 5.8.4, Vulkan SM6. Evidence files are under `Saved/` (not versioned): `Saved/Perf/`
for sessions, `Saved/Logs/tests.log` for the last test run.

## Automated tests

`run_tests` (Unreal automation, headless): **135 tests, all passing** on the source of 2.7.1 (133 on 2.7.0, 101 on 2.6.7).

| Area | What the tests hold |
|---|---|
| Timing and input | Same inputs at 30/60/144 Hz and uneven frame times give the same match; the clock's catch-up and overload; the 6-tick buffer and its expiry; presses during hit-stop kept; held directions; motions from either side and through a crossover; opposite directions cancel; one press starts one move. |
| Combat | A counter hit's extra freeze falls on the victim alone and leaves frame advantage as it was; Ashka's teep comes out on forward + low kick and pushes far for little damage; active windows from the first frame; whiffs out of range; highs, lows, overheads and guards from either side; chip only from specials and projectiles; throws, breaks, back throws, command grabs beating block; trades; counter hits; damage scaling; juggle limit; run-meter costs; projectiles one at a time and clashing; parry, armour, teleport; Ashka's shin check catches a sweep (the kicker stung and left open, Ashka moving first) and lets a high punch through; no special loops into itself, inside its cancel window or out of it, mid-stage or at the edge, and one allowed to (Bram's Prize Ring Drive) strings exactly its three, and mashing any button ends within a string; bodies never overlap or leave the stage. |
| Match rules | KO and the next round, double KO, timeout and draw, a shorter clock or none, deciding KO with finisher, the finish window timing out, restart clearing everything, input after KO changing nothing, flawless, no match running forever. |
| Roster and AI | All data loads and validates; every trial possible; CPU against CPU in every pairing; the CPU never reacts before its delay; the boss can't be thrown, fights to an end, and storms only below half health; the Grandmaster's CPU uses every one of his arts; swept again and again, the CPU Ashka checks some sweeps and is swept no more often than guarding alone; jabbed again and again, the CPU Bram parries some jabs with Stand Down and is hit no more often than blocking alone; a blocked uppercut thrown from close in is punished by the hard CPU 26 times in 28 and by the easy one 26 in 78 (before 2.6.6: 8 in 32 and 3 in 83), and a roundhouse at its range, pushed out of reach, never; uppercuts whiffed from just out of reach are punished by the hard CPU 39 times in 154, every answer landing (before 2.6.7: 27 in 169); a blocked jump-in is read by its fall: kicked early (a long way to fall) the CPU's answer lands 21 times in 21, kicked deep (landing at once) it swings at once 1 time in 7 (7 in 9 when the anti-air planned from the sight of the jump still fired). |
| Animation | The ported rig matches the Godot original joint by joint on every pose of every fighter; the animator follows every state of a match; through the follow-through the arm carrying a blow stays exactly on its authored pose until the blow is over; a straight drives the struck head away, a hook turns the face the way the fist travels, an uppercut lifts the chin and a body shot folds the torso, from either side; turning round after a cross-over moves no joint more than 30 cm in a frame and ends facing the new way; a high punch marks the face, a low punch the torso and a sweep the legs of every fighter; beaten legs limp (hips back standing, lower walking) with the feet on the floor. |
| Content | Every fighter has a body that maps onto the rig, the character material on every slot and a portrait; every stage piece the data places exists with its materials and surface textures. |
| Saves | Settings round trip and bad values; rebinding never binds twice; reserved keys and pad buttons refused (directions, menu keys, F-keys); progress records, a damaged file set aside, the original game's file read once and never written; an interrupted write recovered; a damaged settings file set aside; a later version's progress copied aside. |
| Flow | Arcade ladders, scoring and the end; Survival waves, carried health and the boss; alternate colours kept through the run and Survival's Try Again; the Grandmaster's secret rung after a run without a continue. |

## Ebony Hearts and the new fighters (2.7.0)

Checked on the editor build, windowed at 1920 x 1080 on the real display, with screenshots
(`Saved/Screenshots/LinuxEditor/`: `nf_*`, `tour_*`, `ev_*`, `spent_*`, `gmst_*` for the fighters'
first pass; `gmx_*`, `gmy_*`, `gmz_*`, `gml_*`, `gmb_*` for the expansion, played by the
development script `-SlopScript=` against a standing opponent) and the log.

| What | How | Result |
|---|---|---|
| Roster and canon | `Fighters.RosterOfSevenAndTheSecretGrandmaster`, `Content.DaxionIsArchivedNotInTheGame`; the select screen in a fresh profile and with `-SlopUnlock=grandmaster`; a boss fight | Seven to start (the six and Wren), the eighth tile sealed until unlocked; no data errors; nothing loaded or cooked names Daxion and his archive is whole; the HUD reads "WREN" and "EBONY HEART FRAGMENT" |
| Qhi and no run meter | `OwnBodyHasQhiAndNoRunMeter`; the HUD in a match | Gold Qhi bar with the half mark, no run bar; Run + forward glides on Qhi |
| (a) Borrowed Face | `BorrowsEveryStandardOpponentForHalfTheQhi` (all seven), `CopyOutfightsTheOriginal`, `ShapeshiftNeedsHalfTheMeter`, `BrokenShapeshiftCostsLittle`, `RevertingRefundsNothing`, `ADecidedFightLetsTheShapeGo`, `MirrorsCannotBeBorrowed`, `NoCancelIntoAChangeOfShapeAndNoFarming`, `RememberedShapeCostsThirtyAndIsNotEnhanced` | Exactly 50 on frame 30, Qhi-enhanced, said to be copied; refused below it (aloud); 8 for a broken startup; nothing back on reverting |
| (b) Form library | `FormLibraryHoldsEveryFighterAndTheFragment`, `RememberedShapeCostsThirtyAndIsNotEnhanced`; `gmy_03` | Every fighter's shape and the Fragment's, each on an input of its own; 30 on frame 24, not enhanced, said to be remembered; straight from one shape into another; "SHAPE: NIX HOLLOW" on screen |
| (c) Inside a combo | `ChangesShapeInsideACombo`; `gmz_01` | Frame 3, at once actionable, the opponent still caught, the hit count carrying on (jab, Ashka's jab, Own Face, his kick: 1, 2, 3); two changes a combo and no third; counted afresh after |
| (d) The Fragment's shape | `FragmentShapeCostsSixtyHasNoRunMeterAndDoubleUpkeep`; `gmy_04`, `gmy_05` | 60, no second meter, no boss in it, his own health, upkeep exactly double; facing the Fragment, Borrowed Face takes it for 50, enhanced, upkeep still double; its Tar Spit fired from his hands |
| (e) Own Face | `OwnFaceIsFreeAndInABorrowedShapeAlwaysMeansOwnFace` | Frame 14 (3 in a combo), free, nothing back, never a copy or a refusal on the way |
| Instant kills | `TwoFingersOneStrikeKillsAndHasCounterplay`, `FindTheCrackKillsAStandingOpponentUnlessInterrupted`, `HeartStopEndsOnlyALongComboOfTwoShapes`, the trial "Two Shapes, Heart Stop"; `gmz_03`, `gmz_05`, `gmx_12`, `gmx_13` | Each ends the round whatever the health: One Strike only with 75 Qhi (else a parry), not on a low, not on the Fragment; Find the Crack 90, broken by a jab in its startup (no refund), never begun on an airborne opponent; Heart Stop only at five hits through two shapes, for all the Qhi; on screen the call, the K.O. and the close-up |
| Spent | `EmptyMeterBreaksTheShapeIntoTheSpentBody`, `SpentBodyIsWeakTraditionalBoxing`, `EveryRoundStartsInHisOwnShape`; `gmx_08` to `gmx_11` | Traditional boxing (no drunken boxing, no grappling, no kicks): a high guard, jab, cross and hook chain, the right hook, the left hook to the body, a slip and a parry; weaker on both counts; no Qhi move or glide; "THE SHAPE BREAKS: QHI SPENT"; recentres short of any shape |
| Every move | `EveryMoveIsCovered` (`Docs/MoveCoverage.md`) | 247 of 247 counted moves covered (17 excluded: finishers and moves the game starts itself); Daxion's three moves carried |
| Defences | `WorstPhysicalBestMagicalDefence` | Ashka's jab hurts him more than Bram, her Ember Shot half as much, Wick Knee by its share |
| Move list | Pause, Move List, as the Grandmaster (`gml_*`) | The Qhi rules, each change of shape with its frame (and frame 3 in a combo) and price (the Fragment's upkeep x2), the kills' gates, "WHEN SPENT (boxing)" |
| Wren | `FeintCancelsIntoAnythingButItself`, `BarTheDoorCatchesIntoAnArmlock`, `FlickerStepPassesThroughProjectiles`; her tour with boxes; the data-wide tests | All hold; unchanged by the expansion |
| Unlock | `UnlockedByAnArcadeWithoutAContinue`; `-SlopSecret=wren -SlopAutoPlay=2` in a fresh profile | The still-water face-off, the challenge fight, the result, the ending, and `fighter:grandmaster` saved |
| Old saves | `Saves.ProgressRecordsCorruptionAndMigration` (an older file with a "stranger" memory) | Loaded, records brought over, the memory carried as it was, the original file untouched |
| CPU | `GrandmasterKeepsTheQhiRules`, `GrandmasterCpuUsesHisArts`, `NewFightersMatchups`, every pairing | Below |

CPU against CPU (`AI.NewFightersMatchups`; the numbers swing with the seeds, so the tuning runs
used 24 seeds a pairing, 168 matches a level, with `-SlopSeeds=24 -SlopOnly=grandmaster`). The
Grandmaster at Novice, Warrior, Master and his challenge's tier (against Master CPUs): before the
expansion's AI pass 9%, 16%, 32%, 68% (8 seeds) and 13%, 17%, 28%, 63% (24 seeds); after it 47%,
50%, 58%, 90% (24 seeds; 39%, 52%, 61%, 89% at 8). The sweep that found it: remembered shapes
taken from a standstill lost matches (switching them off alone gave 32% and 71% at the top two
levels), and his zoning won them (palms from 1.6 m at zoning 0.7: 65% and 87%). His telemetry
at Master over the final 168 matches: 91% of the time in his own body (dealing 23 a second, taking
20), 8% in a shape (23 and 20), 1% spent; 76 changes of shape inside combos; the Fragment's shape 39
times; Find the Crack tried 3 times and always answered; One Strike never triggered at Master;
Heart Stop rare (its combos seldom reach five hits). Wren unchanged: 40%, 21%, 50%. Across
all 192 pairings (three levels, both sides): Ashka 33, Gorm 24, Nix 28, Mirela 10, Bram 34,
Hollis 19, Wren 18, the Grandmaster 26 match wins (17 before the pass).

**2.7.2 changed these numbers.** CPUs had no answer to zoning (Mirela firing from 1.6 m won 98% at
Master), which had flattered the Grandmaster's palms. With CPUs that go over or through a thrown shot,
he wins 18-27%, 27%, 29% and 55-64% (Novice, Warrior, Master, challenge tier; 8-16 seeds), Wren 38%,
19%, 44%, Mirela about 60%, 48%, 29%. The 192 pairings: Bram 39, Ashka 30, Gorm 25, Nix 24, Mirela 23,
Hollis 21, the Grandmaster 17, Wren 13. His Master bound in `AI.NewFightersMatchups` was lowered to
15% on that evidence; the challenge tier, where players meet him, still has to beat 50%.

**2.8.0: the six starters, at 12 seeds a pairing (60 matches a level).**

| Level | Standings |
|---|---|
| Novice | Gorm 60, Bram 58, Mirela 52, Nix 50, Ashka 42, Wren 35 |
| Warrior | Nix 67, Bram 63, Gorm 58, Mirela 53, Ashka 40, Wren 27 |
| Master | Bram 75, Ashka 72, Wren 42, Nix 38, Mirela 32, Gorm 27 |

They are far from even, though CPU mirrors are a poor proxy. Trading
the 4-frame heavy punch from Bram to Wren changed nothing beyond the noise (Wren 33/23/48, Bram 57/63/77), so
it was put back. The Grandmaster at his challenge tier, 1.83 m tall now: 33%, 33%, 29%, 75% against the six.

**2.7.3: where the gap between the CPUs comes from.** The tally above is one match a pairing, too few to
rank anyone. At 12 seeds a pairing (72 matches a level, each fighter against the other six):

| Level | Standings |
|---|---|
| Novice | Gorm 67, Bram 61, Mirela 54, Nix 50, Ashka 44, Wren 36, Hollis 25 |
| Warrior | Nix 72, Gorm 64, Bram 61, Mirela 50, Hollis 46, Ashka 39, Wren 24 |
| Master | Bram 72, Ashka 71, Hollis 67, Wren 44, Nix 42, Gorm 29, Mirela 26 |

Tried on the CPU's notes alone, then put back: Wren closer in (range 1.05 to 0.9) with Turning Light
half as often, 32/25/38, no better; Bram less aggressive (0.75 to 0.6), 50/64/81, stronger at Master;
Mirela and Gorm more aggressive (Mirela 60/54/26, Gorm 69/53/28), no real change; Bram's Stand Down
out of CPU hands altogether, 57/69/79, no weaker. Neither aggression nor the parry carries him. The
difference is in the moves: normals are nearly the same across the roster, Bram's HP has the fastest
startup in the game (4 frames, shared with Nix's) at full damage (50 to Nix's 45), and Wren has the
shortest reach (0.65 to 0.92 m) on the same frames and damage as everyone else. Frame data was left
alone: these are CPU mirrors, not players, Markus cleared all three towers with Wren, and a change of
that kind is for a human to judge in play.

Found and fixed while checking: the CPU pairing test kept six win counters (a seventh fighter wrote
past them); a body put on over another kept the old body's materials; the pose library rebuilt for
a new form kept its old poses; a gold rim at four times a fighter's own washed a borrowed shape
out; a move without a swing sound asked for a sound called "None". In the expansion: in a
remembered shape facing someone else, D,B,F + Block copied the opponent instead of giving his own
face back (it is Own Face in every shape now); the unholdable Fragment could be caught into One
Strike; Heart Stop's six-hit gate was out of reach of any combo in the game (five now, with a
trial for it); a remembered shape of the fighter he faced was called "BORROWED FACE"; the
trials of Wren and the Grandmaster ran against Gorm once their invented rivals were gone (they
name partners now); a full asset import would have brought Daxion's body back.

## Playing the modes

Unattended runs on the editor build and the packages, rendering offscreen at 1920 x 1080 (see
"Display" below), with the scripted flows in `Tools/playtest/flows` and the CPU playing
(`-SlopAutoPlay`). Each was followed screen by screen in the log and in screenshots.

| Flow | Result |
|---|---|
| Arcade (CPU at Master, Warrior tower) | Six fights in turn (Hollis, Gorm, Bram, Mirela, the rival Nix, the Fragment in the Roost), the ending, credits, the main menu, and a second ladder begun; finishers performed. |
| Survival | Waves carried over with health refills, difficulty stepping up after wave three, a loss, the results and a new run. |
| Versus | Devices (Continue), both players picking (the second through its own side), stage, face-off, fight, out through the pause menu. |
| Versus CPU | The player's fighter, then the CPU's, stage, the face-off naming the difficulty, fight, out through the pause menu; once with first to 3 and no clock set in Settings (the face-off says so, the HUD shows the infinity sign and three round marks a side). |
| Training | Fighter and dummy picks, stage, the dummy's modes cycled, reset, the move list from pause, character select and main menu. |
| Tutorial | Every step skipped in turn to the end; the results. |
| Trials | Each trial in turn with its prompt and ticks; out through pause. |
| Extras, Settings | Endings, records, move lists, credits; Settings pages; a changed volume saved to the profile. |
| Soak | 30 minutes of the Development package (20 matches) and 30 minutes of the Shipping package (below), no assertions, errors or stuck rounds. |

Seen in screenshots: all eight fighters in matches and all four stages from the fight camera,
fighters on both sides of the screen; every ground move of Ashka, Gorm and the Fragment at its
first active frame with boxes drawn (`-SlopTour`); finishers; the select screen and face-off
figures; every menu and overlay; HUD readouts in training and trials.

## Controllers

| Route | Result |
|---|---|
| Virtual Xbox 360 pad (uinput) on the real input path (SDL 3, engine, router), monitor attached | Passed: title to an arcade fight and back to the menu and out, controller only; the pad unplugged mid-fight paused the match; plugged back, it took the same slot and the pause menu worked. (`Tools/playtest/vpad.py --xbox`, `Tools/playtest/scripts/pad_arcade_flow.txt`.) |
| Virtual DualShock 4 (uinput) | Not usable: SDL reads Sony pads through hidraw, which a uinput device lacks, so it is ignored. |
| Real DualShock 4 over Bluetooth (paired with FLOWBOX) | Played by Markus on 27 October (2.1.0): menus and fights, about four minutes, with the PlayStation button names shown; no fault with the pad was reported. After a night idle, BlueZ listed it as connected with no kernel input device until the stale link was dropped and the PS button pressed (a Bluetooth stack state, not the game's). Rumble and reconnecting mid-fight on the real pad are not confirmed. |
| DualSense, USB connection, two physical pads, keyboard plus pad | **Unverified** (no hardware handled). |

Rumble is implemented through the engine's force-feedback interface for the struck player's pad;
whether it is felt on a real pad is unverified.

## Performance

### On Markus's monitor (2.2.0)

2.5.6 (commit `fb6c57c`) offscreen, unpacked fresh (checksums matched): 85 s of an arcade fight
at 60 fps, every frame within 1 ms, no hitches, GPU 14.93 ms at a 76 % render scale, peak memory
1,447 MB (`Saved/Perf/offscreen-2.5.6`). A four-hour watch (27 October, 17:50-21:50) of a game
started windowed while the monitor was in standby, to see whether it resumes when the display
returns, was inconclusive: the monitor never came back in that time.

2.5.5 (commit `68340cc`) offscreen, unpacked fresh (checksums matched): 85 s of an arcade fight
at 60 fps, every frame within 1 ms, no hitches, GPU 14.83 ms at a 75 % render scale, peak memory
1,459 MB (`Saved/Perf/offscreen-2.5.5`).

2.5.4 (commit `927ed04`) offscreen, unpacked fresh (checksums matched): 85 s of an arcade fight
at 60 fps, every frame within 1 ms, no hitches, GPU 15.17 ms at a 76 % render scale
(`Saved/Perf/offscreen-2.5.4`). The first run peaked at 1,739 MB; a second with the same
fighters at 1,434 MB, and the development build used the same memory with the old and new font
cache settings (3,214 and 3,243 MB), so the first was a one-off. The font cache flushed twice at
start-up and not again in 150 s of menus and fights at 3440 x 1440 (development build).

2.5.3 (commit `c8fce5d`) offscreen, unpacked fresh (checksums matched): 85 s of an arcade fight
at 60 fps, every frame within 1 ms, no hitches, GPU 14.89 ms at a 76 % render scale, peak memory
1,448 MB (`Saved/Perf/offscreen-2.5.3`); the monitor was in standby.

2.5.2 (commit `a944464`) offscreen, unpacked fresh (checksums matched): two arcade matches over
77 s of settled play at 60 fps, every frame within 1 ms, no hitches, GPU 14.75 ms at a 69 % render
scale, peak memory 1,452 MB (`Saved/Perf/offscreen-2.5.2`). The monitor was dropping in and out of
standby (`Monitors: 0` most of the time); a full-screen run started while it was briefly back hung
in presentation when it went again, as described under Display, and was stopped. Full screen on
the monitor is not yet checked for 2.5.1 or 2.5.2.

2.5.1 (commit `27c727f`, the Fragment's tar, wind and bite changes on 2.5.0) was checked
offscreen only, as the monitor was off (`Monitors: 0`; a window run then blocks in Vulkan
presentation, as noted under Display): unpacked fresh (checksums matched), the CPU played an arcade
fight for 85 s at 60 fps with every frame within 1 ms, no hitches, GPU 14.91 ms at a 75 % render
scale, peak memory 1,415 MB (`Saved/Perf/offscreen-2.5.1`). Full screen on the monitor is not yet
checked for 2.5.1; nothing that touches the window or rendering changed since 2.5.0.

2.5.0 (commit `60bf254`, with captured footwork, block and evade, rippled water, the Roost's lit
glowing lake and contact shadows), unpacked fresh from the archive (checksums matched): 7,007 frames
over 85 s at a 12.12 ms median, every frame within 1 ms of it, 1% low 82.5, slowest settled frame
12.2 ms, GPU 10.91 ms median at a 58 % render scale, peak resident memory 1,500 MB
(`Saved/Perf/monitor-2.5.0`). The window came up full screen: 3440 x 1440 at the origin,
`_NET_WM_STATE_FULLSCREEN`. The first 5 s of the match had two frames over 25 ms (the longest
43.7 ms), as 2.4.0 had three over its two match starts.

2.4.0 (commit `d13999b`, with subsurface skin, cloth detail and the captured intros and wins):
6,358 frames over 77 s and two matches at a 12.12 ms median, every frame within 1 ms of it, 1% low
81.1, GPU 10.87 ms median at a 60 % render scale, peak resident memory 1,466 MB, no frame over
25 ms (`Saved/Perf/monitor-2.4.0`). The skin shading's cost is absorbed by dynamic resolution.

2.3.0 (commit `f156369`, with the motion-captured strikes) measured the same way: 7,008 frames over
85 s at a 12.12 ms median, every frame within 1 ms of it, 1% low 81.3, GPU 10.91 ms median at a
61 % render scale, peak resident memory 1,519 MB, no frame over 25 ms (`Saved/Perf/monitor-2.3.0`).
The 2.2.0 figures:

Shipping package of commit `1667c91`, unpacked fresh, full screen (borderless) on a 3440 x 1440
display at 165 Hz (DP, X11, Cinnamon, no variable refresh), default settings: graphics High, VSync
on, frame cap following the display (82.5 fps, every second refresh), dynamic resolution. The CPU
played an arcade fight for about 90 s (`-SlopAutoPlay=2 -SlopFrameStats`).

| Measure | Settled play |
|---|---|
| Frames, time | 7,007 frames, 85 s |
| Frame time median / p95 / p99 / max | 12.12 / 12.12 / 12.12 / 21.3 ms |
| Average FPS, 1% low | 82.5, 79.3 |
| Pacing | 99.9 % of frames within 1 ms of the median; no frame over 25 ms |
| GPU frame time median / p95 | 10.85 / 11.15 ms |
| Render scale (dynamic resolution) | median 60 %, lowest 50 % |
| Resident memory | peak 1,534 MB |

Before the frame-pacing work, 2.1.0 on the same display ran its fights at about 50 fps, unevenly
(GPU about 20 ms at full resolution on the Epic level, 33 frames over 25 ms in two minutes), and
with VSync on at 165 Hz it was held to 60 by the Vulkan CPU pacer.

### Unattended session at 1080p (2.0.0)

Shipping package of commit `f175b91` (the release differs from it only in the HUD's layout on
non-16:9 screens), 30-minute unattended session (`Tools/perf/soak.sh`, the CPU
playing arcade ladders with fighters picked at random), High preset, 1920 x 1080, frame cap 60,
rendered offscreen (see "Display" below: no presentation, no VSync). Frame times are written by the
game for every frame of every match; `Tools/perf/summarize.py` computes the figures; resident and
GPU memory sampled every five seconds. Record: `Saved/Perf/soak-shipping-20261027-032140/`.

| Measure | Settled play (all but the first 5 s of each match) | Match starts (first 5 s) |
|---|---|---|
| Frames, time | 99,105 frames, 27.5 min | 5,714 frames, 1.6 min |
| Frame time median / p95 / p99 / max | 16.67 / 16.67 / 16.67 / 17.5 ms | 16.67 / 16.67 / 16.67 / 100 ms |
| Average FPS, 1% low (1000 / mean of the slowest 1% of frames) | 60.0, 60.0 | 59.9, 49.6 |
| Hitches over 25 ms | 0 | 8 (stage and fighter loading as a fight begins) |
| GPU frame time median / p95 / p99 | 9.25 / 9.85 / 10.08 ms | 9.17 / 9.96 / 11.45 ms |
| Game thread / render thread median | 0.81 / 1.41 ms | 0.79 / 1.42 ms |
| Resident memory | 1,501 MB in the first tenth, 1,524 MB in the last, peak 1,543 MB | |
| GPU memory | about 1.0 GB (peak 1,048 MB) | |

By stage (settled play, GPU median): Rendering Yards 9.4-9.7 ms, Drowned Chapel 8.9-9.1 ms, Stilt
Market 8.9-9.0 ms, the Roost (the Fragment) 8.3-8.5 ms; every stage and matchup held 60 FPS with a
1% low of 59.9-60.0 and no hitches. 18 matches finished, the game quit by itself at the set time
(exit status 0). The GPU has about 40 % of the 16.7 ms frame to spare on the High preset, so no
lower preset was needed for the target; Low (Settings > Video > Graphics) drops point-light
shadows and volumetric fog and halves the weather particles.

An earlier 30-minute session on the Development package (uncapped, while builds ran beside it)
ran at a 6.7 ms median frame (about 150 FPS) with three frames over 25 ms; its memory figures led
to the frame-cap fix below.

### Unattended sessions since (2.5.6, 2.6.0)

Twenty minutes each of the Shipping package (`Tools/perf/soak.sh`, fighters at random), High, 1920
x 1080 offscreen, frame cap 60. Both held 60 FPS in every match with no frame over 25 ms (worst
17.8 ms) and a GPU median of 9.7-10.9 ms by match. Resident memory 1,415 MB in the first tenth
and 1,440 MB in the last for 2.5.6, 1,435 and 1,475 MB for 2.6.0; GPU memory about 1.0 to 1.06 GB
in both. The rise follows which fighters and stages a run happens to load (per-match peaks 1,432
to 1,488 MB), so these runs show no leak but cannot rule out a slow one: that needs one matchup
repeated for longer. Records: `Saved/Perf/soak-2.5.6-20261028-001904/`,
`Saved/Perf/soak-2.6.0-20261028-010056/`.

That longer run, on 2.6.2: 30 minutes of one match over and over (`SOAK_MATCH=bram,mirela,stilt_market
Tools/perf/soak.sh`, 14 matches, Bram against Mirela at the Stilt Market, CPU on both sides).
Resident memory rose from 1,517 MB to 1,565 MB in the first seven and a half minutes (caches,
shaders and sounds filling) and then held at 1,565-1,567 MB for the remaining 22 minutes, about
2 MB an hour: no leak. GPU memory settled the same way, at 1,006 MB. Its frame times are not a
record: screenshots were rendered in the editor on the same GPU during it (370 frames over
25 ms, where the 2.6.0 session without that had none). Record:
`Saved/Perf/soak-2.6.2-same-20261028-111359/`.

## Packaging

`package_linux Shipping` cooks with 0 errors and 0 warnings. `Tools/make_release.sh Shipping`
wraps it with the readme, licences, version and checksums; the archive was unpacked into a fresh
folder and run from there with the editor closed (below).

**2.7.0** (`EbonyHearts-2.7.0-linux-x86_64.tar.gz`, commit `1ae0433`): built in 3 minutes, the
cook with 0 errors and 0 warnings; the archive's checksum and every file's in `SHA256SUMS` verified
after unpacking into a fresh folder; `VERSION.txt` names the commit; the launcher is
`EbonyHearts.sh`. With the editor closed, `Tools/perf/soak.sh` ran the unpacked game for a minute
on the Grandmaster against Wren at Master (CPU against CPU, the Drowned Chapel), offscreen because
X reported no monitor (the display had gone to sleep in the night): 60.0 fps throughout the
settled play (median, p95 and p99 frames 16.67 ms, every frame within 1 ms of the median), GPU 10.3
ms, 1.52 GB resident (flat from first to last tenth), 943 MB of video memory, quit by itself at the
set time (`Saved/Perf/pkg-2.7.0-20261029-043419/`). Launched without `-RenderOffscreen` while X
had no monitor, the window waited forever to present, as described under "Display" below; that is
the display, not the build. Not yet checked with the monitor on: the package's menus, sound and pads.

## Display and what offscreen means

For the 2.0.0 session above FLOWBOX's monitor was switched off (X reported no monitors), and a
Vulkan window cannot present without one, so the game was run with Unreal's `-RenderOffscreen`:
the same renderer and GPU, rendering every frame at 1920 x 1080, without presenting it. In that
mode SDL uses its dummy video and audio drivers, so pads and sound are not available. The 2.2.0
figures were taken on the real display, presenting with VSync. On that display (3440 x 1440, 21:9)
the menus keep their 16:9 layout centred, the fight camera fills the width and the HUD keeps its
16:9 width; switching to a window and back in Settings (with the keep-or-revert prompt), and F11,
were checked.

## Found and fixed during validation

- The fight camera's view was 30 degrees tall only at 16:9: Unreal keeps the vertical angle a
  horizontal field of view gives at the camera's own aspect (16:9 by default). At 4:3 both
  fighters were cut off at the edges; at Markus's 3440 x 1440 they were drawn a quarter smaller
  than the framing meant. Found rendering 4:3 for 2.6.1; the camera now carries the screen's
  aspect, and 16:9 is unchanged.
- The skin's new marks leaked red specks onto unhurt fighters before 2.6.0 shipped: noise with
  two octaves runs past its 0-1 range, so a mask thresholded at 1 still showed. Clamped.
- Every canvas-placed menu was shifted (the engine centres canvas slots by default).
- Two material connections had silently failed (the noise in the stage glows, a vertex colour).
- Stage meshes lost their materials (changes made in the editor session that imported them do
  not hold; slots are now assigned in a second session).
- Sounds loaded on first play mid-fight (now streamed in at start-up).
- Training's boxes are drawn by the HUD (debug drawing does not exist in Shipping).
- From the independent review: R1 could not change the dummy on a pad (the right stick's click
  does); angled stick input changed values in menus; pad rebinding accepted directions; saves
  were not atomic; music stayed paused after Restart; a pad connecting mid-fight dropped everyone's
  held inputs; text size and vibration did nothing; the right-hand keyboard player had no Back.
- An offscreen run with no frame cap let the driver's memory grow to 7-10 GB within a minute; the
  frame cap now always applies, and the capped game holds about 1.4 GB on the title screen.
- On Markus's monitor (Cinnamon, X11) the game came up as an ordinary decorated window while the
  engine recorded full screen: the window manager misses the request the engine makes while the
  window is hidden. The game now checks the window against its display after start and asks again.
- On a 165 Hz display VSync'd frames were held to exactly 60: the Vulkan RHI's CPU frame pacer
  assumes 60 Hz on Linux. It is off; the frame cap follows the display.
- At 3440 x 1440 the GPU needed about 20 ms a frame on the old Epic level (about 50 fps, uneven).
  TSR's history at twice the output size cost 3.5 ms alone; it now stays at output size, High uses
  the engine's High level, and dynamic resolution holds the frame rate.
- Since the port the hit-reaction springs had pitched a struck head toward the attacker before it
  rebounded; they now drive it away (a test holds every blow direction).
- Hands were placed in the fighter's root space, so they hung where the pose put them while the
  body bobbed, leaned or recoiled; they now move with the chest (the arm carrying a blow excepted).

## 2.8.0: Story mode, the bosses and the Grandmaster rebuilt

141 tests pass, among them `Fighters.Hollis.VesselCanBeBrokenAndKillsIfNot`,
`Fighters.Fragment.EnergyWarnsFormsFlowAndWrenHurtsItMost`, `Flow.StoryIsWrensWayToTheFragmentAndTheGrandmasterOnce`,
`Anim.HairHangsAndTrailsTheBody` and `Content.HalfJointsSitBetweenTheirBones`. Story mode was played from the
main menu with the CPU as Wren (`Tools/playtest/run_flow.sh story`, then `story_end`), through five fights,
Hollis, Ashka's half of Hollis, the Fragment, the Grandmaster, the ending and the credits. That run found the
boss clock timing Hollis out and FINISH HIM offered on him; both are fixed. Nobody has played any of this by
hand: how hard the bosses are, whether Hollis's punish windows feel fair and how the forms read in motion are
all unjudged. His forms and the Fragment's skin were checked in stills, not in a long look at motion.

The Shipping package held a locked 60 fps with no hitch in the Fragment's fight (`soak-2.8.0-fragment`, 70
seconds, 1.69 GB). The worm's fight and Hollis at the Palace door ran there too (`soak-2.8.0-worm`,
`soak-2.8.0-hollis`, the second as an ordinary match, so without the boss rules), and Story mode's first
three fights (`soak-2.8.0-story`). Its boss fights and ending were played only in the editor build: Shipping
has no log and `-SlopStoryFrom` does not take effect there.
Every new asset was found by name in the package's IoStore index (forms, worm, material, both stages).

## 2.9.0: Wren Ku to her sheet

142 tests pass, `Fighters.Wren.SpiritStrikesHarderAndHerSheetMovesExist` among them. Her five outfits were
built and looked at in renders, MMA gear was chosen through the real select screen (`Tools/playtest/run_flow.sh
outfit`), and the uniform is worn from the Gorm fight in Story mode. At 12 seeds Wren wins 40%, 23%, 50%, up from
35%, 27%, 42%. The 720 Hook Kick, the spinning kicks and Punt were not looked at in motion; by a human, nothing was.

## 2.10.0: Ashka Rook to her sheet

143 tests pass, `Fighters.Ashka.HerSheetMovesExistAndHerFlameIsCrimson` among them. Story mode is one rung shorter
(she no longer fights Wren); its test holds that. Her fight, her flame pose and her portrait were checked in stills.
Nobody has played her by hand, and the balance figures above predate this change.
Her tattoo's glow is confirmed bound to her coat's material but was not seen lit: her arm covers it in the side-on
views captured. The Shipping package's soak (`soak-2.10.0`, Ashka against the Fragment) is in `Saved/Perf`.

## 2.11.0: Mirela Sorrow to her sheet

144 tests pass, `Fighters.Mirela.KeepsHerArtsAndPrays` among them. Her moves' data is untouched, though that hid a real
change: shared moves scale with height, so her basics lost 9% of their reach until 2.14.0 restored it. Her habit, her prayer
at the round's start and her portrait were checked in stills from a real match in the Drowned Chapel. Her win uses the
same pose and was not captured. Nobody has played her by hand, and her height changed (1.82 m to 1.65 m) with her frames
kept, so the balance figures above predate it. Below the veil her hair trailed as a flat ribbon, thin when seen side-on, until 2.14.0.
The Shipping package's soak (`soak-2.11.0`, Mirela against Ashka in the chapel, 180 seconds) is in `Saved/Perf`:
60 fps, no hitches in settled play, peak 1667 MB resident, 983 MB of GPU memory, 12.2 ms median GPU time.

## 2.12.0: Bram Ketch to his sheet

145 tests pass, `Fighters.Bram.KeepsHisMovesAndHisTattooCanBeSeen` among them. His moves' data is untouched and his push
width and throw range are pinned to their old values, because at 1.91 m they shifted his spacing and failed two CPU
tests. His hit volumes still follow his height, 2.7% up. Both outfits were checked in stills from a real match against
Gorm; the tattoo is legible in a close render and in-game reads as dark lettering on the arm, as P1. Nobody has played him
by hand. `AI.JabbedBramParriesWithoutLosingHisGuard` needed sixteen minutes of play in place of four to find a parry at
all, and this page said the CPU's Stand Down catches 1 try in 240. That was the drill, not Stand Down: see 2.14.0.
The Shipping package's soak (`soak-2.12.0`, Bram against Gorm on the Stilt Market, 180 seconds) is in `Saved/Perf`:
60 fps, no hitches in settled play, peak 1315 MB resident, 944 MB of GPU memory, 9.8 ms median GPU time.

## 2.13.0: Gorm Tallow to his sheet

146 tests pass, 274 of 274 counted moves covered. `Fighters.Gorm.KeepsHisArtsAndGainsGrapples` plays each of his four new
moves through the simulation: each lands and both fighters recover. All four were filmed in motion against Bram in Training
(`-SlopMoveCycle`), the Overarm Throw again after its path was lowered. Breaking the two new grabs was not tested by hand.
His new face, gear and portrait were checked in renders and in the Rendering Yards. Nobody has played him by hand.
The Shipping package's soak (`soak-2.13.0`, Gorm against Mirela in the Rendering Yards, 180 seconds) is in `Saved/Perf`:
60 fps, no hitches in settled play, peak 1630 MB resident, 986 MB of GPU memory, 11.0 ms median GPU time.

## 2.14.0: blows, the standings and her hair

146 tests pass, 274 of 274 counted moves covered. The six at 12 seeds a pairing (60 matches a level), on this build and on
2.13.0:

| Level | 2.14.0 | 2.13.0 |
|---|---|---|
| Novice | Bram 67, Mirela 67, Nix 63, Gorm 55, Wren 47, Ashka 38 | Bram 70, Gorm 70, Nix 70, Wren 57, Mirela 42, Ashka 40 |
| Warrior | Bram 68, Nix 67, Mirela 65, Gorm 48, Ashka 37, Wren 25 | Nix 72, Bram 70, Gorm 63, Ashka 45, Wren 35, Mirela 15 |
| Master | Bram 83, Ashka 63, Wren 57, Nix 45, Mirela 32, Gorm 17 | Bram 82, Ashka 68, Wren 57, Nix 47, Gorm 27, Mirela 17 |

They are still uneven: Bram leads every level, Gorm is last at Master and Wren last at Warrior. Nothing was tuned
for that. Each fighter's total moves with the others', so most of the falls from 2.13.0 are Mirela winning her matches again.

**Mirela had collapsed, and her height was why.** Shared basic moves scale with `stats.height`, so going from 1.82 m to 1.65 m
took 9% off the reach and push of her basics and off her throw range, while her own moves, which load at scale 1, kept theirs.
With only her height put back she measured 58/43/28. `stats.strike_height` now holds her basics at 1.82 m on her 1.65 m body,
and her throw range is pinned there. So her blows reach further than her limbs by design, by as much as 9%. Her heavy kick's
box comes down to meet the leg (`Anim.StrikesReachTheirHitboxes` caught that), its reach and top unchanged. Her push width
follows her body: pinned as well, it held Wren's trial LK, HK, D,U+LK just out of reach.

Heights moved others the same way: Wren up 6%, Bram 2.7%, Ashka 2.3% down. Those stay. Bram's blows were tried at 1.86 m: he
won 31 of 40 at Master, within the noise of his 82%, and `AI.PunishesWhiffsWithBlowsThatReach` failed (37 of 173), so it was
reverted.

**Stand Down was never broken; the drill was.** The 1 try in 240 recorded under 2.12.0 is wrong. Ashka jabbed from under 1.0 m,
her jab reaches 0.95 m and pushback left her at 0.97 to 0.98, so nearly every jab fell short and there was nothing to catch. Jabs
that do land are blocked, and only clean hits arm the read, so a drill of one jabber cannot exercise it. In whole CPU matches
(40 a level, his blows at 1.86 m, the read unchanged):

| Level | Tries | Caught | Struck during it | Nothing came |
|---|---|---|---|---|
| Novice | 31 | 3 | 10 | 18 |
| Warrior | 72 | 10 | 28 | 34 |
| Master | 67 | 28 | 20 | 19 |

The read was left alone. `AI.MasterBramReadsBlowsWithStandDown` replaces the drill with 20 Master matches: 11 of 30 tries catch
a blow, and he is hit no more often for trying (460 hits in 121,663 ticks, against 496 in 128,680 without the read).

**Her hair.** Widening it did nothing side-on, where the fight camera sits, so it now arches well off her back from just below
the nape. Checked in stills from a real match against Bram: a broad fall that fans at the tips as it swings. The portrait's rim
light catches the arch's crown, a pale shape at the frame's left edge. It was there in 2.11.0 and is larger now; left as it is.
Nobody has played any of this by hand.
The Shipping package's soak (`soak-2.14.0`, Mirela against Bram in the Drowned Chapel, 180 seconds) is in `Saved/Perf`:
60 fps, no hitches in settled play, peak 1664 MB resident, 990 MB of GPU memory, 10.2 ms median GPU time.

## 2.14.1: every character checked against its sheet, in the game

Asked whether every character was updated cleanly, the eight sheets were read line by line against the data, then every
body and outfit was captured in the game. The sheets' names, heights, titles, styles and move lists were right; the rest was
not, and the data alone could not have shown most of it.

Fixed:

- **Wren's hair.** It stopped well above her nape and round her ears, a shaved patch, not the sheet's long hair: the cap was
  cut where the head's skin weight gave out. It now runs to the nape and close round the ears, with both braids kept.
- **Skin on the covered fighters.** One gain over what clothes left of a body put the faces far above their look colour:
  Mirela's cheek measured 253, 252, 245 against a target of 228, 208, 196, the Grandmaster's 254, 234, 205, Wren's 245, 221,
  193, against arms at the target. The map now keeps its detail and takes the look colour for its broad tones. Wren's cheek,
  forehead and hand measure 230, 192, 166; 232, 195, 170; 231, 195, 168 against 230, 192, 166. Ashka's are 158, 107, 71;
  155, 106, 71; 156, 105, 69 against 156, 106, 72. The review renders' rim light leaves a pale band on the jaw that is not in
  the map.
- **Ashka's tattoo.** It was a flat piece sinking into a curved coat, shown in broken pieces. It now follows the coat in the
  shape of a cross with a lamp at its heart, on a small slot of its own. A coat glow map and a cloth one from older builds
  had stayed on her materials: the build now removes a map it no longer makes, and the import clears a slot's parameters
  before it sets them.
- **Ashka's face strands.** Kinked bars standing off her head; they now come out of the hair, follow the face and taper.
- **Bram's mutton chops.** Darker than his hair and running down onto his neck; now his hair's colour and ending along the jaw.
- **Hollis's health.** His data went from 1080 to 800. That changed nothing in a real fight, as 2.14.3 below records.
- **Old names.** The tutorial still taught Ember Shot, and 17 trials used names from before the sheets. Saves record trials
  by name, so a cleared trial is carried to its new name on load (tested). The face-off and stage select showed the ids
  `heartwater` and `palace_gate`. The Records screen still named the Roost for the stage it now unlocks.

146 tests pass on the editor build. Shipping package: `EbonyHearts-2.14.1-linux-x86_64.tar.gz`, built from `9e51d4d`,
checksum verified. Soak, Wren against Ashka in the Drowned Chapel, 180 s offscreen: 60 fps, 1% low 60, no hitches over 25 ms,
peak 1594 MB, 983 MB of GPU memory, median GPU 10.3 ms.

Captured in the game at 1080p, standing, on the editor build: Wren in all five outfits, Ashka, Mirela, Nix, Gorm, Bram in both
outfits, the Grandmaster, Hollis, the Fragment, and the worm the Grandmaster sees. Ashka and Nix were also captured fighting.
Portraits redone for Wren, Ashka, Mirela, Bram and the Grandmaster.

Not seen: **Ashka's tattoo lit in a match.** Its cross is checked on the model; she never threw the flame facing the camera in
the minute or so filmed. It sits on her left chest, so it can only show when she faces screen left. Her coat belt shows a
rough edge at the back, left as it is. Gorm, Nix, Hollis and the Fragment were not rebuilt: nothing here called for it, and
Gorm's and Nix's skin keep the old toning until they are. Nobody has played any of this by hand.

## 2.14.2 and 2.14.3: what 2.14.1 left open

The list 2.14.1 ended on, worked through.

- **Hollis as a fight.** Measured for the first time in whole matches, first to two rounds, 96 a level: the six won 96, 100 and
  100%. He never guarded, so he could be hit freely between sequences, and he had no answer to blocking. He now guards between
  them at 30, 60 and 85% of blows by difficulty, never in a recovery, so his sheet's punish windows stand, and he throws whoever
  stands close and only guards. His rests are shorter at Warrior and Master.
- **Hollis's health: a correction.** 2.14.1 said he had 800 health, and 2.14.2 raised his defence to 52% of damage taken on that
  figure. Both were wrong: boss fights replace the data's `health` with `FGameFlow::ServantHealth`, then 1000, 1100 and 1200, so
  800 reached no fight, and the 2.14.2 package has a Hollis who outlasts the Fragment at Master and was never measured. Do not use
  it. In 2.14.3 the table itself is 800, 900 and 1000, he takes 60% of damage, the Vessel breaks at 111 (the same blows as
  before), and `AI.HollisIsAFightForTheSix` and the guardian test read the table, not the data.

How often the six win, one round, the health each boss has in the game, 72 matches a cell:

| Fighter's level | Hollis Novice | Hollis Warrior | Hollis Master | Fragment Novice | Fragment Warrior | Fragment Master |
|---|---|---|---|---|---|---|
| Novice | 75% | 9% | 0% | 20% | 1% | 1% |
| Warrior | 100% | 93% | 43% | 86% | 45% | 15% |
| Master | 100% | 100% | 98% | 100% | 81% | 51% |

He scales with his difficulty and is easier than the Fragment in every cell but Novice against Master, where neither loses (0%
and 1%). A Master CPU still beats him nearly always, with 65% of its health left: it reads every recovery to the frame, which is what a pattern boss
asks of a person. These are CPU fighters; no person has fought him at these numbers.

- **Ashka's belt.** It was a shell of the layers under it, so it took their folds and stepped at the front, and the coat's hem
  showed under it at the back. It is now a smooth tapered ring and the hem ends behind it.
- **Ashka's tattoo, lit in a match.** Filmed from the side that shows it (`-SlopMoveCycleFlip`): it lights with the flame, but in
  the fight camera it is a sliver of crimson at the front of her chest, the raised arm over the rest. Only its edge has been
  seen in the game; the whole cross only on the model.
- **Mirela's portrait.** The pale shape behind her was the top of the arch of her hair taking the rim light. The arch now rises
  over a slope and her hair is more matte; a dim edge of it remains.
- **Gorm and Nix** are rebuilt on the new skin toning, with new portraits.

147 tests pass. Shipping package: `EbonyHearts-2.14.3-linux-x86_64.tar.gz`, built from `c6ea7f7`, checksum verified. Soak, Mirela
against Nix on the Stilt Market, 180 s offscreen: 60 fps, 1% low 60, no hitches over 25 ms, peak 1596 MB, 986 MB of GPU memory,
median GPU 11.5 ms. Nobody has played any of this by hand.

## 2.15.0: Nix Hollow to his sheet

Built to his character sheet, with one ruling from Markus beyond it: as the fastest, with the easiest combos, he has
the lowest damage. `Fighters.Nix.FastestAndHitsLightest` holds all of it: he out-walks and out-runs every fighter who can be picked,
no first blow of theirs starts sooner, and each of the twelve shared blows does less for him than for anyone. (The Grandmaster
in Nix's shape copies that: 0.88 times his usual 1.15. His rule for copying is unchanged.)

148 tests pass, 281 of 281 counted moves covered. Trials cover the new strings. The six at 12 seeds a pairing, 60 matches a level:

| Level | 2.15.0 | 2.14.0 |
|---|---|---|
| Novice | Mirela 72, Gorm 62, Bram 62, Ashka 45, Wren 45, Nix 33 | Bram 67, Mirela 67, Nix 63, Gorm 55, Wren 47, Ashka 38 |
| Warrior | Bram 75, Mirela 67, Gorm 57, Nix 38, Ashka 35, Wren 27 | Bram 68, Nix 67, Mirela 65, Gorm 48, Ashka 37, Wren 25 |
| Master | Bram 83, Ashka 65, Wren 55, Nix 43, Mirela 35, Gorm 20 | Bram 83, Ashka 63, Wren 57, Nix 45, Mirela 32, Gorm 17 |

The first build had him at 18, 13 and 18%. Measured one change at a time at Power 0.85: full reach gave 32, 38 and 45; full damage
gave 37, 35 and 35; the CPU's new combos and specials made little difference. Pinning his reach at his old 1.78 m had been the larger
fault: a bigger target with a shorter fighter's reach. He now reaches as 6'4" does, with Power 0.88, Sinkstep's distance kept as
tuned, and his throw range and push width where they were. Undertow's hold went from 52 frames to 46: at the new proportions it looped
for 16 hits. These are CPU figures. His speed and easy strings are worth more to a person than the CPU makes of them, and that is
not measured. The six are still uneven, Bram above all; nothing was tuned for that here.

Captured in the game at 1080p: Nix standing, and the Code Sea draw, cuts, Hypnosis, kunai and shuriken from both sides. That found one
bug, older than Nix: facing left a pose is mirrored onto the other arm and a held weapon did not follow, so Hollis's greatsword had been
in his off hand for most of every fight. Fixed, and seen right on both of them from the left. The hilt leaves his back while Code Sea
is out. The move's blade is in his left hand facing right; facing left it is in the sickle hand, by that same mirroring.

Package: `EbonyHearts-2.15.0-linux-x86_64.tar.gz`, built from `d6a704f`, checksum verified. Soak, Nix against Ashka on the Stilt Market,
180 s offscreen: 60 fps, 1% low 60, no hitches over 25 ms, peak 1654 MB, 995 MB of GPU memory, median GPU 14.8 ms.

Not seen: Illusion in a close capture; the Grandmaster wearing Nix's shape with the new moves on screen (they are covered by test);
Nix's finisher at the new height. Nobody has played any of this by hand.

## 2.16.0: the interface, on the title card

Commit `3e7b3eb`, Shipping archive `EbonyHearts-2.16.0-linux-x86_64.tar.gz` (619 MiB, SHA-256
`61fdffa65d55d1db8f7560c60c83ea72786a8e4906e5bff98444a5909078ef41`). Design and reasons are in
`Docs/UI.md`; labelled frames are in `Docs/Evidence/UI-2.16.0`.

**Tests.** 148 before the work, 160 after, all passing on the final build: twelve new (`Slop.Ui.*`,
`Rounds.HeldIntro*`, `Save.PreFightIntroSettingIsSafeInOlderFiles`). They hold the design canvas on eight
screen shapes, the card's source file and size, every interface texture, the fonts and their licences,
what each kind of fighter's meters read from the simulation (run; Qhi with its half mark; borrowed run
and Qhi; spent with its recentring mark; the Vessel), and that a held or skipped intro leaves a scripted
600-tick fight identical to the last hundredth of a metre.

**Flows, on the final build unless noted (editor, offscreen).** Tour at all five shapes, 11 screens each.
Training, Trials, Tutorial and Survival on the final build; Versus, Extras and Settings one build earlier;
Versus CPU, Story, Story to its ending and credits, outfit and training options two builds earlier. The
Extras and Settings flows had been stale since Story was added to the menu, as the baseline's logs show
(they landed on Settings and on Quit); they reach their screens now. An unattended packaged session ran
300 s and exited clean.

**Controllers.** No physical pad was used. `vpad.py --xbox` (a kernel virtual pad) played the game by
pad alone on the desktop, twice: title, menu, select, tower, the staging skipped from the pad, a fight, a
simulated cable pull (the fight paused, "Controller disconnected"), reconnection, pause menu, main menu,
quit. The chips read `A` on the Xbox run and `✕`, `○`, `R3` with Glyphs set to PlayStation. Two simultaneous
sides were driven by the keyboard script (`confirm@1`), not by two pads.

**Screens seen.** Every frame judged by eye was a real capture, at 1920 x 1080, 2560 x 1440, 3440 x 1440,
1920 x 1200 and 1440 x 1080, and 4:3 again at Text size 125% with Effects Reduced. Only the 1920 x 1080
packaged run below was on the physical display; the rest were offscreen. Special states seen in frames:
Grandmaster in his own form, borrowed and spent, the Fragment, combos and low health, every result type
reachable by `-SlopResults`, Continue, an ending, Training, Trials and Tutorial.

**Packaged, on the real display.** The Shipping package in a 1920 x 1080 window, driven by the Versus flow
and recorded from the window at 20 fps (1,462 frames): select, stage, the ground, both close-ups, the two
shot, round one, fight, pause, menu, all in order and with no blank frame. This run was of `7e5ed02`; the one
commit since only stops a clear curtain being drawn.

**Performance.** Shipping, offscreen, 3440 x 1440, the same repeating Ashka v Gorm match, 150 s, 2.15.0
then 2.16.0 back to back:

| | 2.15.0 | 2.16.0 |
|---|---|---|
| Frame time median, p99 | 16.67, 16.67 ms | 16.67, 16.67 ms |
| Hitches over 25 ms (settled) | 0 | 0 |
| Game thread median | 0.97 ms | 0.97 ms |
| Render thread median | 1.61 ms | 1.59 ms |
| GPU median, p99 | 14.57, 17.22 ms | 14.56, 17.21 ms |
| Render scale median | 67% | 67% |
| Peak process memory | 1707 MB | 1738 MB |
| Peak GPU memory | 1488 MB | 1489 MB |
| First 5 s of each match: hitches, worst frame | 4, 516 ms | 6, 423 ms |

Those first seconds cover the load, which the curtain now hides, and the first frames after it; the game
and render threads' p95 there rose (4.2 and 9.8 ms against 1.2 and 1.9). Settled play is unchanged.

The first 2.16.0 package settled at 56% render scale where a 2.15.0 run had reached 68%, with the same GPU
time. Runs at a fixed 100% scale put old and new level (21.1 and 21.9 ms against 22.0 and 21.8), and
after the clear curtain stopped being drawn both builds came out at 67%. Whether the curtain or
dynamic resolution's own settling made that difference was not isolated.

**Defects found by review.** An independent read of the diff found six, all fixed: a Continue countdown
running out during the fade into a retry could start a fight with no ladder and record a false tower
clear (a screen being left no longer ticks); a mouse click during a fade could do the same (leaving by
another way cancels the fade); the handover to the fight jolted (`RoundStart` snapped the camera, and a
screen change dipped the curtain over the match); intros never restarted for their close-ups
(`SetState` ignores a state already held); punches stopped skipping the face-off; and two timers had moved.
Screen review found: the tower showing a different plan from the one climbed; wrapped text overlapping
off 1080p (`SStage` reported no layout scale); the foot gradient and lower letterbox bar not drawn (a size
passed as an offset); the fighter name cards invisible. An ending's first page had never shown, in any
earlier build either: its text was read after the screen was built.

## 2.16.1: the retired names

Game commit `17f243a`, archive `EbonyHearts-2.16.1-linux-x86_64.tar.gz` (SHA-256 `3d985804d7b5372cc9bdc701b4f52732592f51b9eea37381670ce4c624dd64c7`, checked). 161 tests
pass, one new: `Slop.Content.NoRetiredNamesReachThePlayer` reads the story, the trials, every fighter's file,
every string in the game's code and the shipped readme and licence, with controls that it would catch the old
words and leave identifiers and Slopperfall alone. Its first run caught its own pattern matching `MortalSlopbat`.

Checked in the release itself, not only the source: the binary's strings hold no retired name but the internal
`sloptality` sound key; `UnrealPak -Extract` of the shipped pak gave 58 data files with no retired name, and
the new reflection line; the readme and licence beside the game are clean. The window's title is Ebony Hearts, and
its class is `EbonyHearts` (the launcher sets `SDL_APP_ID`; the class's instance part stays the binary's name).

Seen on screen: `HEARTED` over a real finisher (Bram's Lights Out, `Docs/Evidence/UI-2.16.0/21_hearted_2.16.1.jpg`)
and on its results, `Stage Hearted` by `-SlopResults`, and the move list's section; the renamed Records line was
not captured in a frame, and is covered by the scan. The sound is drums and brass with no voice, confirmed from
its generator, so nothing audible says the old word. `UnrealPak -Extract` on Linux joins its path with a backslash:
copy the pak beside its folder as a file literally named `<folder>\<name>.pak` first.

## 2.17.0: a 4K TV, every speaker layout, a controller's own buttons

167 tests pass. The release's archive and checksum are recorded at the end of this section.

**Speakers.** `Tools/playtest/speaker_check.sh` plays a real packaged CPU fight into a virtual output of each
layout, with a stand-in `pactl` naming that output as the default, so the automatic choice runs as a player's would.
22 s each, RMS / peak in dBFS. No layout clipped a sample, in any of the final runs.

| Output | Mix | Levels |
|---|---|---|
| Stereo: TV speakers, headphones, most soundbars | Stereo | FL, FR -24.9 / -3.5; again -24.5 / -2.9 (2.16.1: -23.2 / -2.5) |
| 2.1 | Stereo | FL, FR -23.1 / -2.7; LFE silent, as a set with its own crossover expects |
| Mono | Stereo | -33.4 / -11.7: the sound server's fold, about 9 dB down, as it was |
| Quad | Surround | Fronts -24.5 / -2.4; rears -44.1 / -31.0 |
| 5.1 | Surround | FC -26.0 / -3.3; FL -32.4 / -9.1, FR -35.5 / -11.4 (they follow the fight); LFE -38.4 / -15.8; rears -44.1 / -31.0 |
| 7.1 | Surround | As 5.1, FC peak -1.7; the side pair silent, because the engine renders 5.1 |
| Surround forced onto stereo, onto mono | Fold-down | -24.0 / -1.5; -35.2 / -14.1 |

The rears are the music alone, so they read low beside a fight's blows. Stereo was the path players already had: it is unchanged
but for the limiter, which caught a real overshoot there (one run reached full scale, 81 samples, before the limiter worked).

These runs found five faults that no unit test would: the first detection believed SDL, which reports the six channels the
engine opened, so every stereo output took the surround mix; the limiter was added before a world existed and never reached the
mixer; once it did, the engine's defaults (a 10 dB knee, analog response) let blows through 3 dB over it; the rears played 7.5 dB low,
a fade's level multiplying the voice's own; and a stream moved in its first moments can fall back to the real output, which
left two layouts silent and sent their sound to the default device until the script learned to move it until it stays. The limiter sits at
-3 dB because a fold can add about 1.8 dB back, and placed sounds are 3 dB down because one speaker carries what stereo shares.

**The real TV.** Hisense 58R6+ on HDMI, 3840 x 2160 at 59.94 Hz, RTX 2060, Shipping, nothing capturing. Chosen in Settings, then reached
by itself on the next launch (about 3 s of desktop first). The floor is 45% at 4K, up from 40%, which left GPU time unused:

| | Frame median | Average | GPU median, p95 | Render scale | Over 25 ms |
|---|---|---|---|---|---|
| 69 s, Rendering Yards, Master CPUs | 16.68 ms | 59.8 fps | 13.46, 13.89 ms | 45% throughout | 10, all in the first 32 s, none after |

Those ten have game, render and GPU times inside the budget. An offscreen run at 50% missed 60 (GPU 16.3 ms), so 45% is the top that holds. Dynamic resolution
stays at the floor there, though about 3 ms is spare. A run's 1 fps full-size capture added hitches, so menus were judged from frames only. On
screen: 5% margin holds the design inside and the world still reaches the edge; the longer `Grandmaster Tower` fits its card and `GRANDMASTER` its Records column;
`Automatic (stereo)` on Sound; move lists, tutorial and training read clean at full size. Stage pictures are 2160 x 1215, one to one at 4K.
The link is YCbCr 4:2:0, which the capture cannot show: fine coloured text may be softer on the panel than in these frames.

**The TV's sound.** Its EDID offers LPCM on two channels only (and AC-3, E-AC-3, TrueHD, which a game's PCM is not), so on its own it is the stereo
mix. The default output is still the monitor: `Digital Stereo (HDMI 2)` must be picked in the system's sound settings, which is the player's to change.

**Buttons.** Real DualShock 4 over Bluetooth, connected before launch, nothing pressed: the title, menus, Settings, Extras, move lists, tutorial and training all
showed its buttons (`✕ ○ △ □`, `R1`, `R3`, `Share`), and `Button prompts` cycled Automatic, PlayStation, Xbox and PC with the page's prompts changing live.
The move list read `↓ → + △`, `Y` and `U` in the three styles. `Input.ButtonPromptsFollowTheChoice` and `Ui.MoveNotationShowsTheButtonsInUse` hold it.

**The release.** `v2.17.0` on the repository's Releases page: `EbonyHearts-2.17.0-linux-x86_64.tar.gz`, SHA-256
`cc053fc77aa5635a786f0e38ce21e72a2264855daf763138ba197f27c6597e8f`, Shipping build of commit `e14d41f`. The 167 tests passed on that
branch's tree, packaged from it, before a last pass over documents and one comment that changed nothing under `Source`, `Config` or
`Content`. Every file in the archive matched its `SHA256SUMS`. Unpacked fresh and started through `EbonyHearts.sh` on an empty save
folder, it went from the title through the menu into a CPU fight for 70 s: frame median and 99th percentile 16.67 ms, exit 0. That run was
offscreen, so it saw no pads or sound. Then from GitHub with no credentials: a fresh clone of `ue5` fetched all 1253 LFS files, fsck clean, and
the downloaded archive was byte-identical to the local one.

The public branch carries 2.8.0 to 2.17.0 as one commit, so commits named from 2.8.0 on, here and in `Docs/Evidence`, are not in its history.

## Not verified

- DualSense, USB connection, rumble and reconnection on a real pad, two physical pads (above).
- Windowed presentation timings with VSync (above).
- Audio by ear: levels, mix and the layering were checked in code, logs and measurements only. The surround mix has
  never been heard, and no real multichannel output has run it: virtual outputs show what the game sends, not how a room sounds.
  2.16.0 adds no sound, but the stage's music now starts with the staging, where the face-off used to play the select music.
- Pressing buttons on the real pad in 2.17.0: it was seen connected and named, and Markus played that pad on 2.1.0.
- Other aspect ratios, and any full-screen mode, on the real display for 2.16.0: see the section above.
  Nobody has played 2.16.0 by hand.
- Mouse use of the new screens: hit areas and handlers were read, never driven.
