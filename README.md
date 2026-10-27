# Mortal Slopbat

A bog-gothic arcade fighting game by Markus Gillyard, in the line of Mortal Kombat 3: six
fighters, the Slopbat waiting at the top of every tower, a stranger in white who is not part of
the tournament, four stages in the Sump, finishers, combo trials and a full arcade ladder.

This is the Unreal Engine 5 rebuild of the Godot 4.7.2 game (version 1.1.0, at
`~/Documents/Mortal Slopbat`, left untouched). The rules, roster, moves, story and modes are the
original's, from the same data files; the fighters, stages, rendering and menus are new.
`Docs/Preservation.md` lists every part of the original and what became of it.

## Playing

Linux x86_64 with a Vulkan driver. Unpack the release archive and run `./MortalSlopbat.sh`; the
readme inside (`Docs/Player-README.txt`) gives the controls, save locations and modes.

- **Modes:** Arcade (Novice, Warrior and Master towers, a rival, the Reflection, the Slopbat,
  endings, continues, scores), Survival, Versus (each device given to a player), Training (dummy
  behaviours, frame data, boxes, input history), Tutorial, Trials, Extras, Settings.
- **Roster:** Ashka Rook, Gorm Tallow, Nix Hollow, Mirela Sorrow, Bram Ketch, Hollis Fathom; the
  Slopbat; Daxion, the stranger in white.
- **Stages:** The Rendering Yards, The Drowned Chapel, Lowmere Stilt Market, the Slopbat's Roost.
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
| `Docs/Architecture.md` | Code layout, content pipeline, commands, development flags, saves |
| `Docs/Assets.md` | Third-party assets, licences and notices; `SourceArt/ThirdParty/ledger.json` is the full record |
| `Docs/Validation.md` | Tests, playthroughs, controller and performance evidence, known limits |
| `Docs/Handoff.md` | Where things stand and what is next |

## Ownership

© 2026 Markus Gillyard, all rights reserved: `LICENSE` is the original game's licence, unchanged
but for where it points for third-party notices. Third-party assets keep their own licences
(`Docs/Assets.md`). Unreal® Engine, Copyright 1998 – 2026, Epic
Games, Inc. All rights reserved; Unreal® is a trademark or registered trademark of Epic Games,
Inc. in the United States of America and elsewhere.
