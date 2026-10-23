# Validation record

What was checked before release, how, and what was found. Everything here was run on one
machine by the developer's tools; no independent review or outside playtest took place. One part
of the audit for 1.0.2 was a second, separate read of the code by the same development tooling,
working from the source alone; it is not an independent review, and each of its findings was
checked against the code before anything was changed.

| | |
|---|---|
| Version | 1.0.2 |
| Release commit | `e011a4b0fdb9643ff34f106c172bd69f5ef4a13b` (tag `v1.0.2`) |
| Archive | `MortalSlopbat-1.0.2-linux-x86_64.tar.gz` (68,414,623 bytes) |
| SHA-256 | `79694f5231b37c59efd8a4f9c95167c1d67a5d4112e14b5cd0d878609ea51b9f` |
| Machine | Linux Mint 22.3 (X11, Cinnamon), kernel 7.0, AMD Ryzen 7 5700G (8 cores, 16 threads), 14 GB RAM, NVIDIA GeForce RTX 2060 (driver 595.99), 3440×1440 display at 100 Hz |
| Engine | Godot 4.7.2-stable, Vulkan Forward+ |

## Not covered

- **Physical gamepads.** Controller support was exercised with simulated input only: synthetic
  Godot events in the tests and a virtual Xbox 360 pad created through Linux `uinput`
  (`tools/playtest/vpad.py`) in the versus playtest. No physical controller was connected.
- **Listening.** Nobody listened to the sound or music. The game's audio stream and the sound
  files were measured (see below), which shows they play unclipped at consistent levels, not
  that they sound good or are well mixed.
- **Other systems.** Only the machine above: no Wayland session, other distribution, AMD or
  Intel GPU, 60 Hz display, or screen smaller than 1600×900 (the window-fitting code for small
  screens was not exercised). The OpenGL 3 fallback was started into fights, not played through.
- **People.** All play was by scripts and a simple automatic player driving the real game window
  with X11 keyboard events (XTest). The automatic player reacts to visible attacks within
  about 30 ms, far faster than a person, so its wins and losses say little about how hard the
  CPU or the boss feels to a human, how readable the game is at speed, or how it feels to play.

## Automated tests

`tools/run_tests.sh` runs 67 headless tests (fails on any failed assertion or script error); all
pass, run repeatedly on an isolated test profile:

| File | Tests | Covers |
|---|---|---|
| `test_combat` | 24 | Motions from either side and across crossovers, input buffering, strike timing and reach, block levels and chip, trades, throws, breaks and command grabs, a thrower hit mid-throw letting go, harmless hits staying harmless, chains and the run meter, juggles and damage scaling, projectiles and clashes, determinism |
| `test_rounds` | 14 | Knockouts, the dizzy FINISH and finishers by range, timeouts and draws, double knockouts, matches that cannot loop forever, restart clean-up, parry, armour, teleport, the boss's health-gated storm and throw immunity, the loser's projectiles dying at the deciding knockout, flawless meaning no damage all match |
| `test_stranger` | 4 | The stranger's memory stages and responses |
| `test_flow` | 9 | Arcade ladders (rival, Reflection, boss and its tower health), continues, the flawless-win offer, saves (atomic, versioned, damaged-file recovery, test profiles kept apart), settings and rebinding across keyboard halves, device mapping, the tutorial's blocking steps |
| `test_animation` | 3 | Every strike's animated limb reaches its hitbox on its active frames |
| `test_content` | 7 | All data loads; models exist and match their looks; sounds, stages and music loops |
| `test_input` | 6 | Simulated gamepads reach the right player, device assignment, "nobody" devices in one-player modes, short presses, a button held through a pause, menus |

## CPU against CPU

`tools/balance/balance.tscn`: every pairing of the six fighters, four matches per pairing and
side (144 matches per difficulty), on the release code.

| | Ashka | Gorm | Nix | Mirela | Bram | Hollis | Timeouts | Stuck or out of bounds |
|---|---|---|---|---|---|---|---|---|
| Novice | 42% | 73% | 40% | 44% | 54% | 48% | 2 of 354 rounds | 0 |
| Warrior | 29% | 67% | 56% | 35% | 54% | 58% | 0 of 352 rounds | 0 |
| Master | 58% | 21% | 52% | 29% | 71% | 69% | 5 of 353 rounds | 0 |

Rounds average 52–58 seconds. The figures swing a lot between levels (Gorm is strongest at
Novice and weakest at Master), so they measure how each level of CPU copes with a fighter as
much as the fighter. Mirela, last at Warrior and Master before the audit (29% and 21%), is now
second from bottom at both (35% and 29%), ahead of Ashka and Gorm respectively.

**The boss.** Each fighter against the Slopbat, eight matches each per tower, both CPUs at the
tower's level: the Slopbat won 42% at Novice, 40% at Warrior and 29% at Master. Before the
audit it won 71% at Novice, which made the easiest tower's boss the hardest relative to its
opponents.

## Played through the real window

All of the following ran the game as a player would, in its own window, with keyboard input
through X11 and screenshots; "autoplayer" is `tools/playtest/autoplayer.py`, which reads the
game's live trace and plays as player 1. Unless marked, they ran from the source tree at the
1.0.2 code, on an isolated playtest profile. The released archive itself was checked as
described under [Package](#package).

| Check | Result |
|---|---|
| Tutorial, started from the main menu | All 13 steps completed with real keys (five blocks, the special, the chain, the throw), ending in Ashka's Last Light; completion saved. |
| Arcade, Novice, from the title screen | Ashka beat Mirela, Hollis, Bram and her rival Nix, lost to the Slopbat once, continued and won. The clear time, the ending, Ashka's alternate colours and the Roost were saved. |
| Arcade, Warrior | At 1.0.1: five fights won, then the continue/retry path through several boss losses. |
| Training | The dummy's modes cycled with Tab through Stand, Crouch, Block All (it blocked a punch), Block Random, Crouch Block and Jump; F1 drew hit and hurt boxes, F2 frame data and states, F4 reset positions. In slow motion (F7), Ember Shot and the four-hit Lantern Lash dial-a-combo came out from presses at normal speed. |
| Every fighter's specials | All 25 specials of the six fighters entered on real keys in Training and confirmed started in the trace (`tools/playtest/specials.py`); screenshots checked. |
| Every fighter's finisher | Real matches against the Novice CPU, each fighter entering their own finisher, captured frame by frame while it played: Last Light, Rendered Down, Undertow Pool, The Last Note, Lights Out (reworked, see below) and Crush Depth. Every fighter also fought from the right-hand side. |
| All arenas | Rendering Yards, Drowned Chapel, Stilt Market and the Roost each hosted several of the matches above; each was also checked at its widest and highest camera framings. |
| Versus with device assignment | Controllers screen: right keyboard set to nobody, a virtual Xbox 360 pad to player 2. Gorm against Gorm (mirror, alternate colours). The pad's Y/X/B/A gave player 2's punches and kicks, RB blocked, Start paused; holding the arrow keys left player 2 untouched. |
| Pause and focus | Esc pauses and resumes; switching away from the window pauses the fight; the move list opens over the pause menu, Up and Down scroll it, Enter does nothing there, and Esc returns to the pause menu. |
| Character select | Block with nothing picked goes back to the main menu (the gamepad route). |
| Settings and relaunch | Fullscreen on and off (also with F11); music volume, gore and effects changed; Quit from the main menu exits cleanly (status 0); after relaunch the settings screen shows the changed values. |
| The stranger in white | Three meetings on one profile: his memory went 0 → 3 (greets, steps away with a warning, answers the next blow with one palm), 3 → 5 (asks, then Find the Crack) and 5 → 6 (recognition); holding Block bows out ("Go."). |
| The Master CPU | The autoplayer against Master Bram, several five-minute sessions: before the audit it won every round, landing 20 jump-in kicks; afterwards the CPU anti-airs jump-ins (10 uppercuts landed in one session) and took 1–2 rounds in some sessions, though the autoplayer still won most. |
| Sound | The game opens its own PipeWire stream; four seconds recorded from the output during a fight measured RMS 0.03–0.11, peaks 0.12–0.68 (no clipping). All 54 effects peak at −1 dBFS and the seven music tracks sit within about 2 dB of each other in loudness. |
| Sustained session | With the released archive: 30 minutes of unbroken arcade play from the title screen (20 matches, many of them retries against the Warrior boss). Resident memory 1.78 GB after the first fight and 2.12 GB at 30 minutes, 3 MB of it in the last ten; Godot's object, node and resource counts were identical across the last boss rematches; nothing was written to the error output. At 1.0.1, a match restarted 20 times in a row from the pause menu kept those counts flat too. |

## Performance

Measured with the game's own benchmark (`-- --bench=60`): CPU against CPU on each stage at
1920×1080 in a window, timing every frame for 60 seconds after a two-second warm-up, with the
GPU's own render time per frame alongside. Run on the release code with the GPU otherwise idle.

**Held to 60 frames per second** (`--max-fps 60`, VSync off), the way the game is meant to be
played. Frame times in ms; 3,600 frames per run:

| Stage | Graphics | Median | p99 | p99.9 | Worst | Frames over 20 ms | GPU median / p99 / worst |
|---|---|---|---|---|---|---|---|
| Rendering Yards (Gorm v Hollis) | High | 16.67 | 16.69 | 16.71 | 16.72 | 0 | 7.4 / 8.2 / 8.4 |
| | Low | 16.67 | 16.68 | 16.71 | 16.72 | 0 | 5.2 / 6.0 / 6.3 |
| Drowned Chapel (Nix v Mirela) | High | 16.67 | 16.68 | 16.71 | 16.72 | 0 | 9.8 / 10.7 / 11.5 |
| | Low | 16.67 | 16.69 | 16.72 | 16.74 | 0 | 6.8 / 7.5 / 7.8 |
| Stilt Market (Bram v Ashka) | High | 16.67 | 16.69 | 16.71 | 16.71 | 0 | 8.3 / 9.2 / 9.7 |
| | Low | 16.67 | 16.68 | 16.70 | 16.71 | 0 | 5.6 / 6.5 / 6.9 |
| The Roost (Ashka v the Slopbat) | High | 16.67 | 16.69 | 16.71 | 16.72 | 0 | 8.3 / 9.1 / 9.7 |
| | Low | 16.67 | 16.68 | 16.71 | 16.72 | 0 | 5.9 / 6.7 / 7.0 |

On this machine the game holds 60 FPS at 1920×1080 on every stage with at least 5 ms of GPU
time to spare on the heaviest one (the Chapel on High). Simulation costs about 0.08 ms per tick;
animation, cloth and IK about 0.5–1.7 ms per frame.

**Uncapped** (VSync off, no limit; measured at 1.0.1, before the camera change): 86–117 frames
per second on High and 120–161 on Low on average. Uncapped frame times in a composited window
are uneven (p95 up to twice the median) while the GPU's own time stays steady, so the
unevenness is presentation waiting on the desktop compositor's 100 Hz cycle, not rendering
cost.

**Memory.** Godot's own allocations are about 90–180 MB. The process's resident memory is
much larger (about 1.8 GB after the first fight, rising to about 2.4 GB as each new stage is
first loaded, then level), almost all of it the graphics drivers: NVIDIA's mappings, a large
anonymous driver region and driver libraries. Godot's object counts stay flat across 20
restarts of a match.

## Package

`tools/export_linux.sh` exports with Godot's 4.7.2 Linux template (game data embedded in the
executable) and packs the archive: the 115 MB executable, the launcher `MortalSlopbat.sh`,
`install-desktop-entry.sh`, an icon, the player's README, `LICENSE`, `THIRD_PARTY_NOTICES.md` and
`licenses/`. The executable's embedded data was searched for test and tool scripts, the
developer's user name and e-mail, and assistant or "AI-generated" markers: none.

The archive was published as the private GitHub release `v1.0.2`, downloaded again with `gh`
into a new folder whose path contains spaces (`…/fresh 1.0.2/My Games/Mortal Slopbat/`),
checked against its SHA-256 (OK; byte-identical to the local build) and extracted there. From
that folder, started through `MortalSlopbat.sh` from another working directory:

- the title, menus, the select screen with its portraits, a Training mirror match, Extras on a
  fresh profile, and Quit from the main menu (the game exited by itself, status 0);
- the automatic player from the title screen into a Novice arcade run: two fights won, each
  with Last Light, before the four-minute session ended;
- `--rendering-driver opengl3`: a CPU match rendered and played on the OpenGL 3 fallback;
- the 30-minute session above.

**First launch.** With every shader cache cold (the game's and the NVIDIA driver's; the driver's
was bypassed with `__GL_SHADER_DISK_CACHE=0` rather than deleted), the main menu holds still for
about 5 seconds when the select screen first shows fighters, and the face-off for about 12
seconds before the first fight. Both frozen frames now carry a "Loading…" note. Launched straight
into a fight with the development flag `--match`, the window stays black for about 16 seconds.
Later launches start fights in 1–2 seconds. Baking shaders at export (Godot's shader baker) was
tried and did not shorten this, since the driver's own compile dominates, so it is not used.

Earlier releases: 1.0.0 (`c281f03`, SHA-256 `bea78014…acee84`) and 1.0.1 (`c0bb646`, SHA-256
`ba6b93fe…7984`) passed the same download, checksum and launch checks; their release notes now
point to 1.0.2.

## Found and fixed

Before 1.0.0 and 1.0.1:

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
- A long fighter description on the select screen ran into the controls hint.
- One 28–32 ms frame at the end of every match's first round (glyphs rasterised on first use).
- Balance: Gorm hit too hard; Nix and Mirela were too fragile; Mirela's specials too slow.

In the audit for 1.0.2 (found by reviewing hundreds of match screenshots, burst captures of
every finisher, every screen, the logs, CPU-against-CPU runs, and the code review):

- **Camera.** At 4.3 m with a 30° view it showed only 2.3 m of height: jumping fighters lost
  their heads, the grounded one lost their legs when it rose, and the Slopbat's head was out of
  frame. It now fits both fighters whole, feet to head, pulling back as needed, anchored to the
  floor.
- **Mirela's gown** was a closed ring of cloth: kicks wrapped the leg in it and crouches pushed
  it into sacks. It now opens at the front.
- **The Slopbat** read as a black shape on its dark stage; its veins now glow.
- **Boss difficulty** was inverted (hardest, relative to its opponents, in the easiest tower); its
  health now depends on the tower: 1400, 1650, 1900.
- **CPU** could not anti-air a jump it only saw late; it now reads jumps from their start and
  adapts to an opponent who keeps sweeping (and throws one who turtles).
- **Throws**: a thrower hit mid-throw left the victim frozen in the throw.
- **Deciding knockout**: the loser's projectiles in flight could still hurt the winner.
- **Flawless**: a flawless last round counted as a flawless match (banner, statistic, the
  stranger's appearance).
- **Wing Gust** did 1 damage instead of none.
- **Gamepad B** pressed to leave the pause menu threw a High Kick.
- **Slow motion** (Training) and the knockout slow-down dropped button presses.
- **Devices set to nobody** for Versus stayed dead in one-player modes.
- **A match started in the background** (switched away during loading) did not pause.
- **The move list over the pause menu** left Up, Down and Enter driving the hidden pause menu,
  so Enter could quit the arcade.
- **Gamepads** could not leave character select.
- **Rebinding** a key used by the other keyboard half gave one key two actions.
- **Lights Out** sent its victim through the stage's walls, leaving the camera on scenery.
- **First-run stalls** on the select screen froze the menu with no explanation.
- **Shutdown** left every script, the theme and shaders alive (a static variable in the menu).
- **Logs**: a subsurface-scattering warning from menu previews and texture-format conversions
  at load.
- **Small things**: no fullscreen hotkey (now F11 / Alt+Enter); a 1600×900 window could open
  larger than a small screen; the wrong-distance hint stayed over finishers; volumes saved as
  `0.30000000000000004`.

## Known limitations

- CPU-against-CPU win rates spread from about 21% to 73% and swing between difficulty levels;
  how fair each fighter feels to people is untested.
- The automatic player needed a continue to beat the Slopbat at Novice; a human's experience of
  the boss is untested.
- The first fight after installing waits for shader compilation (see Package).
- Resident memory reaches about 2.4 GB, nearly all of it the graphics drivers (see Performance).
- Physical gamepads, other systems and sound quality remain untested (see Not covered).
