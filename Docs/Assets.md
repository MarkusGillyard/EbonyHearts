# Assets, rights and notices

Mortal Slopbat is Markus Gillyard's game. Its design, characters, story, data, code, synthesised
sound and the stage and fighter builds made for this rebuild are his (© 2026 Markus Gillyard).
Everything else the game uses is listed here, with where it came from and under what terms.

The machine-readable record is `SourceArt/ThirdParty/ledger.json` (every downloaded pack: source
page, download URL, licence as stated on the source, file hashes, what it is used for) and
`SourceArt/ThirdParty/MakeHuman/PROVENANCE.json` (the MakeHuman assets and the MPFB tool). The
downloads themselves are not in Git; the ledger is enough to fetch the same files again.

## What ships, and under what terms

| What | From | Licence | Used for |
|---|---|---|---|
| Base meshes, skins, eyes, eyebrows, eyelashes, hair | MakeHuman system assets (makehumancommunity.org), shaped with MPFB 2.0.18 for Blender | CC0 1.0 | All eight fighters' bodies |
| Surface textures (26 sets) | ambientCG (ambientcg.com), Poly Haven (polyhaven.com) | CC0 1.0 | Stage surfaces; leather, cloth and metal on costumes |
| Props (barrels, crates, bucket, lantern, lamp) | Poly Haven | CC0 1.0 | Stage dressing |
| Night skies (lakeside_night, kloppenheim_07) | Poly Haven | CC0 1.0 | Ambient light and reflections; the Market's and Yards' skies |
| Impact, interface and RPG sounds | Kenney (kenney.nl) | CC0 1.0 | Hits, blocks, footsteps and UI, layered with the original's own sounds |
| New Rocker | Pablo Impallari, Brenda Gallo, Rodrigo Fuenzalida | SIL OFL 1.1 (`Docs/Licenses/NewRocker-OFL.txt`) | Titles |
| Oswald | The Oswald Project Authors | SIL OFL 1.1 (`Docs/Licenses/Oswald-OFL.txt`) | Body text |
| Droid Sans Fallback | The Android Open Source Project, via the engine's Slate fonts | Apache 2.0 (`Docs/Licenses/DroidSansFallback-Apache-2.0.txt`) | Arrows, ticks and symbols the two typefaces lack |
| Unreal Engine 5.8.4 | Epic Games | Unreal Engine EULA (proprietary, not open source) | The engine |

CC0 asks for nothing, but the credits screen names every source anyway. The credits also carry the
engine's notices: "Mortal Slopbat uses the Unreal® Engine. Unreal® is a trademark or registered
trademark of Epic Games, Inc. in the United States of America and elsewhere." and "Unreal®
Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved."

## Rules that were followed

- Only assets marked CC0 on their own page or in their own bundled licence file were taken; a
  permissive downloader or a "free" label was not taken as a licence. Nothing downloaded was run.
- MPFB itself (GPL code) is a tool that ran in Blender; none of it ships. What ships are meshes
  and textures made from the CC0 MakeHuman assets.
- MakeHuman's skin textures carry a "public domain / MakeHuman.org" stamp in an unused corner;
  it is painted out of every shipped skin. MakeHuman is a trademark (CC0 licenses no trademarks),
  so the name appears only as a credit.
- A few MakeHuman hair and eyebrow textures carry an "author" tag that contradicts their CC0
  header (bob01, bob02, eyebrow009-011). None of them is used.
- Excluded on purpose: Quaternius packs (their current Quaternius Asset License restricts
  redistributing the assets themselves, so they are not used at all), and anything from Markus's
  other projects (Kivanagh, Wyseforge Fighters, Slopperfall) merely because they share an owner.

No rights question is open.
