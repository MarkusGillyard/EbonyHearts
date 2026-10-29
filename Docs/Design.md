# Mortal Slopbat — design

## Premise

The Sump is the low country where every river in the world ends: black water, reed forests and
stilt-towns slowly sinking into living mud. Beneath it sleeps **the Slopbat**, a winged thing of
tar and storm whose dreams make the mud alive. Once a generation it wakes and calls the mortal
world to fight. The fighters it summons battle until one remains; that champion is carried into
its roost. Win, and the Slopbat sleeps another generation and grants the victor one thing from
its drowned hoard. Lose, and the flood rises and a town goes under.

The game takes these stakes seriously. The title is the joke, along with a few chosen moments
(the finishers' name, the Reflection, the stranger's dry humour). There is no fourth-wall
commentary and no AI-themed material.

## Tone and art direction: bog-gothic arcade

- **Silhouette first.** Every fighter is recognisable as a black shape: a lean kickboxer, a
  giant, a hooded assassin trailing a chain, a tall veiled mourner, a long-coated constable in a
  peaked cap, a round-helmeted diving suit, a braided fighter in a short belted jacket, a bald old
  master in a long robe, and the winged boss.
- **Palette.** Stages in deep teal-black, moss and wet stone, lit by lantern amber and furnace
  orange; supernatural power in sickly slop-green (the Sump), ember orange (Ashka), pale
  dirge-blue (Mirela), brass (Hollis), fen-light teal (Wren), Qhi gold (the Grandmaster), violet
  (the stranger). Each fighter has one dominant
  colour that reads against every stage.
- **Materials.** Stylised realism: sculpted bodies with skin, cloth, leather and metal
  read from material, not texture. No photorealism; animation and readability come first.
- **Lighting.** Each stage has a warm key, a cool rim, a soft fill from the camera side so
  fighters always read, fog for depth, and one signature light (vat glow, candles, lanterns,
  green lightning).
- **Camera.** Fixed side view on the fighting plane, about 30° vertical field of view, keeping
  both fighters whole from feet to head with a margin, pulling back for jumps and juggles and
  anchored to the floor, never cutting during play.
  Finishers may take a closer framing and always return.
- **UI.** New Rocker (display) and Oswald (text), bone-white on near-black teal, slop-green for
  focus and selection, amber and rust for warnings. MK-style top bars: health with names, a run
  meter beneath, round wins, the timer between them.
- **Effects.** Short and punchy: impact sparks and rings, a brief light, blood that follows the
  gore setting, dust where bodies land. Reduced effects and no-flash options exist.
- **Finish.** The frame is finished like a dark print: the stage softens a few metres behind the
  fighting plane (high graphics), and a soft vignette and faint grain lie over the fight (can be
  turned off). Each fighter's own colour catches along the edges of the body like a back light.

## Combat (MK3-informed)

Six buttons as on the MK3 panel: High Punch, Low Punch, High Kick, Low Kick, Block, Run. Block is
a button (so there are no cross-ups); standing block stops highs, mids and overheads, crouching
block stops lows and mids. Shared basics as in MK3: uppercut (D+HP), sweep (B+LK), roundhouse
(B+HK), crouching punch and kicks, jump punches and kicks (angled jumps are somersaults), and a
throw (F+LP or B+LP up close; press a punch just after being grabbed to break it).

Running (hold Run with forward) drains the run meter, which refills after a short rest.
Dial-a-combos (preset button strings) also spend run meter, one cost per link, as in MK3: an
empty meter means no run and no chains. Specials use short MK-style motions (D,F+HP; B,F+LK;
F,F+LP; D,U+HK...).

Deliberate adaptations, stated in the move list: a throw break; juggle points and damage scaling
to stop infinites (MK3 had neither); a block button that also works against attacks from behind;
chip damage only from specials and projectiles; one projectile per fighter on screen; counter
hits (a blow that lands during the other fighter's wind-up does 15% more and stuns four frames
longer, called out on the HUD), so reading an opponent pays.

**Combos connect.** The first blow of a combo pushes the other fighter back the full
distance; a move that opens a dial-a-combo pushes 60% of it and every hit after the first
35%, so the strings in the move list reach the body the first hit drove back (before, the
push-back alone put the second link out of reach). Every fighter's combos are set out as
**Trials** (main menu), one at a time against the rival, each hit ticked off as it lands and
every trial proven possible by a test that performs it.

**Impact.** Every hit is felt: hit-stop scaled to the blow (longer when it sends a body
flying), a flash through the victim's body, a ring and sparks from the point of contact, and a
camera that shakes a little on every hit and kicks along the blow on heavy ones. The deciding
blow freezes the fight for a quarter of a second, the camera closes in on the loser, and the
fall plays in slow motion. The combo counter grows warmer as a combo grows and jumps with every
hit.

Rounds last 90 seconds; two round wins take the match. The deciding knockout leaves the loser
dizzy (FINISH HIM / FINISH HER / FINISH IT) for seven seconds: the winner may enter their
**Sloptality** at the right distance (close, sweep or far), or simply hit them. The Rendering
Yards also has a stage finisher (D,D,D+LP up close).

### Loops closed, and one kept for later

Markus's playthrough of 2.6.1 found two infinites inherited from the original's data, both now
closed, with tests that repeat every special inside its cancel window (`Combat.Combo.*`).

- **Ashka's elbow loop (now Bram's, bounded: see below).** Elbow Rush (forward,
  forward + low punch) has a cancel window on frames 11-20 once it lands, and a cancel may start
  any special, itself included. It starts in 9 frames, travels 1.5 m and leaves 24 frames of
  hitstun, so a second rush cancelled from the first lands well inside the first one's hitstun,
  and so on without end: 29 hits for all of Nix's health in play, 42 to 50 in 15 s in the tests,
  mid-stage as well as at the edge. Now a special may cancel into itself only as many times in a
  row as its `self_cancels` allows (0 by default). To give that string to another fighter: a
  rushing special with a cancel window that opens before its hitstun runs out (Elbow Rush:
  `"cancel": [11, 20]`, `"hitstun": 24`, `"startup": 9`, a forward `motion`) and
  `"self_cancels": N` for at most N + 1 rushes in a row; damage scaling (to 40 % from the
  third hit) and the run meter are the only other limits, so N decides how far it goes.
  Since 2.6.7 it is **Bram's Prize Ring Drive** (forward, forward + high punch): a stepping
  straight right with Elbow Rush's timing (9 frames to start, 1.3 m forward, 24 of hitstun, a
  cancel window on frames 11-20 once it lands) and `"self_cancels": 2`, so three drives in a row
  and no more, for about 160 damage; on block it is 7 frames short before the push. A boxer
  driving his man back behind straight rights suits him better than the others: Gorm's charge
  has armour (a loop on it could not be answered), Bull Rush knocks down, and Nix, Mirela and
  Hollis have no rush. His CPU strings it, and his trials end with Three Drives.
- **Mirela's Keen.** It dazes for 70 frames and she is free 31 frames after it lands, so it
  landed again inside its own daze. Now a fighter is dazed once per combo; another dazing hit
  in the same combo is an ordinary one (at most 20 frames of hitstun). Hollis's dazing bubble
  follows the same rule.

## Roster

| Fighter | Look | Style and role | Specials (planned) | Sloptality |
|---|---|---|---|---|
| **Ashka Rook** (she), the Last Lamplighter, 1.74 m | Lean, athletic, brown skin, black ponytail, amber eyes; dark leather sleeveless vest, rust-red silk fight shorts with gold trim and waistband, bone-white hand and shin wraps, gold arm band, a small brass lamp charm | Muay Thai and lamp-fire. Aggressive striker | Ember Shot (fireball), Wick Knee (flying knee), Elbow Rush, Rising Knee (anti-air); the Teep (forward + low kick), a push kick that keeps distance | Last Light: burns the opponent to ash |
| **Gorm Tallow** (he), the Renderer, 2.12 m | Huge, bald, pale ruddy skin, heavy jaw; bare barrel chest under a thick rubberised leather apron, rolled canvas trousers, heavy boots, iron vambraces | Sumo and catch wrestling. Grappler: slow, armoured, devastating up close | Vat Press (command grab), Tallow Hide (armour), Rendering Charge (armoured shoulder), Ground Slap (low shockwave) | Rendered Down: lifts and slams, then stamps them flat |
| **Nix Hollow** (he), Hand of the Drowned, 1.78 m | Slim; charcoal and black wrapped garments, deep hood, a cloth mask over nose and mouth, slop-green sash, glowing green eyes; a kusarigama (sickle and chain) | Ninjutsu. Mix-up and teleports | Sickle Throw (returns), Sinkstep (sink through the floor and rise behind), Undertow (chain trips the legs), Drowned Drop (air dive) | Undertow: drags them down into a slop pool |
| **Mirela Sorrow** (she), the Keening Widow, 1.82 m | Tall, slender, pale; very long black hair; an ivory mourning gown with a long skirt open at the front over ivory leggings, a black lace veil over hair and shoulders, black gloves | Baguazhang palms and a voice that unmakes. Zoner | Lament (slow, large wave), Keen (scream that dazes and passes through projectiles), Grave Float (hover), Veil Step (backward fade) | The Last Note: turns them to salt that crumbles |
| **Bram Ketch** (he), Captain of the Lantern Watch, 1.86 m | Broad, weathered, short grey-brown hair and mutton-chop beard; a long navy watch coat with brass buttons and coat tails, a peaked watch cap, leather boots; a hardwood baton and a lantern at his belt | Bare-knuckle boxing and the baton. Rushdown | Signal Flare (high arc / low roll), Baton Trip, Bull Rush, Stand Down (counter) | Lights Out: a baton swing that sends them over the horizon |
| **Hollis Fathom** (he), the Diving Bell, 1.95 m | An olive canvas diving suit, a round brass helmet with three portholes and a faint green glow inside, a brass breastplate, lead-soled boots, rubber gloves, a harpoon gun | Deep-sea pressure. Heavy zoner and trapper | Harpoon (pulls them in), Anchor Drop (overhead from above), Bubble (slow trap), Vent (steam burst reversal) | Crush Depth: reels them in and the helmet swallows them |
| **Wren Sallow** (she), the Fen Light, 1.68 m | Young, lean and strong; light skin, dark-blonde hair in tight braids; a short, heavy-weave charcoal jacket with cross-collar lapels and forearm sleeves, belted with a faded red belt, over a dark rash-guard top; ankle-length grappling trousers with reinforced knees, laced wrestling shoes, open-finger gloves; a little teal glass at the belt | Mixed martial arts (kickboxing, taekwondo kicks, boxing, judo, an armlock) and marsh-light. All-rounder | Turning Light (spinning back kick), Flying Cross (lunging cross), Lifting Light (spinning crescent anti-air), Bar the Door (catches a high or mid blow into an armlock), Flicker Step (a short blink, forward or back, through projectiles), Wisp Feint (a lit feint that cancels into anything); judo throws | Led Astray: the marsh-lights lead them off into the fog |
| **The Grandmaster** (he), no other name given, 1.70 m | Elderly, East Asian, thin and wiry; a shaved head, heavy brow, high cheekbones over hollow cheeks, deep-set eyes, a grey-white goatee; a long ink-blue cross-collar robe over a bone-grey inner robe, pale-gold cloud embroidery at hem, cuffs and collar, a broad black sash with a jade pendant, black cloth shoes | Qhi and a lifetime of martial arts; drunken boxing when the Qhi is spent. Secret, for experts | Still Palm (Qhi bolt), Sinking Palm (Qhi along the floor), Rising Palm (anti-air), Drift (floats over and behind), Weave, Yield (parry and returning palm), Ward (breaks projectiles), Borrowed Face (becomes the opponent, stronger) | Still Water: two fingers to the brow, and they still to standing water and fall |

Rivals (the second-to-last arcade fight): Ashka and Nix (the Drowned Hand put out Ninefathom's
beacon), Gorm and Bram (Bram once arrested him for a vat fire he did not start), Mirela and
Hollis (she sang at the funeral of the diver who never came back up), Wren and Mirela (one leads
the lost out of the water, the other sings them down into it), the Grandmaster and Wren (he
watched her on the causeway the night the lights followed her; she knew).

### Roster sheet: how each one plays

From the fighters' data (`Content/Data/fighters/*.json`, the same files as the original). Walk is
forward/back in m/s; frame data are startup frames at 60 Hz.

| Fighter | Archetype and feel | Strengths | Weaknesses | Signature tools | Counterplay |
|---|---|---|---|---|---|
| Ashka | All-rounder; tall square Muay Thai stance (thumbs at the brows, a slow rock between the feet), walk 2.5/2.05, run 5.4 | 6-frame elbow and knee starters, a fireball, a 5-frame anti-air knee | No armour, average range; the knee and rush are punishable on block | Ember Shot, Wick Knee, Elbow Rush, Rising Knee | Block the rush low-high, punish Rising Knee's 24 recovery frames |
| Gorm | Grappler; deep wide sumo squat with a straight back and sliding feet, slow walk 1.9/1.6, heavy jump | Command grab (Vat Press), Tallow Hide armour, armoured charge, overhead hammer | Slowest walk and run, almost no zoning, big recoveries | Vat Press, Tallow Hide, Rendering Charge, Ground Slap | Stay out of grab range, zone him, jump the slap, punish the 32-frame grab whiff |
| Nix | Mix-up; slim oblique ichimonji stance (sunk rear leg, upright spine), fastest walk 2.7/2.3, run 5.8 | Overheads and lows from the same chain, a returning sickle, a teleport behind | Low damage per hit; Sinkstep (34 frames) and Undertow are slow and reactable | Sickle Throw, Sinkstep, Undertow, Drowned Drop | Block on reaction to the teleport, crouch the trip, anti-air the dive |
| Mirela | Zoner; tall palm stance on bent knees, the torso twisted to the opponent, a circling walk, walk 2.4/2.0, 1040 health | Slow large wave, a scream that passes through projectiles and dazes, hover, a backward fade | Slow startups (13-14 frames), weak up close | Lament, Keen, Grave Float, Veil Step | Close the distance under the wave, punish Keen's recovery, catch the hover landing |
| Bram | Rushdown; upright Prize Ring guard (lead fist out, rear forearm across the body), walk 2.6/2.0, run 5.6 | Fast hooks, a long-reaching Bull Rush (9 active frames), high and low flares, a 3-frame counter stance | Flares are slow (13), the counter loses to throws and lows | Signal Flare high/low, Baton Trip, Bull Rush, Stand Down | Throw the counter stance, block the rush and punish, low-block the trip |
| Hollis | Heavy zoner and trapper; planted suit (wide flat feet, the helmet turning only with the chest), walk 2.0/1.7, 1080 health | The pull (Harpoon) from range, an overhead from above, a slow trapping bubble, a steam reversal | Slowest normals (9-10 frames), slow projectiles, poor mobility | Harpoon, Anchor Drop, Bubble, Vent | Jump the harpoon, dash in behind the bubble, bait Vent and punish its 30 recovery frames |
| Wren | All-rounder; bladed MMA guard (hands high, lead hand out), a light bounce, walk 2.6/2.15, run 5.6 | Reliable fundamentals at every range: a 1-2 into a head kick, a lunging cross, a crescent anti-air, judo throws that leave her close (6-frame recovery), a parry for highs and mids that becomes an armlock, a blink through projectiles, a feint that cancels into anything | The shortest reach, no projectile, average damage; Turning Light (-12) and Flying Cross (-9) are punishable on block; Bar the Door loses to lows, throws and projectiles; Flicker Step has no strike invulnerability | Turning Light, Flying Cross, Lifting Light, Bar the Door, Flicker Step, Wisp Feint | Low-block her strings' kicks, punish the spinning kick and the cross on block, throw or sweep the parry, hit her out of the flicker |
| Grandmaster (own body) | Expert; with Qhi in him he stands upright and unafraid, chest open, chin level, both hands calmly behind his back (a hand comes forward only to strike); walk 2.1/1.9, no run (a Qhi glide, hands still behind him), lower jump | The best magical defence (magic does half), Qhi zoning high and low, a 6-frame invulnerable anti-air, a cross-up float, a free weave and parry, a ward against projectiles, economical normals (two or three frames less recovery), and Borrowed Face | The worst physical defence (blows do 20% more), weaker blows (95%), everything supernatural costs Qhi, and an empty meter leaves him spent | Still Palm, Sinking Palm, Rising Palm, Drift, Weave, Yield, Ward, Borrowed Face | Rush him with physical blows, low-hit the weave, throw the parry, hit the 30-frame shapeshift startup, make him spend |
| Grandmaster (borrowed) | The opponent's own moves and run meter, stronger by Qhi: 120% damage, 85% taken, 108% speed | Everything the copied fighter does, better | The shape burns Qhi (3.6 a second) and every blow it takes (0.05 a point of damage); it breaks into the spent body at zero | The copied fighter's | Hit it: each blow shortens it; make it break |
| Grandmaster (spent) | Drunken boxing, reeling and wobbling: the body he hides with Qhi shows, hunched, head lolling, hands cupped; walk 1.6/1.45, no run, a weak hop | A reeling weave for highs, a spilled-cup parry, a lurching elbow, a backward tumble to get away | Blows do 50% more, his own do 60%; no Qhi, no specials of Qhi, no shapeshift, until 25 Qhi has crept back (about nine seconds) | Lurching Elbow, Tumble, Reel, Spilled Cup | Press him: this is the moment |

All eight share the original's procedural animation vocabulary, driven from the same pose data:
idle with breathing, walk forward and back, run, crouch, jump (neutral, forward, back, with
somersaults), every normal and special, block high and low, hit reactions by height, juggle,
fall, knockdown, get-up, throw and being thrown, dizzy, KO, intro, win pose and finisher. Stance,
lean, sway and step lengths differ per fighter (their pose files); the bodies are new
(`SourceArt/Fighters`).

## The Grandmaster: Qhi, borrowed faces and the spent body

The Grandmaster has no other name in the game: he was the greatest martial artist of his age and
has outlived it; his body is failing, his Qhi is not. Nothing undercuts him: he is exactly what his
title says. He is the expert's fighter, with a higher ceiling than anyone and a meter that
punishes carelessness. The rules, as the owner set them and as the data has them
(`Content/Data/fighters/grandmaster.json`, `grandmaster_spent.json`; `FQhiRules` in code):

- **Qhi, not a run meter.** His own body has a Qhi meter (100) and no run meter. Run + forward is a
  glide on Qhi (15 a second); a dial-a-combo link costs 35% of its run cost in Qhi; Still Palm 12,
  Sinking Palm 10, Rising Palm 10, Drift 12, Ward 10, the returning palm after a Yield 6 (it does
  not follow if the meter cannot pay). Walking, jumping, normals, Weave and Yield are free.
  Recovery is slow: 3.6 a second once nothing has been spent for a second; a parry or weave that
  catches a blow gives 5, at most once every two seconds. A new round starts full.
- **Damage types.** Every blow has a magical share (0 physical, 1 magical, between them mixed:
  Ember Shot, Lament, Keen, the Storm Shroud and his palms are magical; Wick Knee, the flares,
  Bubble, Vent, Sinkstep and Tar Spit are mixed). His own body takes 120% from physical blows (the
  worst in the roster) and 50% from magical ones (the best); a mixed blow is weighed by its share.
- **Borrowed Face** (down, back, forward + Block) takes the shape of the opponent facing him, on
  frame 30 of a startup he can be hit out of. It costs exactly half the meter, paid the frame it
  takes; with less than half it is refused aloud; broken in its startup it costs 8. The shape is
  the opponent's whole fighting definition (moves, specials, chains, body, run meter, full) with
  Qhi laid in: 120% damage, 85% taken, 108% speed. It has no finisher. While he wears it the Qhi
  burns (3.6 a second, and 0.05 a point of damage taken) and nothing refills it; the same input
  gives his own face back on frame 14 of 20 (nothing refunded). At zero the shape breaks into the
  spent body. A fighter in a borrowed shape is copied from its source, never the copy; the Slopbat
  (its shape cannot be held), the stranger and a Grandmaster in his own shape (a mirror shows only
  itself) are refused, aloud and for free. When a fight is decided the shape is let go.
- **Spent.** An empty meter drops him into drunken boxing (`grandmaster_spent`): 150% from
  blows, 90% from magic, 60% damage, slow feet, no glide, no Qhi move, no shapeshift; only a
  reeling weave for highs, a narrower parry, a lurching elbow and a backward tumble. The meter
  creeps back at 2.7 a second and he stays spent until it reaches 25 (about nine seconds), then
  recentres with what he has: well short of another shapeshift.
- **Readable.** Every change of shape has its sound, light and callout under his bar; a borrowed
  shape wears the opponent's body with a gold rim and rising motes, thinning as the Qhi runs down;
  the HUD shows his Qhi with a mark at the shapeshift's half, the borrowed shape's run meter with
  the Qhi beneath it, and a pulsing spent bar refilling to its mark.

**The secret challenge.** He is not on the select screen at first: a sealed tile and a clue in
Extras ("An old master is said to watch the champions who never need a second chance"). Clear an
arcade tower, any tower, without a continue: after the Slopbat sinks, an old man stands on the
still water of the Drowned Chapel. Face him (Master CPU, his full kit) or walk on to the ending.
Beat him and he is unlocked for good (saved with the progress); lose and the run ends as it would
have, with nothing lost, and he waits for the next clean run.

## The Slopbat (final boss), 2.65 m

A gaunt, towering sovereign of tar: glossy black-green skin traced with glowing green veins, long arms
and claws, a bat's ears and a crown of horn, glowing green eyes, a tattered membrane cloak, and
great bat wings folded at its back. It cannot be thrown and has much more health (1400, 1650 or 1900 by tower), but every
pattern can be learned: claw swipes; Tar Spit (high, duck it); Wing Gust (pushes, no damage);
the Dive (it flies off the top of the screen, its shadow follows you for a second, then it comes
down where the shadow is; move away, and punish while it is stuck in the mud); and below half
health the **Storm Shroud** (inspired by Vessimund's lightning burst in Slopperfall): it gathers
green lightning for most of a second, then lightning strikes all around it. Get out of range, or
hit it while it gathers to break the charge. Its defeat collapses it into slop and the waters
recede; then the champion's ending plays.

## The stranger in white (optional encounter)

Daxion, from the wider Wyseforge mythology, appears without explanation: a tall, graceful man
with light-brown skin, very long black hair, amethyst eyes, a plain ice-white robe and white
boots, a reddish-gold ring on his right middle finger. He is investigating something; what, he
never says. He is not a boss and not a roster fighter: no health bar, no ordinary combat AI.

- **Where.** After any arcade victory won without losing health (a flawless win), the ladder
  offers an optional stop: a stranger in white on the stilt market's far walkway. Approach him or
  walk on; walking on costs nothing. After the first meeting he is also reachable from Extras.
- **What happens.** He stands with his hands behind his back. You may leave at any time (hold
  Block to bow out) or attack. The first attack that would land, he steps aside from:
  "Careful, child." The next, he answers with one strike that ends the encounter (a two-finger
  touch, a moment's study, then one punch). Later meetings remember: "What have you learned,
  child?"; after he has struck you twice, "Interesting." / "You remember?" / "So do you." /
  "It seems I'm not the only immortal bastard around here." (Markus's established line, kept
  verbatim); from then on any attack gets "Apparently, very little." and the strike.
- **Safety.** Losing to him never costs a continue or ends the arcade run; the ladder resumes
  where it was. His memory is one small integer in the progress file that only moves forward.

## Stages

| Stage | Where | Look | Signature light |
|---|---|---|---|
| The Rendering Yards | Tallowmarket, Gorm's home | An iron floor between great rendering vats of glowing slop, catwalks, chains, a brick wall with furnace mouths, smokestacks, drizzle | Green vat glow and orange furnaces |
| The Drowned Chapel | Lowmere's sunken church, Mirela's home | A flooded nave: ankle-deep water on stone flags, broken pews, columns, a great rose window with moonlight, hundreds of candles on ledges, hanging censers | Moonlight through the rose window; candle warmth |
| Lowmere Stilt Market | The market on stilts over the bog at night, Bram's beat | A boardwalk of planks with rope rails, stalls with awnings and hanging paper lanterns, nets and fish, bog water and reeds below, distant stilt houses with lit windows, mist | Strings of amber lanterns; cold mist |
| The Slopbat's Roost | Beneath the Sump | A cavern of tar and bone: a ledge over a lake of slop, stalactites, tar falls, the drowned hoard glinting, green storm light | Pulsing green lightning |

## Modes

Arcade (three towers: Novice, Warrior, Master, which also set the CPU's difficulty; rival flavour
before the rival; the Reflection, a slop-dark mirror match, in Master; the boss; a short ending
per fighter; points for every fight, from health and time left, flawless victories and
Sloptalities, with the best total per fighter and tower kept), Trials (every fighter's combos,
checked as they land, progress kept), Survival (an endless ladder, one
round a fight, health carried on with 30% of full given back after a win and 45% after a
flawless one, three waves at each difficulty and then Master, the Slopbat on every fifth wave;
most waves cleared per fighter kept), Versus (two players, any pairing including mirrors, explicit device assignment),
Training (dummy modes, move lists, reset, frame data, hitboxes, input history), Tutorial
(movement, defence, attacks, a combo, a special, a throw and a finisher), Extras (endings seen,
records, the stranger once met), Settings.

Unlocks are cosmetic or narrative: each fighter's alternate colours (finish arcade with them),
the Roost as a versus stage (finish arcade once), the endings gallery, and the stranger. The one
fighter to unlock is the Grandmaster (below).

## Audio

All sound is synthesised by `tools/assets/gen_audio.py`: layered impacts (a low thump, a
mid-body slap and a high crack), whooshes by weight, blocks, throws, landings, cloth and steps,
projectile casts, menu cues, gongs and drums for round calls, and a restrained music loop per
stage, for the menu and for the boss. There is no synthetic announcer voice; round calls are
text with stingers.

## The Unreal rebuild: art and presentation specification

The design above is unchanged: same roster, stages, modes and rules, the same data files. What
changes is how it is drawn.

- **Bodies.** Real anatomy instead of signed-distance sculpts: MakeHuman base meshes (CC0, all-quad
  topology, UV-mapped skin, eyes, brows, lashes) shaped per fighter, on one 53-bone game skeleton.
  Proportions follow each fighter's height and build: Gorm a 2.12 m heavyweight, Ashka lean and
  athletic, Mirela tall and slender, Bram broad, Hollis bulked by his suit, Nix slim.
- **Costumes.** Modelled garments with their own material slots (leather, silk, canvas, wool,
  brass, iron, rubber), never painted on and never letting skin show through. Silhouette rules
  from the roster table hold: ponytail and wraps, apron and vambraces, hood and chain, veil and
  gown, peaked cap and coat tails, round brass helmet.
- **Materials.** Stylised realism read from colour, roughness and normal detail; CC0 surface sets
  for cloth, leather and metal; one master character material with the fighter's accent rim, the
  hit flash, tints (the Reflection, ash, salt) and a burning or salt dissolve for finishers.
- **Stages.** Modelled sets with bevelled edges built per stage in Blender from the original
  compositions, CC0 surfaces, a CC0 night sky for ambient light; a warm key, a cool rim, a soft
  fill from the camera side, fog for depth and one signature light per stage. The walkable strip
  stays flat and clear; foreground dressing stays low and outside the fighting space.
- **Exposure.** Manual (no eye adaptation), so a stage looks the same from round to round. Key
  light on the fighters about 2–3 lux, point lights in tens of candela.
- **Camera.** As before: side-on, 30° vertical field of view, both fighters whole, springs, small
  shake. Framing is computed for 16:9; wider screens show more stage around the same view.
- **Effects.** Sparks as short streaks, an expanding ring and a soft flash at the contact, a brief
  light, blood or (gore off) pale flakes, dust on landings. Reduced effects thin every burst.
- **UI.** The original's look: New Rocker and Oswald (OFL), bone-white on near-black teal,
  slop-green focus. Button names follow the device in hand (Cross/Circle on PlayStation pads).
- **Audio.** The original's synthesised set, with recorded CC0 punches and thuds (Kenney) layered
  under hits, blocks and landings; footsteps by floor. Cries and screams have their own volume.
