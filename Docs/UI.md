# Interface: design system and screen flow

How Ebony Hearts looks and moves on screen, and how to change it. `Source/MortalSlopbat/UI` is the
implementation; this file explains it. Everything here is native Slate, driven by the input router,
with no Blueprints, UMG, CommonUI or level content. `Docs/Validation.md` has the evidence.

## The authority: the title card

`SourceArt/UI/EbonyHearts_TitleCard.png` is Markus's title card, 2172 x 724 RGBA, byte-identical to
the file he supplied (`1000012807.png`, SHA-256 starts `2856a3520ba7def7`). Never redraw, retype,
recolour or stretch it; it is drawn 3:1 on the title, main menu and credits, imported uncompressed
(`T_ui_logo`) with mips so it stays clean when small. Its edges are clean on dark: no defringing was
needed. Its middle word is dark metal, so anywhere it stands on a dark field a low violet glow sits
behind it (`ETex::Glow`), which lifts the word from the night without touching a pixel of the card.

Everything else in `SourceArt/UI` is generated from the card and the tokens by `Tools/ui_art.py`:
rerun it, then import (below). `logo_energy.png` is the card's violet veins blurred into a halo that
breathes behind it; it is drawn under the card, never over it.

## What was taken from where

Studied from real footage where it could be reached: low-resolution frames of a Mortal Kombat vs. DC
Universe Xbox 360 playthrough (YouTube, RQ87, "Mortal Kombat Vs. DC Universe playthrough (Xbox 360, 1CC)",
0:46 to 1:58). MobyGames' stills sit behind a verification page and were not used. Elden Ring's HUD rule is
from Bandai Namco's starter guide; its menus were not viewed for this work, so that part is general
knowledge of the game, not a study of frames.

| Seen | Taken as a principle | Not taken |
|---|---|---|
| MK vs DC menu: small list at left on a thin bracket, one live 3D fighter in the world at right | Main menu: list on a rail, a live fighter in the hall | Rooftop, art, type |
| MK vs DC select: huge full-body fighters left and right, two rows of small portraits at the bottom | Fighter select | Layout proportions, portraits |
| MK vs DC ladder: stacked cards, each with the opponent | Towers: a column of portrait cards | Card art |
| MK vs DC loading: full-screen 3D scene, thin bar; a fly-through of the stage before the fight | The staging: the camera finds the stage first, on the real set | Pre-rendered clips, the cut |
| MK vs DC HUD: long thin bars, centred medallion clock, combo text small at the side | Long bars, a clock plaque, disciplined combo readout | Green bars, red FIGHT, portraits on the bars |
| Elden Ring: contextual HUD outside combat | Quiet menus, the world in sight; but a fight's bars never hide | Its stats, its HUD hiding |

The one thing MK vs DC's designers said they wanted (Kikizo interview, part 3) was continuity from
cinematic into fight. The face-off here goes further than theirs could: it is one unbroken camera
move on the real stage with the real fighters, so nothing is loaded or cut between it and round one.

## Tokens (`UI/SlopStyle.h`)

Colours were sampled by k-means over the card's opaque pixels, and only then given meanings.

| Token | sRGB | Use |
|---|---|---|
| `Ink` | 10 7 16 | Curtain, gradients, under bars |
| `Panel` | 14 9 21 at 84% | Blocks of text over the world |
| `Raised` | 44 26 66 at 90% | A focused row's fill |
| `Line` | 133 122 138 at 55% | Hairlines, quiet frames |
| `Text` | 232 225 236 | Everything primary |
| `Dim` / `Faint` | 174 162 184 / 108 98 116 | Secondary / disabled |
| `Accent` | 136 77 186 | The focus sweep: sparing |
| `Spark` | 210 125 242 | Small marks only: the focus mark, a damage trail, a small heading |
| `Second` | 124 196 214 | Player 2's cursor, always with a "2" beside it |
| `Warn` / `Danger` / `Good` | 232 168 76 / 218 62 72 / 126 200 160 | Warnings, low health, ready |
| `Qhi` | linear 1.0 0.78 0.3 | The Grandmaster's Qhi and nothing else |
| `Run` | 96 150 232 | The run meter and nothing else |

Stage and fighter colours (rims, the Sump's green) are the world's own and do not pass through these.

**Type.** Cinzel (OFL, Roman inscriptional capitals, the same bones as the card) replaced New Rocker,
a blackletter that fought the card. Source Sans 3 (OFL) replaced Oswald, whose condensed shapes tire
long copy. `Display` is Cinzel SemiBold, `Call` Cinzel Bold for round calls, the clock and big numbers,
`Body` and `Strong` Source Sans 3 Regular and SemiBold, `Caps` tracked SemiBold for small labels. The
files in `Content/Fonts` are static instances of the variable fonts, because Slate draws a variable
font's default instance only. Button symbols and arrows come from the engine's Droid fallback face.
`Settings > Comfort > Text size` multiplies every size, so sizes are never hard-coded outside these.

**Space.** Layout is in units of a 1920 x 1080 canvas that grows on the long side of any other shape
(`StageScale`, `StageSize`, `SStage`): 3440 x 1440 gets a canvas 2580 wide, 1440 x 1080 one 1440 tall.
Nothing is ever stretched or cropped. `Settings > Video > Screen margin` insets the whole design by up
to 5% for a TV that crops its edges (`SetScreenMargin`, `StageFrame`), while the world, veils and
letterbox still reach the edge (`SStage.FullBleed`). Pieces are pinned to anchors (`Place`), so a corner
stays a corner; side margins are 84 to 130 units, the foot's 40. Corners are cut, never rounded, like the
card's serifs (`ETex::Frame`, nine-sliced).

**Motion.** Menus drift slowly; focus moves in about 0.1 s; a new screen takes 0.2 s; going into a load
takes 0.2 s to ink and 0.42 s back, and a new round dips to ink for 0.4 s over the jump to the marks.
`EaseOut` for arrivals, `EaseInOut` for travel. Input is never
locked by an animation except while the curtain is closing on a load: a press there could start the
same thing twice, so router input waits, the screen being left stops ticking, and any other way out
(the mouse) cancels the queued fade. With `Effects: Reduced`, ambient motion stops and focus and the hall jump
rather than travel. Round calls never flash the screen.

## Widgets (`UI/SlopWidgets.h`)

`Shell` is a whole screen: canvas, veil, prompts. `Menu` is a list on a rail with a mark that eases
along it. `Heading`, `Title`, `Rule`, `PanelBox`, `CapsLabel`, `Prompts` (a button chip plus what it
does) and `Motes` cover the rest. Add to these rather than styling inside a screen.

**Buttons.** `Router->Glyphs()` is the one answer to which buttons to show: the choice in `Settings >
Controls > Button prompts`, or on Automatic a connected pad's own, even after a key was pressed. Write
a button through `ActionKey`, or move notation through `ButtonsIn`; a page that writes them into its
text is rebuilt when a pad comes or goes. A DualShock 4's button says Share, a DualSense's Create.

Three veils: `Side` (ink at the left, the world in sight), `Full` (for pages of text) and `Foot` (a
composed scene, with text on the foot's gradient).

## The hall (`View/SlopShowroom`)

One place behind every menu: a wet stone floor, a far violet sky (`T_ui_hall` on `M_Hall`), a low
haze. `Goto` empties it and each screen composes it in `Enter` with `SetLayout` and `Show`; the camera
eases between layouts, so changing screens never changes the scene. Figures are centred by their hips,
because a stance carries them half a metre off their mark. The view keeps 16:9's width on narrower
screens and shows more of the hall on wider ones.

## The staging before a fight (`FFaceOff`, `ASlopMatch::BeginPrelude`)

`GI->EnterFight(true)` fades to ink, starts the match, and shows `FFaceOff` over it. The match holds
the round's intro (`FFightSim::IntroHold`) while `PreludeStep` moves the real camera: the ground, the
first fighter, across to the second, then both, and `ClearOverride` lets the springs carry it into the
live framing as round one is called. No fighter or stage is duplicated, and `FFaceOff` holds only the
words. Confirm, Back or Pause skips at any moment; pausing freezes it. `Settings > Pre-fight intro:
Short` plays only the last shot. Rounds after the first, and Rematch, have no staging.

`IntroHold` only delays the round. `Slop.Rounds.HeldIntroChangesNothingInTheFight` proves that.

## The fight HUD (`UI/SlopHud`)

Painted directly each frame from the simulation. `Slop::FHudReadout` / `Slop::HudReadout(F)` is the
one place that decides what a fighter's meters show, and it is tested without a screen:

| Fighter | Meters |
|---|---|
| Anyone with a run meter | Run, blue |
| Grandmaster, own body | Qhi, gold, with a notch and tooth at the half a shapeshift costs |
| Grandmaster, borrowed shape with a run meter | That shape's Run, and the Qhi that holds it beneath |
| Grandmaster, borrowed shape without one (the Fragment) | Qhi, labelled with the shape |
| Grandmaster, spent | Pulsing red refilling toward the mark where it recentres, labelled |
| Hollis | Nothing until the Fragmented Vessel fills; then its gauge; `BROKEN` when broken |

Names always read the fighter's own identity, whatever shape they wear. No state is colour alone:
bars are scored in quarters, wins are filled or hollow marks, the Qhi's half has a tooth, spent and
low states are labelled or pulse, players carry `P1`/`P2`/`CPU`/`DUMMY` tags.

Bars stay inside the middle 1920 units on a wide screen, so the eye does not travel; on a narrow one
they stay at the top. The top 130 units are the HUD's; the fight camera's framing keeps heads below
them. Combos sit under the bars at the side; form changes under the meters; round calls on a dark band
in the upper third, clear of both fighters' heads and hands at normal framing.

Cost: static textures, no per-frame texture or widget creation. Names, the clock and shape labels are
cached and rebuilt only when they change. Animated text scale stays quantised to sixteenths in
`PaintText`, because each new scale rasterises new glyphs (see the comment there).

Training readouts sit on their own panels in plain type, apart from the fight's own marks.

## Screen flow

Title → Main Menu → (Story → fight) · (Arcade → Select → Tower → fight) · (Survival → Select → fight)
· (Versus → Devices → Select → Stage → fight) · (Versus CPU → Select, opponent → Stage → fight) ·
(Training → Select, partner → Stage → match) · (Tutorial → match) · (Trials → Select → match) ·
(Extras → Endings, Records, Move Lists, Credits) · Settings. A fight is the staging, Match, Results.

Screens are in `SlopScreens.cpp` (title, menu, list screens, pause, results) and `SlopScreensFlow.cpp`
(select, stage, tower, face-off, devices, moves, extras, ending, credits); settings has its own file.
Every screen's logic, timers, gating and save behaviour were kept as they were.

## Changing things

- A colour: its token in `SlopStyle.cpp`; if a texture carries it, also `Tools/ui_art.py`.
- A generated texture: edit `Tools/ui_art.py`, run it (`numpy`, `scipy`, `Pillow`), then
  `SLOP_IMPORT_ONLY=ui editor_py "$PROJ_DIR/Tools/import_assets.py"`, and revert the
  `AndroidFileServer` block the editor adds to `Config/DefaultEngine.ini`.
- A new texture: add to `ETex` and the table in `Brush`; the `ui` import step finds it by file name.
- Screenshots at any shape with no window: `SLOP_OFFSCREEN=1 RESX=3440 RESY=1440 Tools/playtest/run_flow.sh tour`.
