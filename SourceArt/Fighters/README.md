# Fighters (source art)

One folder per fighter, built by script from `Tools/blender/fighters.json`. Git keeps this file and
each fighter's report; the FBX, textures, `.blend` and renders are rebuilt by the scripts and are
not versioned (the game's imported copies are under `Content/Slop/Fighters`). After a build each
folder holds:

| File | What it is |
|---|---|
| `<id>.fbx` | The fighter for Unreal: body, costume, hair and props as separate meshes, all skinned to one `game_engine` skeleton, 65 bones with the half joints. Textures are **not** embedded. |
| `<id>_<slot>_basecolor.png` | sRGB base colour. RGBA for the face/hair slot: alpha is the opacity mask. |
| `<id>_<slot>_normal.png` | Tangent-space normal, **DirectX convention (green down)**, as Unreal expects. |
| `<id>_<slot>_orm.png` | R = ambient occlusion, G = roughness, B = metallic. Linear data. |
| `<id>_<slot>_emissive.png` | Only where something glows: Ashka's eyes and her lamp charm. sRGB. |
| `<id>.blend` | The assembled scene in metres, for inspection. Rebuild from the script rather than editing it. |
| `<id>_report.json` | Triangle, vertex and weight stats per part, material slots, height fit, timings. |
| `renders/` | Review renders, re-imported from the FBX: front, side and three-quarter views, `<id>_portrait.png` (head and shoulders from the front right, the select-screen framing) and five poses. |

Status: all ten are built: ashka, gorm, nix, mirela, bram, hollis, slopbat, daxion, wren, grandmaster.

| Fighter | Height | Triangles | Material slots (atlas size) |
|---|---|---|---|
| ashka | 174 cm | 46,428 | skin 2048, hairface 2048, leather 1024, cloth 2048, metal 512 |
| gorm | 212 cm | 37,336 | skin 2048, face 1024, leather 2048, canvas 2048, iron 1024 |
| nix | 178 cm | 53,378 | skin 2048, face 1024, cloth 2048, props 1024 |
| mirela | 182 cm | 58,270 | skin 2048, hairface 2048, gown 2048, leather 1024, veil 1024 (alpha lace: veil and bodice lace) |
| bram | 186 cm | 50,950 | skin 2048, hairface 2048 (incl. beard fur), coat 2048, leather 1024, props 1024 |
| hollis | 195 cm | 43,600 | skin 1024, face 512, suit 2048, metal 2048, leather 1024 |
| slopbat | 265 cm | 36,852 | skin 2048 (+ emissive veins), face 512, membrane 2048, horn 1024 |
| daxion | 195 cm | 47,280 | skin 2048, hairface 2048, cloth 2048, metal 512 |
| wren | 168 cm | 65,980 | skin 2048, hairface 2048 (incl. procedural braids), jacket 2048, cloth 2048, belt 512, leather 1024 |
| grandmaster | 170 cm | 60,024 | skin 2048, hairface 2048 (incl. goatee fur), robe 2048, garb 1024, props 512 |

The height is floor to the top of the skull; helmets, caps, horns and hair go above it (`top_of_everything_cm` in each report).

## Rebuild

```bash
Tools/blender/build_fighters.sh ashka gorm nix mirela bram hollis slopbat daxion wren grandmaster
Tools/blender/build_fighters.sh gorm         # one fighter (no ids: ashka + gorm)
# or the two steps by hand:
Tools/blender/run_blender.sh Tools/blender/build_fighter.py -- gorm
Tools/blender/run_blender.sh Tools/blender/render_fighter.py -- gorm
```

- `run_blender.sh` runs Blender 5.2 headless with `--factory-startup` inside a memory-capped systemd scope (5 GB). It also points every Blender user path at `Tools/blender/.blender_user/`, so `~/.config/blender` is never touched.
- The isolated config holds the MPFB 2.0.18 add-on.
- A build takes about 20–25 s and peaks at about 2.3 GB. A render pass takes about 8 s.
- Inputs that must exist:
  - `SourceArt/ThirdParty/MakeHuman/extracted/makehuman_system_assets/`: MakeHuman system assets.
  - `SourceArt/ThirdParty/ambientCG/<set>/`: the ambientCG sets. `python3 -I Tools/blender/fetch_ambientcg.py` downloads them, records their licence evidence and adds them to `SourceArt/ThirdParty/ledger.json`.
- The scripts:
  - `fighter_core.py`: MPFB body, shape targets, height fit, rig.
  - `fighter_outfits.py`: Ashka's and Gorm's garments.
- `fighter_outfits2.py` and `fighter_wardrobe.py`: the next six and the shared garment library.
  - `fighter_outfits3.py`: Wren and the Grandmaster (crossed collars, procedural braids, the goatee).
  - `fighter_geo.py`: the mesh tools.
  - `fighter_tex.py`: the textures.
  - `fighter_render.py` and `render_fighter.py`: the review renders and poses.

## FBX conventions

- Centimetres (`UnitScaleFactor` 1.0), Z up, character faces −Y. This is Blender's native axis system, so no axis conversion is baked in.
- The armature node is named `Armature`, which Unreal's FBX importer drops for Blender files. The first bone is `root`, at the floor.
- No leaf bones, deform bones only, face smoothing groups, tangents and binormals.
- The bone names are the UE4 mannequin's, without its twist and IK bones (`game_engine` rig from MPFB, root renamed `root`).
- Twelve helper bones sit on the limb joints, `shoulder_half_l`, `elbow_half_l`, `wrist_half_l`, `hip_half_l`, `knee_half_l`, `ankle_half_l` and the same on the right. Each takes the middle of its joint's skin blend and turns half way between the bones either side, so a bent or twisted joint keeps its width (`Tools/blender/fighter_joints.py`; `FMeshRig::Pose` turns them in the game).
- The only other extra bones are the Slopbat's wing bones: `wing_l_01` (parent `clavicle_l`) → `wing_l_02` → `wing_l_03`, and the same on the right. The 53 standard bones keep their names and hierarchy.
- The wings rest folded at the back. The membrane is weighted from the spar bones (outer edge) to `spine_03`/`spine_02`/`pelvis` (inner edge), so rotating `wing_*_01` opens the wing.
- Standing height is floor to the top of the skull, boots included:

| Fighter | Standing height | How |
|---|---|---|
| Ashka | 174.0 cm | barefoot |
| Gorm | 212.0 cm | 209 cm body plus a 3 cm boot sole; all bones except `root` are lifted by the sole |

- The FBX stores texture file names. Blender also writes the absolute path of the build machine. Unreal finds the PNGs beside the FBX.

## Unreal material notes

- Each fighter has at most 6 material slots; each slot is one atlas. Build a material per slot from the PNG set:
  - Use the ORM channels as AO, roughness and metallic.
  - Leave the normal maps as DirectX (sRGB off).
  - Make `hairface`/`face` **masked** with base-colour alpha as the opacity mask, two-sided, for the hair cards, brows and lashes.
  - Wire the `emissive` PNGs into emissive colour. The glow colours are in the look files (`glow`).
- The skin slot is MakeHuman's diffuse texture, tinted. It needs a skin shader (subsurface) in Unreal.
- Texture sizes and slots:

| Fighter | Slots |
|---|---|
| Ashka | skin 2048, hairface 2048, leather 1024, cloth 2048, metal 512 |
| Gorm | skin 2048, face 1024, leather 2048, canvas 2048, iron 1024 |

## What each part is made from

The body for every fighter:

- MPFB base mesh (hm08, 13,380 vertices) with macro sliders plus region shape targets (listed per fighter in `fighters.json`), fitted to the exact height.
- MPFB `game_engine` rig and its skin weights.
- Body faces fully covered by tight clothes are deleted (MakeHuman's delete-group idea). Texels of the deleted faces are refilled with skin tone.
- Eyes `high-poly`, eyebrows and eyelashes are MakeHuman assets, fitted and weighted by MPFB and recoloured into the face atlas.

Garments are meshes authored by the script on that body. Most are **shells**: copies of the body faces they cover, so they carry the body's own skin weights and UVs. They are pushed out along the normals, smoothed, kept outside the body (and outside earlier garments), cut along clean plane or level-set outlines, and given thickness with Solidify. Rigid props are weighted 100% to one bone.

**Ashka Rook**

| Part | Made from | Weights |
|---|---|---|
| Vest (dark leather, `Leather021`) | Torso shell: cropped at the ribs, V neck, scooped back, straps over the shoulders | Body |
| Lacing | Four gold crosses (tubes) on the front | Rigid `spine_03` |
| Fight shorts (rust-red silk, `Fabric040`) | Pelvis/thigh shell, flared radially (wide Muay Thai legs) with folds | Body; the leg tubes follow the thighs |
| Gold hem bands and waistband | Separate shells | Body |
| Hand wraps | Hand/wrist shell to 40% up the forearm | Body |
| Shin and foot wraps (`Fabric001`) | Shin/foot shell, sole left bare | Body |
| Gold arm band (`Metal042A`) | Lofted ring from cross-sections of the left upper arm | Rigid `upperarm_l` |
| Brass lamp charm | Lathe-turned parts with an emissive ember globe | Rigid `spine_03` |
| Cord | Tube around the neck | Transferred from the body |
| Ponytail | MakeHuman `ponytail01`, recoloured black, pushed out over the vest | MPFB's interpolated weights (head, neck, clavicles) |

**Gorm Tallow** (bald)

| Part | Made from | Weights |
|---|---|---|
| Scalp | The skin's painted hair is repainted with forehead tone | — |
| Rubberised leather apron (`Leather026`), front only, to the knees | A sheet that lies on the chest and belly and swings back towards the thighs below the belt. Centre slit from the hem to mid-thigh. | Bib: transferred from the body (Data Transfer, nearest face interpolated). Below the belt: blends to 100% of that side's thigh by mid-thigh, so each half rides its own thigh in kicks and crouches |
| Neck strap | Ribbon round the back of the neck | Transferred from the body |
| Belt | Ring shell lifted over the apron at the front | Body |
| Iron buckle and rivets (`Metal055A`) | Small solids | Rigid `pelvis` / `spine_03` |
| Rolled canvas trousers (`Fabric066`) | Pelvis/leg shell with folds and a rolled cuff over the boot tops | Body |
| Heavy boots (`Leather021`) | Lofted through convex cross-sections of the foot and shin (rounded toe box) | Transferred from the body |
| Soles | Slab under each boot | `foot`→`ball` blend |
| Iron vambraces | Forearm shells with two raised bands | Rigid `lowerarm_l` / `lowerarm_r` |


**Nix Hollow**

| Part | Made from | Weights |
|---|---|---|
| Wrapped top with sleeves (`Fabric001`) | Torso and arm shell | Body |
| Deep hood and capelet (`Fabric034`) | Head/neck/shoulder shell with a face opening, decimated. The ears are cut out of the shell and the holes closed with patches pushed onto a smooth cap over the side of the skull; the body's ears are removed under it, so no ear shape shows | Body |
| Cloth mask | Face shell over the nose and mouth | Body |
| Loose trousers (`Fabric066`) | Leg shell gathered into the shins | Body |
| Shin wraps, forearm bands, hand wraps, split-toe foot wraps | Shells and section rings | Body; bands transferred from the body |
| Chest cross straps | Ribbons | Transferred from the body |
| Slop-green sash (`Fabric040`) | Band with a knot at the left hip | Body; knot rigid `pelvis` |
| Two sash tails | Ribbons | `pelvis` → 50% `thigh_l` |
| Kusarigama (`Wood051` handle, `Metal055A` blade and chain) | Wood handle, steel blade, 14-link chain with a weight | Rigid `hand_r` |

**Mirela Sorrow**

| Part | Made from | Weights |
|---|---|---|
| Ash-ivory gown (`Fabric040`, `#b6b0a1`: greyer and darker than her skin) | Bodice with a high collar to the throat and long sleeves in one shell; the upper sleeves puffed out (leg-of-mutton) and gathered under the shoulder | Body |
| Black cuffs, choker | Section rings at the wrists and over the collar top | Transferred from the body |
| Black overbust corset (`Fabric040`) | Shell over the bust and waist (bust flattened under it), straight across the front, dipping under the arms, pointed at the front hem, shallow seam grooves | Body |
| Ivory piping | Strips laid on the corset's outer surface along both edges and down ten seams (centre-front busk, side and back seams) | Transferred from the body |
| Black close lace (`veil` slot) | Shell from the corset top to the collar over the chest, shoulders and upper back; alpha lace generated in code (flowers on a net, about 70% cover) | Body |
| Long skirt | Two wrap-over panels from the waist (tucked under the corset), overlapping at the front and back; the panels share leg weights where they overlap, so the front stays closed | Body above the hips, then `pelvis` with a thigh share falling from front to back; below the knee the hem rides the shins and the pelvis share is capped by height, so it stays above the floor in a deep crouch |
| Leggings (`#a7a193`) | Leg shell, smoothed and decimated (mostly hidden) | Body |
| Black gloves (`Leather026`), short boots | Hand shell including fingers; boot loft | Body |
| Black lace veil | Wrap sheet over the hair, open at the face, alpha lace generated in code | Transferred from the body |
| Hair | MakeHuman `long01`, lifted clear of the corset and piping where it lies on the back | MPFB |

**Bram Ketch**

| Part | Made from | Weights |
|---|---|---|
| Navy watch coat (`Fabric034`) | Torso and sleeve shell | Body |
| Coat tails | Wrap sheet open at the front, back vent | `pelvis`/thigh blend |
| Stand collar, epaulettes, dark cuffs | Section rings and plates | Rings transferred from the body; epaulettes rigid `clavicle_*` |
| Double-breasted brass buttons (`Metal042A`) | Lathe parts | Rigid `spine_02`/`spine_03` |
| Black belt with brass buckle | Band shell; plate | Body; buckle rigid `pelvis` |
| Trousers, polished boots (`Leather026`) | Leg shell; boot loft | Body |
| Peaked watch cap | Crown lofted over the head and hair, band, leather peak | Rigid `head` |
| Face and skin | About 55: MakeHuman age 0.74 plus head-age, eye-bag, laugh-line, heavier-brow and nose-hump targets. Skin is the middle-aged MakeHuman skin with 60% of the old one blended in for lines, tinted to the weathered `#b5806a`, ruddier on the cheeks, nose, ears and brow, with a stronger detail normal | Body |
| Mutton chops (`#5f5449`, roughness 0.85) | Thick sideburns down the jaw joined by a moustache, chin clear. Two fur shells (3 mm and 9 mm off the skin) cut along an outline round the head, with a generated strand texture in the hairface atlas: an opaque core that breaks into single strands, sparse outer strands, alpha fading to a ragged edge | Body |
| Hair (`#6e6256`), brows (`#5c5047`) | MakeHuman `short02` and `eyebrow005`, recoloured so the opaque strands match the look colours; the scalp under the hair is painted in the hair colour | MPFB |
| Baton (`Wood051`) | Lathe part | Rigid `hand_r` |
| Lantern at the left hip | Brass frame, emissive amber glass | Rigid `pelvis` |

**Hollis Fathom**

| Part | Made from | Weights |
|---|---|---|
| Inflated olive canvas suit (`Fabric066`) | One shell over the whole body below the neck, except the hands | Body |
| Rubber gloves and cuffs (`Leather026`) | Hand shells and rings | Body |
| Brass corselet | Shell; the edge dips to a point front and back | Body |
| Round brass helmet | Three portholes with emissive green glass and guard bars on the side ports | Rigid `head` |
| Belt, lead-soled boots | Band shell; boot loft | Body |
| Harpoon gun | Lathe parts | Rigid `hand_r` |
| Head | Only the face is kept inside the helmet | Body |

**The Slopbat**

| Part | Made from | Weights |
|---|---|---|
| Body | Gaunt MPFB body: very long arms and fingers, scaled to 2.65 m. Black-green glossy skin with procedurally generated glowing veins (emissive map). | Body |
| Folded bat wings | Membrane plus three spars each (`Leather026`) | Wing bones (see FBX conventions) |
| Tattered membrane cloak | Wrap sheet from the shoulders, open at the front, ragged hem | `pelvis`/thigh blend |
| Crown of seven horns, bat ears | Lofted parts | Rigid `head` |
| Claws on every finger and toe | Lathe parts | Rigid `*_03_*` / `ball_*` |
| Wing thumb claws | Lathe parts | Rigid `wing_*_01` |

**Daxion**

| Part | Made from | Weights |
|---|---|---|
| Ice-white robe (`Fabric001`) | Torso and sleeve shell with a V opening and bell sleeves | Body |
| Collar band along the V | Ribbons | Transferred from the body |
| Sash | Band shell | Body |
| Robe skirt | Wrap sheet open at the front | `pelvis`/thigh blend |
| White trousers and boots | Leg shell; boot loft | Body |
| Hair | MakeHuman `long01` | MPFB |
| Reddish-gold ring on the right middle finger | Section ring | Rigid `middle_01_r` |

**Wren Sallow**

| Part | Made from | Weights |
|---|---|---|
| Spats (`Fabric034`, `#202226`) | Leg shell, close fit. Reinforced knees: an oval panel over each kneecap painted into the spats' texture (darker, smoother, a dashed stitched seam) and padded out 3.5 mm | Body |
| Dark rash guard (`Fabric034`) | Torso and arm shell to a crew neck, sleeves ending under the jacket's | Body |
| Judo-style jacket (`Fabric066` heavy weave, `#33373a`) | Torso and sleeve shell with a crossed V (left panel over right), wide sleeves cut at mid-forearm, rolled cuffs (section rings); below the belt two wrap-over panels to the hip | Body; the panels' thigh share starts at the belt, so the hem rides the thighs in kicks |
| Thick lapel (`Fabric066`, darker) | One band along the crossing and round the back of the neck, lifted where the outer lapel crosses the under one | Transferred from the body |
| Faded red belt, knot and two front tails (`Fabric001`, `#6f2826`) | Band shell over the jacket and its panels, flattened lathe knot, ribbons; five rows of stitching painted in | Band: body; knot rigid `pelvis`; tails `pelvis` to 85% of that side's thigh |
| Teal glass bead (`#5ef2d6`, emissive) | Lathe sphere near the end of the left tail | Transferred from the tail |
| Open-finger grappling gloves (`Leather026`) and wrist straps (`Leather021`, oxblood) | Hand shell cut across the first finger segments and the thumb, padded knuckle bar; section rings | Body |
| Laced wrestling shoes (`Leather021`), thin soles | Close loft of the foot and ankle; crossed lace tubes up the instep | Transferred from the body; soles `foot`→`ball` |
| Tight braided hair (`#5f4c3a`) | Procedural, no MakeHuman hair: a cap of pulled-back strands over the scalp (feathered hairline, the ears cut out) and two Dutch braids of overlapping lobes from the front hairline over the head, hanging to the shoulder blades; strands drawn into the hairface atlas | Cap: body (head); braids rigid `head` on the scalp, blending to the neck and upper back below the nape |
| Face and eyes | Young MakeHuman female, firmer jaw and cheekbones; `green` eyes tinted hazel with a faint fen-light teal emission | Body |

**The Grandmaster** (bald)

| Part | Made from | Weights |
|---|---|---|
| Ink-blue jiaoling robe (`Fabric040`, `#1b2333`) | Torso and sleeve shell with a deep crossed V (left panel over right, the edge running to the right of the waist); sleeves widening below the shoulder and tapering to the cuffs, a little extra hang under the forearm; the front hangs from the chest as loose cloth (the body's chest is flattened under it) | Body; the sleeve shaping only round the arm itself |
| Bone-grey inner robe (`Fabric001`) and its collar | Torso shell with a higher, narrower crossing showing at the neck; band along it | Body; band transferred |
| Robe skirt | Three panels from the waist to 8.5% of the height: front-under, back, and the front-outer flap whose edge continues the collar line. Side slits from the hip down: the front panels share 70–80% with the thighs, the back 20–30%, so the slits open in kicks and wide stances | `pelvis`/thigh blend; below the knee the hem rides the shins (stays above the floor in a crouch) |
| Edge bands with pale-gold embroidery (`#d6c38e` on `#10141d`) | Collar band (crossing and round the neck), cuffs (section rings) and the skirt's hem and front edge. The embroidery is painted procedurally: a double border and a running scroll of ruyi clouds | Collar transferred from the body; cuffs transferred; skirt as above |
| Broad black sash (`Fabric040`, `#0e0e11`), knot at the left front, two flaring ends | Band shell over the robe and skirt panels; lathe knot; ribbons | Body; knot rigid `pelvis`; ends `pelvis` to 70% `thigh_l` |
| Jade bi disc on a cord under a bronze bead, right hip | Flattened torus, tube, lathe | Rigid `pelvis` |
| Wide dark trousers gathered at the ankle (`Fabric034`) | Leg shell | Body |
| Black cloth shoes on pale layered soles (`Fabric066`, `Fabric001`) | Close loft of the foot; sole slab | Transferred from the body; soles `foot`→`ball` |
| Goatee and thin moustache (`#c8c2b6`) | Two fur shells as Bram's mutton chops, outline set from the face's own profile (nose, lips, chin): a thin moustache over the upper lip drooping past the mouth corners, a goatee from under the lower lip over the chin, and a fur cone for its point (about 6 cm) | Body (head); the point rigid `head` |
| Face and skin | About 77: MakeHuman age 0.93 with heavy brow, high cheekbones over hollow cheeks, narrow deep-set eyes, a downturned mouth and a strong jaw, a lowered, rounded crown (targets in `fighters.json`); `old_asian_male` skin, scalp repainted with forehead tone plus faint age mottling; `eyebrow008` in grey; dark brown eyes with a faint gold emission | Body |

## Rights of every input

Everything that ships is either CC0 or first-party. Nothing in the FBX/PNGs comes from GPL code.

| Input | Licence | Evidence |
|---|---|---|
| MPFB 2.0.18 add-on code | GPL-3.0-or-later. A **development tool only**, not shipped. MPFB's `LICENSE.md` §D: the MakeHuman team claims nothing over its output (exports, renders, scripted geometry). | `SourceArt/ThirdParty/MakeHuman/downloads/mpfb2_repo_license_files_v2.0.18/LICENSE.md`; `PROVENANCE.json` |
| MPFB bundled assets: base mesh, targets, `game_engine` rig and weights | CC0 1.0 | `LICENSE.md` §C (same folder); `data/3dobjs/base.obj` header; `weights.game_engine.json` `"license": "CC0"` |
| MakeHuman system assets used: skins `young_african_female`, `young_caucasian_male`, `young_caucasian_female`, `young_asian_male`, `middleage_caucasian_male`, `middleage_african_male`, `old_caucasian_male`, `old_asian_male`; eyes `high-poly`/`low-poly` with `brownlight`, `brown`, `grey`, `green`, `lightblue`, `deepblue` textures; eyebrows `001`, `002`, `003`, `005`, `006`, `008`; eyelashes `01`–`03`; hair `ponytail01`, `long01`, `short02` | CC0 1.0 | `packs/makehuman_system_assets.json` (all 94 assets CC0) and the CC0 header in every asset file; SHA-256 and URLs in `SourceArt/ThirdParty/MakeHuman/PROVENANCE.json` |
| ambientCG `Leather021`, `Leather026`, `Fabric040`, `Fabric001`, `Fabric066`, `Fabric034`, `Metal042A`, `Metal055A`, and `Wood051` (an existing stage entry, reused for the baton and sickle handle) (1K JPG) | CC0 1.0 | Ledger entries `ambientcg/<id>` in `SourceArt/ThirdParty/ledger.json`, with saved asset pages, SHA-256 and the four rights conclusions |
| Garment geometry, recoloured and atlased textures, procedural lace, veins, beard fur, braided hair, cloud embroidery and stitching, build scripts | First-party (Mortal Slopbat) | Generated by `Tools/blender/` |

Notes:

- **Trademarks.** MakeHuman assets carry a "MakeHuman™" tag, and CC0 §4(a) licenses no trademarks. The skin textures' "MakeHuman.org / public domain" stamp is painted out of every shipped skin PNG. Don't use the MakeHuman name in marketing.
- **Avoided assets.** Five pack textures carry a `Doctor Hell` author tag (`bob01`, `bob02` hair; `eyebrow009`–`011`). They aren't used here; keep avoiding them until their provenance is confirmed.
- **Excluded sources.** Nothing from Quaternius, Kivanagh, Wyseforge Fighters or Slopperfall is used.
- **Look references** come from the game repo (`docs/DESIGN.md`, `data/looks/*.json`) and are first-party.

## Known limitations

- **Deformation** is plain linear-blend skinning on a skeleton without twist bones.
  - Elbows and knees lose some volume; the hip crease folds in deep flexion.
  - The apron halves are rigid on the thighs, so they read as stiff tassets rather than cloth.
  - Chaos Cloth in Unreal would be the next step for the apron, the shorts legs and the ponytail.
- **Hair, veil, sash tails, coat tails and skirts** are skinned, not simulated.
- **Nix's chain** and other props are rigid to one bone.
- **Long skirts and tails** follow a share of the thighs: in high kicks and crouches they lift like panels. Coat tails and the other long skirts can dip below the floor in a deep crouch; Mirela's skirt hands its hem to the shins and stays above it (lowest skirt vertex 2.2 cm above the soles in the review crouch).
- **The fist poses** in the review renders are rough: the pose library is a deformation check, not animation.
- **Rebuilds are not all bit-identical.** Ashka, Gorm and Nix come out slightly different from run to run: Blender's nearest-surface queries break ties differently, which moves a few vertices of some shells (vest, waistband, belt, hood, mask) by up to about 2 cm and changes the UV packing. Mirela, Hollis, the Slopbat and Daxion rebuilt identically in checks.
- **Vest straps** come to small peaks where they meet the neck, and the waistband's lower edge ripples slightly.
- **Wren**: the braids are rigid on the scalp and skinned, not simulated, below the nape; seen edge-on the braid lobes read slightly beaded. The jacket's skirt panels are short, so a raised thigh still passes partly through the front panel in the review kick.
- **The Grandmaster**: in the deep crouch the knees press through the front skirt panels (as with the other long skirts); with both arms high the sleeves bunch round the elbow. The robe and sash ends sway through the material only.
