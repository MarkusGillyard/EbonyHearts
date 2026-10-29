# Daxion, archived for Slopperfall

Daxion (the Man in White) came from Markus Gillyard's Wyseforge mythology and was intended for
*Slopperfall*. On 29 October 2026 Markus removed him from *Ebony Hearts* (the game formerly called
Mortal Slopbat). Nothing here is loaded, cooked or shipped by Ebony Hearts: the game reads
`Content/Data` and cooks `Content`, and this folder is outside both. It is kept whole, with its
history, for Slopperfall.

## What is here

| File | What it was in the game |
|---|---|
| `Data/fighter.json` | His fighting definition: stats (1.95 m), `no_basics: all`, the three moves below, `ai.stranger` |
| `Data/look.json`, `Data/poses.json` | His look (violet accent) and his own key poses |
| `Data/poses_mocap_evade.json` | The ten captured poses of his evade (`mc_dax_evade_02` to `_11`), from ACCAD Male 2 `E19_DodgeLeft` (CC BY 3.0, credit as in `Docs/Assets.md`) |
| `Data/story.json` | The encounter's offer, approach, walk-on and bow lines |
| `Content/Fighters_daxion/` | The imported Unreal assets: skeletal mesh, skeleton, physics asset, material instances and textures (Git LFS). Copy into a project's `Content` to use them |
| `Content/T_portrait_daxion.uasset`, `portrait.png` | His select-screen portrait |

His body is rebuilt from source by the fighter pipeline: the `daxion` entry in
`Tools/blender/fighters.json` and `daxion()` in `Tools/blender/fighter_outfits2.py` (both kept;
`SourceArt/Fighters/daxion/daxion_report.json` records the last build). The encounter's code
(`FStrangerRules`, the offer screen, his lines and the Find the Crack close-up) is in the Git
history up to commit `59e61c6` on branch `ue5-grandmaster-wren`.

## Where his moves went

Every authored technique was carried over to the Grandmaster, adapted to the game's rules (in
the encounter they were scripted answers that could not be fought):

| Daxion's move | In Ebony Hearts | Change |
|---|---|---|
| `dax_evade` "Step aside" (18 frames, invulnerable throughout, 0.85 m back) | Grandmaster's `step_aside` | Costs 12 Qhi; 20 frames, invulnerable on frames 3-12 only, so it is a read, not a wall; the same capture retimed (`mc_step_aside_*`, `Tools/mocap` clip `step_aside`) |
| The encounter's catch, "a two-finger touch, a moment's study" | Grandmaster's `two_fingers` (a catching parry) | Catches a high or mid blow on frames 4-9 |
| `dax_palm` "Palm" (unblockable, 9999 damage) | `one_strike`, the blow after Two Fingers | A true instant kill, only with 75 Qhi to pay when the catch lands |
| `dax_crack` "Find the Crack" (58-frame startup, unblockable, 9999) | Grandmaster's `find_the_crack` | A true instant kill: 90 Qhi, only begun against a standing opponent, broken by any blow in its startup |

Ebony Hearts checks this list: `grandmaster.json` "carried" names each archived move and the move
it became, and the move-coverage test fails if one is missing or not reachable
(`Docs/MoveCoverage.md`).

His dialogue and the encounter itself (the flawless-win offer, the walkway, the memory of earlier
meetings) are not used in Ebony Hearts.
