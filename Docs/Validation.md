# Validation

What was checked, how, and what was not. FLOWBOX: AMD Ryzen 7 5700G (8 cores, 16 threads),
14 GiB RAM, NVIDIA GeForce RTX 2060 6 GB, driver 595.99.02, Linux 7.0 (Linux Mint 22.3), X11,
Unreal Engine 5.8.4, Vulkan SM6. Evidence files are under `Saved/` (not versioned): `Saved/Perf/`
for sessions, `Saved/Logs/tests.log` for the last test run.

## Automated tests

`run_tests` (Unreal automation, headless): **67 tests, all passing** on the final source.

| Area | What the tests hold |
|---|---|
| Timing and input | Same inputs at 30/60/144 Hz and uneven frame times give the same match; the clock's catch-up and overload; the 6-tick buffer and its expiry; presses during hit-stop kept; held directions; motions from either side and through a crossover; opposite directions cancel; one press starts one move. |
| Combat | Active windows from the first frame; whiffs out of range; highs, lows, overheads and guards from either side; chip only from specials and projectiles; throws, breaks, back throws, command grabs beating block; trades; counter hits; damage scaling; juggle limit; run-meter costs; projectiles one at a time and clashing; parry, armour, teleport; bodies never overlap or leave the stage. |
| Match rules | KO and the next round, double KO, timeout and draw, deciding KO with finisher, the finish window timing out, restart clearing everything, input after KO changing nothing, flawless, no match running forever. |
| Roster and AI | All data loads and validates; every trial possible; CPU against CPU in every pairing; the CPU never reacts before its delay; the boss can't be thrown, fights to an end, and storms only below half health; the stranger's memory only moves forward. |
| Animation | The ported rig matches the Godot original joint by joint on every pose of every fighter; the animator follows every state of a match. |
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
| Real DualShock 4 over Bluetooth (paired with FLOWBOX) | Detected by the game ("PS4 Controller", rumble supported, slot 1). **No button on it was pressed** in this work: menus, fighting, rumble and reconnecting on the real pad are **unverified**. |
| DualSense, USB connection, two physical pads, keyboard plus pad | **Unverified** (no hardware handled). |

Rumble is implemented through the engine's force-feedback interface for the struck player's pad;
whether it is felt on a real pad is unverified.

## Performance

Shipping package of commit `16cb3e3` (the release differs from it only in the HUD's layout on
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

## Packaging

`package_linux Shipping` cooks with 0 errors and 0 warnings. `Tools/make_release.sh Shipping`
wraps it with the readme, licences, version and checksums; the archive was unpacked into a fresh
folder and run from there with the editor closed (below).

## Display and what offscreen means

For the later part of the work FLOWBOX's monitor was switched off (X reported no monitors), and a
Vulkan window cannot present without one, so the game was run with Unreal's `-RenderOffscreen`:
the same renderer and GPU, rendering every frame at 1920 x 1080, without presenting it. In that
mode SDL uses its dummy video and audio drivers, so pads and sound are not available; the pad
tests above were run earlier with the monitor on. Timings offscreen leave out presentation and
VSync, so a windowed session is worth one more measurement (`Tools/perf/soak.sh` with the monitor
on).

## Found and fixed during validation

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

## Not verified

- Physical PS4 and PS5 pads, USB and Bluetooth, rumble, two pads (above).
- Windowed presentation timings with VSync (above).
- Audio by ear: levels, mix and the layering were checked in code and logs only.
- Other aspect ratios on a real display. At 1920 x 1200 (offscreen) the menus letterbox, the
  fight camera frames both fighters and the HUD fits with a margin above and below (fixed: it
  had run off both sides); 4:3 and ultrawide follow the same rules but were not rendered.
