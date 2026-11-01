# Ebony Hearts

> **Early development.** Ebony Hearts is a work in progress: characters, animation, art and balance
> are still changing, and builds are development snapshots, free to download from the
> [Releases page](https://github.com/MarkusGillyard/EbonyHearts/releases).
>
> **Not open source.** Everything here (code, data, characters, story, art, models, animation, audio
> and documentation) is proprietary and all rights are reserved by Markus Gillyard. The repository
> is public so people can follow development; no license is granted to use, copy, modify or
> redistribute any of it (see `LICENSE`). Third-party assets keep their own licences
> (`Docs/Assets.md`).

A bog-gothic arcade fighting game by Markus Gillyard, in the line of Mortal Kombat 3 (the project,
module and save folder keep an older internal name, `MortalSlopbat`). The
Ebony Heart has been lost for five hundred years, and the bored Outer Gods stage a tournament for
one worthy mortal to claim it: six fighters and a secret seventh, Hollis Fathom and a Fragment of the
Heart at the top of every tower, six stages, Wren's story, finishers, combo trials and a full arcade
ladder.

This is the Unreal Engine 5 rebuild of the Godot 4.7.2 game (version 1.1.0, on branch `main`, left
untouched). The rules, roster, moves, story and modes are the
original's, from the same data files, with Ebony Hearts' canon, Wren and the Grandmaster added,
Daxion archived for Slopperfall (`Archive/Slopperfall/Daxion`) and the old boss for its own sake (`Archive/Slopbat`); the fighters, stages, rendering
and menus are new.
`Docs/Preservation.md` lists every part of the original and what became of it.

## Playing

Linux x86_64 with a Vulkan driver. Download the archive from the
[latest release](https://github.com/MarkusGillyard/EbonyHearts/releases/latest), unpack it and run
`./EbonyHearts.sh`; the readme inside (`Docs/Player-README.txt`) gives the controls, save locations and modes.

- **Modes:** Story (Wren), Arcade (Novice, Warrior and Master towers, a rival, the Reflection, Hollis,
  the Fragment, endings, continues, scores), Survival, Versus (each device given to a player), Training (dummy
  behaviours, frame data, boxes, input history), Tutorial, Trials, Extras, Settings.
- **Roster:** Ashka Rook, Wren Ku, Gorm Tallow, Nix Hollow, Mirela Sorrow, Bram Ketch; The Grandmaster
  (secret); bosses Hollis Fathom and the Ebony Heart Fragment.
- **Stages:** The Rendering Yards, The Drowned Chapel, Lowmere Stilt Market, The Roost, The Fragmented
  Palace, The Heartwater.
- **Controllers:** PlayStation and Xbox pads through SDL; keyboard halves for two players on one
  keyboard. What has and has not been tested on real hardware is in `Docs/Validation.md`.

## Building

Needs Unreal Engine 5.8.4 (the scripts expect it at
`/media/cinqic/Cinqic Storage/UnrealEngine/UE_5.8.4`; change `UE` in `Tools/ue.sh` otherwise),
Git LFS for the imported content, and for rebuilding art, Blender 5.2 with MPFB 2.0.18.

```bash
git lfs pull
source Tools/ue.sh
build_editor                      # compile
run_tests                         # automation tests, headless
run_game                          # play from the editor build
package_linux Shipping            # cook and package
Tools/make_release.sh Shipping    # archive with readme, licences, version and checksums
```

## Documents

| | |
|---|---|
| `Docs/Design.md` | The game's design (from the original) and the rebuild's art direction; the roster sheet |
| `Docs/Preservation.md` | Every feature of the original and its disposition here |
| `Docs/Martial-arts.md` | The study of real martial arts and fighting games behind the combat's look and feel, with the ranked changes |
| `Docs/Architecture.md` | Code layout, content pipeline, commands, development flags, saves |
| `Docs/Assets.md` | Third-party assets, licences and notices; `SourceArt/ThirdParty/ledger.json` is the full record |
| `Docs/UI.md` | The interface's design system and how to change it |
| `Docs/Validation.md` | Tests, playthroughs, controller and performance evidence, known limits |
| `Docs/Handoff.md` | Where things stand and what is next |

## Ownership

© 2026 Markus Gillyard, all rights reserved: `LICENSE` is the original game's licence, unchanged
but for where it points for third-party notices. Third-party assets keep their own licences
(`Docs/Assets.md`). Unreal® Engine, Copyright 1998 – 2026, Epic
Games, Inc. All rights reserved; Unreal® is a trademark or registered trademark of Epic Games,
Inc. in the United States of America and elsewhere.
