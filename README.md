# Mortal Slopbat

An arcade fighting game in the tradition of *Mortal Kombat 3*: six fighters, sculpted 3D
characters on a classic side-on fighting plane, a run button and dial-a-combos, finishers, an
arcade tower that ends at a winged boss, and a stranger in white who is best left alone.

**Copyright © 2026 Markus Gillyard. All rights reserved.** Proprietary; see [LICENSE](LICENSE).
Third-party components keep their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

| | |
|---|---|
| Version | 1.0.1 |
| Engine | Godot 4.7.2-stable (GDScript, Forward+, falls back to OpenGL 3) |
| Tested on | Linux Mint 22.3 (X11, Cinnamon), x86_64, NVIDIA GeForce RTX 2060 (driver 595), Ryzen 7 5700G |
| Settings | `~/.local/share/MortalSlopbat/settings.cfg` |
| Progress | `~/.local/share/MortalSlopbat/progress.json` |

Other distributions, Wayland sessions, other GPUs and physical gamepads have not been tested.

## Playing the Linux build

```bash
tar -xzf MortalSlopbat-1.0.1-linux-x86_64.tar.gz
./MortalSlopbat-1.0.1-linux-x86_64/MortalSlopbat.sh
```

The archive holds a self-contained executable: no Godot editor, network or account is needed.
`install-desktop-entry.sh` in the same folder adds the game to your application menu (for your
user only). If Vulkan is unavailable the game falls back to OpenGL 3; to force it, run
`./MortalSlopbat.sh --rendering-driver opengl3`.

## Controls

The keyboard is split in two halves laid out like the MK3 arcade panel (High Punch, Block, High
Kick on the top row; Low Punch, Run, Low Kick below). In one-player modes both halves and any
gamepad control Player 1. In Versus, the Controllers screen assigns each keyboard half and each
gamepad to a side. Every key can be rebound in **Settings**.

| Action | Player 1 keys | Player 2 keys | Gamepad |
|---|---|---|---|
| Move, jump, crouch | W A S D | Arrow keys | D-pad or left stick |
| High Punch | U | Numpad 7 | Y |
| Block | I | Numpad 8 | RB or RT |
| High Kick | O | Numpad 9 | B |
| Low Punch | J | Numpad 4 | X |
| Run | K | Numpad 5 | LB or LT |
| Low Kick | L | Numpad 6 | A |
| Pause | Esc | | Start |

Menus: arrows or WASD to move, Enter (or J/U) to choose, Backspace or Esc to go back.

## How to fight

- **Block is a button.** Standing block stops high, mid and overhead attacks; crouching block
  stops lows (sweeps, crouching kicks) and mids. It works from either side.
- **Run** by holding Run with forward. Running drains the blue run meter, which refills after a
  short rest. **Dial-a-combos** (preset button strings such as High Punch, High Punch, Low Punch,
  Back + High Kick) also spend run meter on every link.
- **Basics** everyone shares: Down + High Punch uppercut, Back + Low Kick sweep, Back + High Kick
  roundhouse, crouching punches and kicks, jump punches and kicks (up-forward and up-back jumps
  are somersaults), and a throw (Forward or Back + Low Punch up close; press a punch just after
  being grabbed to break it).
- **Specials** use short motions such as Down, Forward + High Punch. Every fighter's full list,
  with frame data, is under Pause → Move List, Training and Extras → Move Lists.
- **Finishing.** Win the deciding round by knockout and the loser stands dizzy: FINISH HIM, HER
  or IT. Step to the right distance and enter your **Sloptality**, or just hit them.

## Modes

- **Arcade**: choose Novice, Warrior or Master (which also sets how hard the CPU fights), climb
  past your rival to the Slopbat's roost, and see your fighter's ending. Losing offers a continue.
- **Versus**: two players, any pairing (mirror matches use the alternate colours), any stage.
- **Training**: a dummy that stands, crouches, blocks, jumps or fights back (Tab), with resets
  (F4), hitboxes (F1), frame data (F2), states (F3) and input history (F8).
- **Tutorial**: a guided five minutes covering movement, blocking, strikes, a special, a combo, a
  throw and a finisher.
- **Extras**: endings you have earned, records, move lists and the credits.

Clearing arcade with a fighter unlocks their alternate colours (High Kick on the select screen)
and the Roost as a versus stage. Some things are only found by fighting well.

## Building from source

Requirements: Linux x86_64, Godot **4.7.2-stable** (standard build, not .NET) on `PATH` as
`godot`, and its Linux export templates (Editor → Manage Export Templates, or extract
`Godot_v4.7.2-stable_export_templates.tpz` into `~/.local/share/godot/export_templates/4.7.2.stable/`).

```bash
git clone https://github.com/MarkusGillyard/Slopbat.git
cd Slopbat
godot --path .                    # run from source
tools/run_tests.sh                # headless test suite
tools/export_linux.sh             # builds dist/MortalSlopbat-<version>-linux-x86_64.tar.gz and .sha256
```

Development flags (after `--`): `--match=p1,p2,mode[,stage[,difficulty]]` jumps into a fight
(for example `-- --match=ashka,gorm,versus,drowned_chapel`), `--screen=name` opens a screen,
`--profile=name` keeps progress in a separate test profile, `--trace=path` writes every tick as
JSON lines, `--bench=seconds` prints frame-time statistics and quits, `--uncapped` turns VSync off.

## Regenerating assets

Everything visual and audible is generated by this repository's own tools; the generated files
are committed, so a normal build needs none of this.

```bash
python3 -m venv .venv && .venv/bin/pip install -r tools/assets/sculpt/requirements.txt   # once
godot --headless --path . -s tools/assets/export_rigs.gd    # rigs for the sculptor
.venv/bin/python tools/assets/sculpt/build.py [--only=gorm]  # fighter models (minutes and ~1.3 GB each)
.venv/bin/python tools/assets/gen_audio.py                   # all sound effects and music
python3 tools/poses/common.py && python3 tools/poses/fighters.py   # pose data
godot --path . res://tools/preview/portraits.tscn            # select-screen portraits
```

`tools/preview/pose_sheet.tscn` renders poses and moves for review and `tools/balance/balance.tscn`
runs CPU-against-CPU matches. The playtest tools drive the real game window on an X11 desktop:
`tools/playtest/play.sh` plays a scripted sequence of key presses with screenshots,
`autoplay.sh` lets a simple closed-loop player fight whole matches or an arcade run from the
game's live trace, `versus.sh` adds a virtual gamepad (uinput) as player 2, and `specials.py`
enters every special of a fighter and checks that each one started.

## Documentation

[docs/DESIGN.md](docs/DESIGN.md) (premise, roster, stages, modes),
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (how the code fits together and why),
[docs/VALIDATION.md](docs/VALIDATION.md) (what was tested, measured and played, and what is
known to be missing).
