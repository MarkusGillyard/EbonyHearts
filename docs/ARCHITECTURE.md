# Architecture

Godot 4.7.2, GDScript, Forward+ (OpenGL 3 fallback). Almost everything is built in code from
data; the only scene files are the root, the test runner and the authoring tools.

## Layers

```text
data/basics.json, data/fighters/*.json ─► FighterDef / MoveDef / Command      (scripts/combat)
data/looks/*.json, data/poses*/        ─► Rig, PoseLibrary, CharacterModel   (scripts/anim)

keys / pads ─► PadReader ─┐
CpuBrain / BossBrain ─────┼─► FightSim.step(pads) ──► events ──► Hud, Vfx, Audio, FinisherDirector
TrainingDummy / Tutorial ─┤      60 Hz, no scene tree              StrangerRules (hooks)
StrangerRules ────────────┘            │
                                       ▼
                         Fighter state ──► FighterView: pose, mirror, IK, feet, cloth ──► model
```

The rule that holds it together: **the simulation never reads the scene, the clock or input
devices, and nothing visual changes the simulation.** Every outcome is an event dictionary.

| Directory | Responsibility |
|---|---|
| `scripts/combat/` | `FightSim` (phases, collision, hit and block, throws, projectiles, KO and finish flow), `Fighter` (one fighter's state machine), `MoveDef`, `FighterDef`, `Command` (arcade notation and motion matching), `InputBuffer`, `Projectile`. Pure data and arithmetic; tests run it headless. |
| `scripts/ai/` | `CpuBrain`, `BossBrain`, `TrainingDummy`, `StrangerRules`, all producing ordinary pad input through `PadIntent`. |
| `scripts/anim/` | `Rig` (joints and two-bone IK), `Pose`, `PoseLibrary`, `CharacterModel` (the sculpted, skinned body), `ClothSheet` (Verlet cloth and hair), `Shapes`. |
| `scripts/view/` | `Match` (steps the sim, owns the scene, pause and device handling, the knockout freeze and slow motion), `FighterView`, `FightCamera` (framing, shake, the sprung kick, depth of field), `Arena` and `arenas/*`, `Vfx` (sparks, rings, flashes, blood, projectiles), `PostFx` (vignette and grain), `FinisherDirector`, `Props`, `DebugDraw`. |
| `scripts/ui/` | `Hud`, every menu screen (each a `Screen`), `FighterPreview`, the tutorial. |
| `scripts/core/` | Autoloads: `Content` (data), `Settings`, `Progress` (saves), `Audio`, `Game` (flow, the arcade ladder and its scoring, the survival ladder); `Main` (screen host); `PadReader`. |

## Combat timing

- One simulation tick is 1/60 s (`physics/common/physics_ticks_per_second`). Rendering runs at the
  display rate; views interpolate between the last two ticks (Settings → Motion smoothing).
- A tick: read both pads into the input buffers (recording whether each fighter is frozen in
  hit-stop), advance the round phase, turn idle fighters to face each other, tick both fighters,
  advance throws, move projectiles, separate pushboxes, clamp to the stage and the camera's
  maximum separation, then resolve contact: every hitbox–hurtbox and projectile contact is
  gathered first and applied afterwards, so trades are symmetric. Then the round is checked.
- Moves are data: startup, active and recovery frames, level (high, mid, low, overhead,
  unblockable, throw), damage and chip, hit-stun and block-stun, hit-stop, pushback, effect
  (stagger, launch, knockdown, sweep, crumple, pull, daze), hitboxes and extra hurtboxes as
  rectangles in the fighter's forward/up space, root motion and lift curves, invulnerability,
  armour, low-profile and airborne windows, chain links with run cost, special-cancel windows,
  projectiles, throw paths and finisher frames. Shared basics are authored for a 1.8 m body and
  scaled by height, so bigger fighters reach further.
- **Inputs.** Directions are stored as seen on screen with the facing of each tick. Held-direction
  commands read the direction at the press; motions are matched backwards from the press with
  lenient direction groups, a step gap of 14 ticks and a 32-tick window, read both with each tick's
  facing and with the current one so a motion that crosses over still works. Presses are buffered
  for 6 ticks, and ticks spent in hit-stop do not count, so a press made during hit-stop is never
  lost. A plain button pressed a tick or two before the rest of a chord or motion upgrades to the
  more specific command.
- **Combos.** Damage scales from the third hit (10% per hit, to 40%); juggle points (an uppercut
  costs 3) end a juggle at 7 and juggled bodies fall faster with every hit, so there are no
  infinites. Chains cost run meter per link, as in MK3.
- **Rounds.** 90 seconds; two wins (one long round for the boss). The deciding KO leads to the
  dizzy FINISH phase (7 s); double KOs and draws award nothing, and a match never exceeds nine
  rounds.
- **Counter hits and hit-stop.** A hit on a fighter whose own attack has not reached its first
  active frame is a counter: damage x1.15 (before scaling), hit-stun +4, hit-stop +2. Default
  hit-stop is 5, 8 or 11 ticks by damage, +2 for launches, knockdowns and sweeps. Hit events carry
  `counter` and `ko` (the blow emptied the health bar), which the presentation reads.
- **Survival** (`Game.configure_wave`) is a one-round match per wave with `p1_health` carried
  in (applied by `Match.start`), `difficulty` by wave and the boss every fifth wave; the arcade's
  points come from `Game.fight_score(result)`, with the result's `health` and `time_left`.

## Drawing a fighter

`FighterView.update_visual` picks the pose source from the state (stance, crouch, block, run
cycle, move keyframes at fractional frames, reactions built for the stun's length, juggles,
knockdowns, throws), crossfades from what was on screen, **mirrors the pose when the fighter faces
left** (as a sprite game would, so the same side of the body always faces the camera), adds lean,
breathing, dizzy sway and hit springs, plants the feet so they do not slide, solves the IK, copies
the rig into the sculpted model's skeleton and steps the cloth. Somersaults, juggle tumbles and
knockdown spins rotate the whole body about the hips. Presentation effects (tint, flash, dissolve,
alternate colours) are shader parameters.

The character models are generated offline (`tools/assets/sculpt/`): a signed-distance sculpt of
the body and outfit from the look data, meshed at 2 mm, decimated to about 120,000 triangles,
skinned by distance to the rig's bones, with materials baked per vertex. Rigid pieces (a helmet,
claws, rings) are meshed separately at a finer voxel, over the box their surface actually
occupies (sampled first, since a cuff cut from a whole arm inherits the arm's box) and within a
fixed grid budget, so a build peaks at about 1.3 GB. `test_content` fails when a model is older
than its look.

## The frame

`Match` holds the fight still for 14 frames on the deciding blow (18 before a finish), then
steps the simulation every third frame for 50, while `FightCamera` closes in on the loser;
`round_start` undoes both. The camera's shake is a small sine on top of its sprung position and
its kick a sprung displacement along the blow, both off with the screen-shake setting. On the
high graphics setting the camera carries `CameraAttributesPractical` with far depth of field
starting 3.4 m behind the fighting plane (the distance follows the camera every frame). `PostFx`
is a `CanvasLayer` under the HUD with one full-screen quad multiplying a vignette and a grain
(`assets/shaders/post.gdshader`); Settings "post_fx" hides it.

## Saves and settings

`Settings` stores preferences, key bindings and device assignments in `user://settings.cfg`
(read key by key with defaults). `Progress` stores arcade records, unlocks, endings seen and the
stranger's memory as versioned JSON in `user://progress.json`, written to a temporary file and
renamed; an unreadable file is moved aside to `progress.json.bad` and a fresh profile is used.
`--profile=name` keeps development and test runs in `progress_<name>.json`. `user://` is
`~/.local/share/MortalSlopbat/` (`config/use_custom_user_dir`).

## Decisions worth knowing

- **Godot 4.7.2.** Already installed with its Linux export templates on the development machine,
  proven there by two earlier games of the owner's; MIT licensed; native Linux export as one
  self-contained executable. Unreal (disk and size) and Unity (licensing, Linux workflow) were not
  needed for a game of this scope.
- **Sculpted-in-code characters** instead of downloaded or AI-generated assets: full control of
  silhouettes and a clean provenance; animation as data plus IK keeps hitboxes and poses in one
  coordinate system (`test_animation` checks every strike's limb against its hitbox).
- **2D rectangles for collision** on a 3D-rendered plane: readable, deterministic and exactly
  what a side-view fighter needs.
- **No announcer voice.** Synthetic speech of acceptable quality was not available offline;
  round calls are large text with stingers.
- **Adaptations from MK3**, listed in the move lists and README: throw breaks, juggle limits and
  damage scaling, chip only from specials, one projectile per fighter.

## Adding a fighter

1. `data/fighters/<id>.json` (stats, moves, commands, chains via `basic_tweaks`, finisher, ai) and
   the id in `Content.ROSTER`; `data/looks/<id>.json`; an outfit class in
   `tools/assets/sculpt/outfits.py`; poses in `tools/poses/fighters.py`; story in
   `data/story.json`.
2. Generate the rig and model, regenerate poses and the portrait, then run `tools/run_tests.sh`.
