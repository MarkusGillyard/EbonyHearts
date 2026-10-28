# Validation

What was checked, how, and what was not. FLOWBOX: AMD Ryzen 7 5700G (8 cores, 16 threads),
14 GiB RAM, NVIDIA GeForce RTX 2060 6 GB, driver 595.99.02, Linux 7.0 (Linux Mint 22.3), X11,
Unreal Engine 5.8.4, Vulkan SM6. Evidence files are under `Saved/` (not versioned): `Saved/Perf/`
for sessions, `Saved/Logs/tests.log` for the last test run.

## Automated tests

`run_tests` (Unreal automation, headless): **95 tests, all passing** on the source of 2.6.2.

| Area | What the tests hold |
|---|---|
| Timing and input | Same inputs at 30/60/144 Hz and uneven frame times give the same match; the clock's catch-up and overload; the 6-tick buffer and its expiry; presses during hit-stop kept; held directions; motions from either side and through a crossover; opposite directions cancel; one press starts one move. |
| Combat | A counter hit's extra freeze falls on the victim alone and leaves frame advantage as it was; Ashka's teep comes out on forward + low kick and pushes far for little damage; active windows from the first frame; whiffs out of range; highs, lows, overheads and guards from either side; chip only from specials and projectiles; throws, breaks, back throws, command grabs beating block; trades; counter hits; damage scaling; juggle limit; run-meter costs; projectiles one at a time and clashing; parry, armour, teleport; Ashka's shin check catches a sweep (the kicker stung and left open, Ashka moving first) and lets a high punch through; no special loops into itself, inside its cancel window or out of it, mid-stage or at the edge, and mashing any button ends within a string; bodies never overlap or leave the stage. |
| Match rules | KO and the next round, double KO, timeout and draw, deciding KO with finisher, the finish window timing out, restart clearing everything, input after KO changing nothing, flawless, no match running forever. |
| Roster and AI | All data loads and validates; every trial possible; CPU against CPU in every pairing; the CPU never reacts before its delay; the boss can't be thrown, fights to an end, and storms only below half health; the stranger's memory only moves forward; swept again and again, the CPU Ashka checks some sweeps and is swept no more often than guarding alone; jabbed again and again, the CPU Bram parries some jabs with Stand Down and is hit no more often than blocking alone. |
| Animation | The ported rig matches the Godot original joint by joint on every pose of every fighter; the animator follows every state of a match; through the follow-through the arm carrying a blow stays exactly on its authored pose until the blow is over; a straight drives the struck head away, a hook turns the face the way the fist travels, an uppercut lifts the chin and a body shot folds the torso, from either side; turning round after a cross-over moves no joint more than 30 cm in a frame and ends facing the new way; a high punch marks the face, a low punch the torso and a sweep the legs of every fighter; beaten legs limp (hips back standing, lower walking) with the feet on the floor. |
| Content | Every fighter has a body that maps onto the rig, the character material on every slot and a portrait; every stage piece the data places exists with its materials and surface textures. |
| Saves | Settings round trip and bad values; rebinding never binds twice; reserved keys and pad buttons refused (directions, menu keys, F-keys); progress records, a damaged file set aside, the original game's file read once and never written; an interrupted write recovered; a damaged settings file set aside; a later version's progress copied aside. |
| Flow | Arcade ladders, scoring and the end; Survival waves, carried health and the boss. |

## Playing the modes

Unattended runs on the editor build and the packages, rendering offscreen at 1920 x 1080 (see
"Display" below), with the scripted flows in `Tools/playtest/flows` and the CPU playing
(`-SlopAutoPlay`). Each was followed screen by screen in the log and in screenshots.

| Flow | Result |
|---|---|
| Arcade (CPU at Master, Warrior tower) | Six fights in turn (Hollis, Gorm, Bram, Mirela, the rival Nix, the Slopbat in the Roost), the ending, credits, the main menu, and a second ladder begun; finishers performed. |
| Survival | Waves carried over with health refills, difficulty stepping up after wave three, a loss, the results and a new run. |
| Versus | Devices (Continue), both players picking (the second through its own side), stage, face-off, fight, out through the pause menu. |
| Training | Fighter and dummy picks, stage, the dummy's modes cycled, reset, the move list from pause, character select and main menu. |
| Tutorial | Every step skipped in turn to the end; the results. |
| Trials | Each trial in turn with its prompt and ticks; out through pause. |
| Extras, Settings | Endings, records, move lists, credits; Settings pages; a changed volume saved to the profile. |
| Soak | 30 minutes of the Development package (20 matches) and 30 minutes of the Shipping package (below), no assertions, errors or stuck rounds. |

Seen in screenshots: all eight fighters in matches and all four stages from the fight camera,
fighters on both sides of the screen; every ground move of Ashka, Gorm and the Slopbat at its
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

2.5.1 (commit `27c727f`, the Slopbat's tar, wind and bite changes on 2.5.0) was checked
offscreen only, as the monitor was off (`Monitors: 0`; a window run then blocks in Vulkan
presentation, as noted under Display): unpacked fresh (checksums matched), the CPU played an arcade
fight for 85 s at 60 fps with every frame within 1 ms, no hitches, GPU 14.91 ms at a 75 % render
scale, peak memory 1,415 MB (`Saved/Perf/offscreen-2.5.1`). Full screen on the monitor is not yet
checked for 2.5.1; nothing that touches the window or rendering changed since 2.5.0.

2.5.0 (commit `60bf254`, with captured footwork, block and evade, rippled water, the Roost's lit
slop lake and contact shadows), unpacked fresh from the archive (checksums matched): 7,007 frames
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
Market 8.9-9.0 ms, the Roost (the Slopbat) 8.3-8.5 ms; every stage and matchup held 60 FPS with a
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

## Not verified

- DualSense, USB connection, rumble and reconnection on a real pad, two physical pads (above).
- Windowed presentation timings with VSync (above).
- Audio by ear: levels, mix and the layering were checked in code and logs only.
- Other aspect ratios on a real display. Offscreen: at 1920 x 1200 the menus letterbox, the
  fight camera frames both fighters and the HUD fits with a margin above and below (fixed: it
  had run off both sides); at 1440 x 1080 (4:3, since 2.6.1) the title, device and fighter-select
  screens fit and the fight frames both fighters whole (fixed: see above); at 3440 x 1440 the
  fighters fill the height as the framing means, with the stage around them. Markus's display
  is 3440 x 1440; 2.6.1 is the first build drawing it that way, and nobody has looked on it yet.
