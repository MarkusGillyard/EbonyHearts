# Validation record

What was checked before release, how, and what was found. Everything here was run on one
machine by the developer's tools; no separate review or outside playtest took place.

| | |
|---|---|
| Version | 1.0.1 |
| Release commit | `c0bb646e22f866c83fa995e060baaa9da06657a7` (tag `v1.0.1`) |
| Archive | `MortalSlopbat-1.0.1-linux-x86_64.tar.gz` (68,346,209 bytes) |
| SHA-256 | `ba6b93fe624adf72b2ea85f98c14c866e3a6a6e9125da31c66d1ecc767d17984` |
| Machine | Linux Mint 22.3 (X11, Cinnamon), kernel 7.0, AMD Ryzen 7 5700G (8 cores, 16 threads), 14 GB RAM, NVIDIA GeForce RTX 2060 (driver 595.99), 3440×1440 display at 100 Hz |
| Engine | Godot 4.7.2-stable, Vulkan Forward+ |

## Not covered

- **Physical gamepads.** Controller support was exercised with simulated input only: synthetic
  Godot events in the tests and a virtual Xbox 360 pad created through Linux `uinput`
  (`tools/playtest/vpad.py`) in the versus playtest. No physical controller was connected.
- **Listening.** Nobody listened to the sound or music. The game's audio stream was measured
  (see below), which shows it is producing unclipped sound, not that it sounds good.
- **Other systems.** Only the machine above: no Wayland session, other distribution, AMD or
  Intel GPU or 60 Hz display. The OpenGL 3 fallback was started into fights, not played through.
- **People.** All play was by scripts and a simple automatic player driving the real game window
  with X11 keyboard events (XTest). It shows that the game works end to end; it says little about
  how it feels to a human, how readable it is at speed, or how fair the CPU seems.

## Automated tests

`tools/run_tests.sh` runs 61 headless tests (fails on any failed assertion or script error); all
pass, run repeatedly on an isolated test profile:

| File | Tests | Covers |
|---|---|---|
| `test_combat` | 22 | Motions from either side and across crossovers, input buffering, strike timing and reach, block levels and chip, trades, throws, breaks and command grabs, chains and the run meter, juggles and damage scaling, projectiles and clashes, determinism |
| `test_rounds` | 12 | Knockouts, the dizzy FINISH and finishers by range, timeouts and draws, double knockouts, matches that cannot loop forever, restart clean-up, parry, armour, teleport, the boss's health-gated storm and throw immunity |
| `test_stranger` | 4 | The stranger's memory stages and responses |
| `test_flow` | 9 | Arcade ladders (rival, Reflection, boss), continues, the flawless-win offer, saves (atomic, versioned, damaged-file recovery, test profiles kept apart), settings, device mapping, the tutorial's blocking steps |
| `test_animation` | 3 | Every strike's animated limb reaches its hitbox on its active frames |
| `test_content` | 7 | All data loads; models exist and match their looks; sounds, stages and music loops |
| `test_input` | 4 | Simulated gamepads reach the right player, device assignment, short presses, menus |

## CPU against CPU

`tools/balance/balance.tscn`: every pairing of the six fighters, four matches per pairing and
side (144 matches per difficulty), after the balance changes made during validation.

| | Ashka | Gorm | Nix | Mirela | Bram | Hollis | Timeouts | Stuck or out of bounds |
|---|---|---|---|---|---|---|---|---|
| Novice | 67% | 75% | 27% | 29% | 52% | 50% | 0 of 344 rounds | 0 |
| Warrior | 42% | 71% | 44% | 29% | 54% | 60% | 0 of 347 rounds | 0 |
| Master | 62% | 35% | 31% | 21% | 77% | 73% | 0 of 351 rounds | 0 |

Rounds average 46–53 seconds. The figures move a lot between difficulty levels (the same fighter
can swing 35% to 75%), so they measure the CPU's habits as much as the fighters. Before the
changes Gorm won 83–88% at Novice and Warrior; Mirela is still the weakest in CPU hands.

## Played through the real window

All of the following ran the game as a player would, in its own window, with keyboard input
through X11 and screenshots; "autoplayer" is `tools/playtest/autoplayer.py`, which reads the
game's live trace and plays as player 1. They ran from the source tree during validation, on
an isolated playtest profile, at commits between `f0811e7` and `c0bb646`; each problem they
found was fixed and checked again. The released archive itself was checked as described under
[Package](#package).

| Check | Result |
|---|---|
| Tutorial, started from the main menu | All 13 steps completed with real keys, ending in Ashka's Last Light; completion saved. First attempt found the blocking step impossible (fixed). |
| Arcade, Novice, from the title screen | Ashka beat Hollis, Mirela, Bram, Gorm and her rival Nix (Last Light each time), lost to the Slopbat (left on 188 of 1900), continued, and won. The ending and credits play; the clear time, the ending, Ashka's alternate colours and the Roost were saved. |
| Arcade, Warrior | Five fights won, then three losses to the boss with the continue/retry path working each time (stopped there: the automatic player did not yet block projectiles). |
| Training | The dummy's modes cycled with Tab through Stand, Crouch, Block All (it blocked a punch), Block Random, Crouch Block and Jump; F1 drew hit and hurt boxes, F2 frame data and states, F4 reset positions (also used before each special below). |
| Every fighter's specials | All 25 specials of the six fighters entered on real keys in Training and confirmed started in the trace (`tools/playtest/specials.py`); screenshots checked. |
| Every fighter's finisher | Real matches against the Novice CPU, each fighter entering their own finisher: Rendered Down ×3, Undertow Pool ×2, The Last Note ×2 (a Mirela mirror match on the Roost), Lights Out ×1, Crush Depth ×2, Last Light ×5. Every fighter also fought from the right-hand side (978–4,539 ticks each). |
| All arenas | Rendering Yards, Drowned Chapel, Stilt Market and the Roost each hosted several of the matches above. |
| Versus with device assignment | Controllers screen: right keyboard set to nobody, a virtual Xbox 360 pad to player 2. Gorm against Gorm (mirror, alternate colours). The pad's Y/X/B/A gave player 2's punches and kicks, RB blocked, Start paused; holding the arrow keys left player 2 untouched. |
| Pause and focus | Esc pauses and resumes; switching away from the window pauses the fight; the move list opens from the pause menu (it was drawn under the HUD; fixed). |
| Settings and relaunch | Fullscreen on (on the 3440×1440 desktop, letterboxed 16:9) and off; music volume, gore and effects changed; Quit from the main menu exits cleanly (status 0); after relaunch the settings screen shows the changed values. |
| The stranger in white | Three meetings on one profile: his memory went 0 → 3 (greets, steps away with a warning, answers the next blow with one palm), 3 → 5 (asks, then Find the Crack) and 5 → 6 (recognition); holding Block bows out ("Go."). |
| Sound | The game opens its own PipeWire stream; four seconds recorded from the output during a fight measured RMS 0.03–0.11, peaks 0.12–0.68 (no clipping). |
| Sustained session | 30 minutes of unbroken arcade play (26 matches, most of them retries against the Warrior boss): no script errors; resident memory 1.81 GB after the first fight and 2.02 GB at 30 minutes, 11 MB of it in the last 20. A match restarted 20 times in a row from the pause menu kept Godot's object, node and resource counts flat (3,762, 286 and 55). |

## Performance

Measured with the game's own benchmark (`-- --bench=60`): CPU against CPU on each stage at
1920×1080 in a window, timing every frame for 60 seconds after a two-second warm-up, with the
GPU's own render time per frame alongside. Run on the release code.

**Held to 60 frames per second** (`--max-fps 60`, VSync off), the way the game is meant to be
played. Frame times in ms; 3,600 frames per run:

| Stage | Graphics | Median | p99 | p99.9 | Worst | Frames over 20 ms | GPU median / p99 / worst |
|---|---|---|---|---|---|---|---|
| Rendering Yards (Gorm v Hollis) | High | 16.67 | 16.69 | 16.71 | 16.72 | 0 | 7.8 / 8.8 / 9.4 |
| | Low | 16.67 | 16.69 | 16.72 | 17.33 | 0 | 5.1 / 6.2 / 6.8 |
| Drowned Chapel (Nix v Mirela) | High | 16.67 | 16.69 | 16.72 | 17.10 | 0 | 10.2 / 11.3 / 12.5 |
| | Low | 16.67 | 16.68 | 16.71 | 16.87 | 0 | 7.0 / 8.0 / 8.5 |
| Stilt Market (Bram v Ashka) | High | 16.67 | 16.68 | 16.71 | 16.71 | 0 | 8.8 / 9.9 / 10.3 |
| | Low | 16.67 | 16.68 | 16.72 | 17.08 | 0 | 5.9 / 7.0 / 7.3 |
| The Roost (Ashka v the Slopbat) | High | 16.67 | 16.68 | 16.71 | 17.08 | 0 | 8.9 / 10.1 / 10.5 |
| | Low | 16.67 | 16.68 | 16.71 | 16.72 | 0 | 6.4 / 7.6 / 8.1 |

On this machine the game holds 60 FPS at 1920×1080 on every stage with at least 4 ms of GPU
time to spare on the heaviest one (the Chapel on High). Simulation costs about 0.08 ms per tick;
animation, cloth and IK about 0.5–1.7 ms per frame.

**Uncapped** (VSync off, no limit), for headroom: 86–117 frames per second on High and 120–161
on Low on average. Uncapped frame times in a composited window are uneven (p95 up to twice
the median) while the GPU's own time stays steady, so the unevenness is presentation waiting
on the desktop compositor's 100 Hz cycle, not rendering cost.

Found and fixed here: held to 60, every match had one 28–32 ms frame when the first round
ended, because the large "WINS" lettering was rasterised on first use. The HUD now draws its
lettering once while the match loads.

## Package

`tools/export_linux.sh` exports with Godot's 4.7.2 Linux template (game data embedded in the
executable) and packs the archive: the 115 MB executable, the launcher `MortalSlopbat.sh`,
`install-desktop-entry.sh`, an icon, the player's README, `LICENSE`, `THIRD_PARTY_NOTICES.md` and
`licenses/`. The executable's embedded data was searched for test and tool scripts, the
developer's user name and e-mail, and assistant or "AI-generated" markers: none.

The archive was published as the private GitHub release `v1.0.1`, downloaded again with `gh`
into a new folder whose path contains spaces (`…/fresh 1.0.1/My Games/Mortal Slopbat/`),
checked against its SHA-256 (OK; byte-identical to the local build) and extracted there. From
that folder, started through `MortalSlopbat.sh` from another working directory:

- the title, menus, the select screen with its portraits, a Training mirror match, Extras on a
  fresh profile, and Quit from the main menu (the game exited by itself, status 0);
- the automatic player from the title screen into a Novice arcade run: three fights won, each
  with Last Light, before the four-minute session ended at the rival;
- `--rendering-driver opengl3`: a CPU match rendered and played on the OpenGL 3 fallback.

**First launch.** With every shader cache cold (the game's and the NVIDIA driver's; the driver's
was bypassed with `__GL_SHADER_DISK_CACHE=0` rather than deleted), the main menu holds still for
about 5 seconds when the select screen first shows fighters, and the face-off for about 12
seconds before the first fight (it now says "Loading…"). Launched straight into a fight with
the development flag `--match`, the window stays black for about 16 seconds. Later launches
start fights in 1–2 seconds. Baking shaders at export (Godot's shader baker) was tried and did
not shorten this, since the driver's own compile dominates, so it is not used.

1.0.0 (`c281f03`, SHA-256 `bea78014…acee84`) was released first and passed the same download,
checksum and launch checks; the first-launch test then found the select-screen overflow below,
so 1.0.1 replaced it. Its release notes say so.

## Found and fixed during validation

- The tutorial's blocking step could not be completed: Gorm stopped short and his punches never
  reached a held block.
- Hollis's diving helmet was missing (NaN geometry) and his brass corselet nearly so; the cause
  was loose bounding boxes in the sculpting tools, which also made one model build use over
  12 GB of memory. Every model was rebuilt.
- Fighter names on the health bars vanished over the emptied part of the bar.
- Ending text ran off the screen and the credits drew over it.
- The menu preview's green rim light dyed dark hair green.
- The move list opened from the pause menu sat under the HUD.
- Returning to the main menu always reset the cursor to Arcade.
- Select-screen portraits were rendered letterboxed and cropped.
- The tests left device assignments changed in their own settings file.
- Balance: Gorm hit too hard; Nix and Mirela were too fragile; Mirela's specials too slow.
- A long fighter description on the select screen ran into the controls hint.
- The face-off screen froze without explanation before the first fight of a fresh install.
- One 28–32 ms frame at the end of every match's first round (see Performance).
- Playtest tools: a restarted match wrote over its own trace.

## Known limitations

- Mirela wins least in CPU-against-CPU play at every difficulty.
- The automatic player needed a continue to beat the Slopbat even at Novice; a human's
  experience of the boss's difficulty is untested.
- Mirela's long skirt can pass through her legs when she crouches.
- The Slopbat is dark on dark stages by design; it reads by its green glow and silhouette.
- The first fight after installing waits for shader compilation (see Package).
- Resident memory settles at about 2.0–2.3 GB during play.
- At exit Godot's log lists GDScript resources still referenced (the engine reports scripts
  that refer to each other by class name); it has no effect on play or on saved data.
