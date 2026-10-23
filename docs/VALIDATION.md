# Validation record, 1.1.0

What was checked for 1.1.0, how, and what was found. Everything here was run on one machine by
the developer's tools; no outside playtest took place. The record for 1.0.2 is kept beside this
one as [VALIDATION-1.0.2.md](VALIDATION-1.0.2.md); checks that 1.1.0 did not repeat are listed
there and were not re-run.

| | |
|---|---|
| Version | 1.1.0 |
| Branch | `improve/feel-and-look` on top of 1.0.2 (`e011a4b`) |
| Machine | Linux Mint 22.3 (X11, Cinnamon), kernel 7.0, AMD Ryzen 7 5700G, 14 GB RAM, NVIDIA GeForce RTX 2060 (driver 595.99), 3440×1440 display at 100 Hz |
| Engine | Godot 4.7.2-stable, Vulkan Forward+ |

## What changed, in one paragraph

Hits land (counter hits, longer hit-stop on knockdowns, a flash through the victim, impact rings
and more sparks, a camera that shakes a little on every blow and kicks along heavy ones, a
freeze, punch-in and slow motion on the knockout, a combo counter that jumps), the fighters look
sculpted rather than moulded (folds, grime, mottled skin, an accent rim in each fighter's colour),
the frame is finished (depth of field behind the fighting plane, a vignette and grain), the walk
and idle have life in the hips, dial-a-combos actually connect (push-back scaled on chain openers
and combo hits), and three things give a reason to come back: Survival, arcade scoring and Trials.

## Not covered

As for 1.0.2: no physical gamepad, nobody listened to the sound, no other machine, and all play
was by scripts and the automatic player. The new presentation was judged from screenshots of the
real window, not by eye in motion; a 30 ms flash can only be checked by lengthening it or by
frame-stepping, both of which were done once.

## Automated tests

`tools/run_tests.sh`: 74 headless tests, all passing (67 before). New:

| File | Tests | Covers |
|---|---|---|
| `test_flow` | +3 | Survival waves (the boss every fifth, never the same opponent twice running, difficulty by wave), health carried with a refill and capped, survival records; the arcade points formula and best-score records |
| `test_trials` | 3 | The trial checker (order, drops, one-hit trials, the combo_end event); every trial in `data/trials.json` performed headless from touching distance against the fighter's rival, dialling each link once the blow before it has landed |
| `test_scripts` | 1 | Every script under `scripts/` compiles (a parse error in a menu screen had slipped past the other suites, which only load what they exercise) |

The counter-hit, hit-stop and push-back changes passed the existing 24 combat and 14 round tests
unchanged.

## CPU against CPU

`tools/balance/balance.tscn`, every pairing, four matches per pairing and side (144 per level),
after the counter-hit and push-back changes:

| | Ashka | Gorm | Nix | Mirela | Bram | Hollis | Timeouts | Stuck or out of bounds |
|---|---|---|---|---|---|---|---|---|
| Novice | 42% | 69% | 56% | 31% | 50% | 52% | 0 of 353 rounds | 0 |
| Warrior | 25% | 65% | 77% | 40% | 52% | 42% | 0 of 360 rounds | 0 |
| Master | 52% | 29% | 46% | 38% | 71% | 65% | 4 of 346 rounds | 0 |

Rounds average 48–53 seconds. The boss at Warrior (eight matches per fighter, both CPUs at
Warrior): the Slopbat won 31% (40% in 1.0.2); Hollis alone lost to it seven times in eight,
Ashka never. The counter-hit change alone (before the push-back change) gave 42/62/46/42/56/50,
29/62/67/38/50/54 and 62/25/38/35/71/69.

The spread is the same shape as in 1.0.2 (there: 42–73%, 29–67% and 21–71%); counter hits and
the softer push-back did not tip it, no match stalled and no fighter left the stage.

## Performance

`-- --bench=60` at 1920×1080 in a window, held to 60 frames per second, VSync off, GPU
otherwise idle, on the branch with depth of field, the post layer and the new effects:

| Stage | Median | p99 | Worst | Frames over 20 ms | GPU median / p95 / p99 / worst | 1.0.2 GPU median |
|---|---|---|---|---|---|---|
| Drowned Chapel (Nix v Mirela), High | 16.67 | 16.69 | 18.79 | 0 | 11.41 / 12.15 / 12.48 / 13.60 | 9.8 |
| Rendering Yards (Gorm v Hollis), High | 16.67 | 16.69 | 16.73 | 0 | 9.12 / 9.58 / 9.83 / 10.97 | 7.4 |

The depth of field and the post pass cost about 1.6 ms of GPU time on the heaviest stage, which
still leaves it 4 ms inside the 60 Hz frame on this machine. Simulation 87 µs per tick;
animation, cloth and IK 2.2 ms per frame on the Chapel pairing (Mirela's gown, veil and hair).
The Low graphics setting drops the depth of field with the other high-end effects.

**Memory.** The automatic player's Survival run (seven fights over five minutes from the title
screen): resident memory 0.9 GB at the first sample, 2.27 GB after the first fight, 2.41 GB at
three minutes and 2.41 GB at the end; Godot's object counts per match stayed between 3,323 and
3,440 with no orphans. As in 1.0.2, nearly all of the resident memory is the graphics driver.

## Played through the real window

All with `tools/playtest/play.sh` (scripted X11 keys and screenshots of the game window) or
`autoplay.sh` (the automatic player), from the source tree, on the playtest profile.

| Check | Result |
|---|---|
| Every screen | Title, menu (with Survival and Trials), select, tower, face-off, a fight, pause, move list, settings (with Vignette and grain), extras, controllers: all drawn as before. |
| All four stages with the new look | CPU matches on each stage, sixteen screenshots each, looked over: the fighters read against every stage (the accent rim), the stage softens behind them, the vignette holds the eye on the fight, nothing clips or flickers. The Yards, flattest in 1.0.2, gains the most. |
| Hits | Punches and an uppercut in Training captured at 30 ms steps and, once, with the flash and ring lives lengthened eight times: the body flash, the glow, the ring and the sparks all draw at the point of contact; at their real lengths they are over within two to eight frames, as intended. |
| Counter hits | "COUNTER!" seen on the HUD in a CPU match when a roundhouse landed during a wind-up. |
| Knockout | Two versus matches played by script to a knockout (one in the game's slow motion): the camera closes in on the fallen fighter under the WINS banner and returns to the normal framing at the next round. The quarter-second freeze on the deciding blow itself fell between screenshots both times and was checked by reading the code, not seen. |
| Survival | The automatic player from the title screen: wave 1 won (Gorm, Sloptality), wave 2 lost to Bram, retry, then Bram, a mirror match and Mirela won, wave 4 lost to Gorm at Warrior (the difficulty step), retry: every transition (results, retry, face-off with the wave number, the carried health) worked and nothing was printed to the error output. |
| Trials | From the title screen into Trials with Ashka: the prompt shows "Trial 1 of 5: Lantern Lash", its inputs and a circle per hit; a scripted dial from touching distance landed the first three hits as one combo (3 HIT COMBO, the ring, glow, sparks and blood at the contact) and the last link, pressed off-rhythm by the script, was reported as dropped and the circles reset; Tab moved to the next trial and Esc paused with Restart, Character Select and Main Menu. Every trial's full completion is covered by `test_trials`, which performs each one headless. |
| Sustained | See Memory above. |

## Package

`tools/export_linux.sh` on the final commit: `MortalSlopbat-1.1.0-linux-x86_64.tar.gz`
(68,437,829 bytes), SHA-256 `f9c21420125224f66ed43d204c871bf7e0bd107b0913ae2a05532e5ce7bbbd0a`. The archive was extracted into a new folder
whose path contains spaces, checked against its SHA-256 (OK), and its executable searched for
test and tool scripts, the developer's user name and e-mail, and assistant markers: none.
Launched from that folder through `MortalSlopbat.sh` with the shader cache cold: the title,
the main menu with Survival and Trials, the select screen, the Novice tower and the face-off
(showing its Loading note while the first fight's shaders compiled), the fight, and Quit Arcade
from the pause menu back to the main menu; nothing on the error output. The archive is not
committed; the checksum above ties it to the source commit named in the pull request.

## Found and fixed

- **Dial-a-combos could not connect.** The first hit's push-back (0.33 m) exceeded the reach of
  the next link, so the strings in the move list never landed in full except by accident; the
  CPU's combo lists had the same problem. Found by the trials test, which performs every combo:
  all 23 chain trials failed before the push-back change and pass after it.
- **Face-off screen** (survival label) had a type-inference error that the suites did not load;
  caught by the new compile test.
- **Impact effects were too short to see in a screenshot** at 50 ms steps, which is also how a
  player would miss them: flash and ring lives were raised from 0.09 s to 0.13–0.22 s.

## Known limitations

- The feel changes were tuned from screenshots and frame data, not by hand on a pad.
- Survival's difficulty curve (three waves per level, the Slopbat every fifth) is a first
  setting; the automatic player lost at wave 2 and wave 4, a person will do differently.
- Trials dial each link once the blow before it has landed; a player must do the same (the
  window closes about ten frames after the hit).
- The 1.0.2 limitations stand: CPU-against-CPU win rates swing by level, the first fight after
  installing waits for shader compilation, resident memory is mostly the driver's, and sound was
  never listened to.
