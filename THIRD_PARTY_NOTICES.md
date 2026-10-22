# Third-party notices and asset provenance

Mortal Slopbat's own code, data, characters, story, models, animation, stages, sound and music
are proprietary (see [LICENSE](LICENSE)). The components below are **not** owned by Markus
Gillyard; each keeps its own license, whose full text ships in the [`licenses/`](licenses/)
folder of this repository and of every distributed build.

| Component | Version / source | License | Where it is used | Obligations |
|---|---|---|---|---|
| Godot Engine | 4.7.2-stable, <https://godotengine.org> | MIT | The engine and export template inside the Linux executable | Include the Godot license (`licenses/GODOT_LICENSE.txt`). |
| Godot's bundled third-party libraries (FreeType, HarfBuzz, Jolt Physics, mbed TLS and others) | As listed in Godot 4.7.2's `COPYRIGHT.txt` | Various permissive licenses (MIT/Expat, FTL, Apache-2.0, BSD, zlib and others) | Compiled into the engine | Include their notices: `licenses/GODOT_COPYRIGHT.txt` reproduces the upstream list. |
| New Rocker (font) | Google Fonts repository, `ofl/newrocker/NewRocker-Regular.ttf`, © 2011 Pablo Impallari, Brenda Gallo, Rodrigo Fuenzalida | SIL Open Font License 1.1, Reserved Font Name "New Rocker" | Titles and announcements (`assets/fonts/NewRocker-Regular.ttf`, unmodified) | Ship the license (`licenses/NewRocker-OFL.txt`); do not sell the font by itself; keep the name for unmodified copies. |
| Oswald (font) | Google Fonts repository, `ofl/oswald/Oswald[wght].ttf`, © 2016 The Oswald Project Authors | SIL Open Font License 1.1 | Menus and HUD text (`assets/fonts/Oswald-Variable.ttf`, unmodified, renamed file) | Ship the license (`licenses/Oswald-OFL.txt`); do not sell the font by itself. |

## Original material and how it was made

Nothing in the game comes from *Mortal Kombat* or any other game; MK3 was studied only as a
design reference (controls, run meter, dial-a-combos, finishers, ladder structure).

| Material | Made by | How |
|---|---|---|
| Fighter models (`assets/characters/*.wfm`) | This project's code | Signed-distance sculpting in `tools/assets/sculpt/` from each fighter's look (`data/looks/`) and rig, meshed, skinned and baked by the build script. No scans, base meshes or purchased assets. |
| Daxion (the stranger in white) | Markus Gillyard's Wyseforge mythology | Appearance and lines follow Markus's canon notes (the Wyseforge cosmology and the Slopperfall Daxion reference); his model is sculpted here. |
| Animation | This project | Key poses written as data (`tools/poses/`, `data/poses*`, move keyframes in `data/fighters/`), driven by procedural IK, foot planting, springs and cloth at run time. |
| Stages, effects, UI art, icon | This project | Built in code from primitives and shaders (`scripts/view/`, `assets/shaders/`); the icon and portraits are rendered by `tools/`. |
| Sound effects and music | This project | Synthesised by `tools/assets/gen_audio.py`; nothing sampled or recorded. |
| Rig, IK, cloth and sculpting code | Markus Gillyard's earlier game *Wyseforge Fighters* | Copied from that private project (same owner) and adapted. |

## Development tools (not distributed)

Python 3 with NumPy, SciPy, scikit-image, fast-simplification and Pillow (pinned in
`tools/assets/sculpt/requirements.txt`) for sculpting, sound generation and the icon; Python
with python-xlib and Pillow for the X11 playtest driver; the Godot 4.7.2 editor and export
templates (MIT) to build. None of their code or data ends up in the game.
