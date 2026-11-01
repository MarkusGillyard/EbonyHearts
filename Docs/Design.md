# Ebony Hearts — design

The game was called Mortal Slopbat until 2.7.0, when Markus renamed it Ebony Hearts; the
project, its module and the save folder keep the old name inside, and the data still calls the final
boss `slopbat`.

## Premise

The Ebony Heart is said to bestow Divinity upon whoever holds it. It has been lost for exactly
five hundred years. The Outer Gods, bored, have staged the Ebony Heart Tournament for their entertainment: one
worthy mortal may claim the Heart. The fighters gather in the Sump, the low country where every
river in the world ends (black water, reed forests and stilt-towns sinking into living mud), and
at the top of every tower waits a **Fragment of the Ebony Heart**, with Hollis Fathom, its servant,
before it.

Wren is the story's protagonist. The original fighters' arcade endings still speak of asking the boss
for something.

The game takes these stakes seriously, apart from a few chosen moments (the Reflection). There is no fourth-wall commentary and no
AI-themed material.

## Tone and art direction: bog-gothic arcade

- **Silhouette first.** Every fighter is recognisable as a black shape: a lean kickboxer, a
  giant, a hooded assassin trailing a chain, a tall veiled mourner, a long-coated constable in a
  peaked cap, a round-helmeted diving suit, a braided fighter in a short belted jacket, a long-haired
  master in a black robe, and the boss, which keeps no fixed shape.
- **Palette.** Stages in deep teal-black, moss and wet stone, lit by lantern amber and furnace
  orange; supernatural power in sickly green (the Sump), crimson (Ashka), pale
  dirge-blue (Mirela), brass (Hollis), fen-light teal (Wren), Qhi gold (the Grandmaster). Each
  fighter has one dominant
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
- **UI.** Built on the owner's title card: Cinzel and Source Sans 3, worn silver on obsidian, violet
  for focus. Long top bars: health with names, the fighter's meter beneath, round wins, the timer
  between them. `Docs/UI.md` is the design system.
- **Effects.** Short and punchy: impact sparks and rings, a brief light, blood that follows the
  gore setting, dust where bodies land. Reduced effects and no-flash options exist.
- **Content.** Made for adults: intense violence, blood and gore. The Gore setting (Full, Low, Off) is
  the player's choice, and Full is the game as intended.
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
**Hearted** at the right distance (close, sweep or far), or simply hit them. The Rendering
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

| Fighter | Look | Style and role | Specials (planned) | Hearted |
|---|---|---|---|---|
| **Ashka Rook** (she), the Last Lamplighter, 1.70 m | Lean and slim, long crimson hair in a loose braid, with bangs and two strands framing her face, crimson eyes, a calm expression. A white shirt, black tie, black trousers and brown loafers under a long black trench coat. Muay Thai & Flame Style: the Lamplighter Flame, spinning elbows, flying knee, teep and front kicks, clinch strikes and trips, leg and calf kicks. Her crimson cross-shaped lamp tattoo shows through her coat as the flame burns. |
| **Gorm Tallow** (he), the Renderer, 2.12 m | Huge, bald, pale ruddy skin, a pronounced brow and a heavy jaw; bare barrel chest under a thick rubberised leather apron with a reflective stripe, rolled canvas work trousers with reinforced knees, heavy steel-toed boots, strapped iron forearm guards | Sumo and catch wrestling. Grappler: slow, armoured, devastating up close | Vat Press (command grab), Tallow Hide (armour), Rendering Charge (armoured shoulder), Ground Slap (low shockwave), Tallow Slaps (a stepping thrust string that shoves), Rising Belly (armoured anti-air), Belly Slide (under high projectiles), Overarm Throw and Double Wristlock (breakable command grabs, the first a side throw, the second a catch hold), Forearm Smash (armoured), Headbutt (quick and short), Belly-to-Belly (a breakable second command grab, thrown behind him) | Rendered Down: lifts and slams, then stamps them flat |
| **Nix Hollow** (he), Shinobi of the Code Sea, 6'4" (1.93 m) | Muscular, in charcoal, jet black and lime green; deep hood, a cloth mask over nose and mouth, glowing eyes; a kusarigama (sickle and chain) and Code Sea, a jet-black katana with a line of lime-green water, sheathed across his back | Ninjutsu and kusarigama. The fastest, the easiest combos, the lowest damage | Sickle Throw (returns), Sinkstep (sink through the floor and rise behind), Undertow (chain trips the legs), Drowned Drop (air dive), Hidden Weapons, Code Sea, Hypnosis, Illusion | Undertow: drags them down into a pool |
| **Mirela Sorrow** (she), the Keening Nun, 5'5" (1.65 m) | Slim, fair, beautiful; very long black hair, pale blue eyes; a simple nun's outfit: a plain black habit, a white collar, a black veil edged in white. Her motif is a nun praying | Baguazhang and supernatural vocal techniques. Zoner | Lament (slow, large wave), Keen (scream that dazes and passes through projectiles), Grave Float (hover), Veil Step (backward fade) | The Last Note: turns them to salt that crumbles |
| **Bram Ketch** (he), 'Officer Knuckles', Captain of the Lantern Watch, 6'3" (1.91 m) | Lean and muscular, weathered, short grey-brown hair and mutton-chop beard; a long navy watch coat with brass buttons and coat tails, a peaked watch cap, leather boots; a hardwood baton and a lantern at his belt; "The Law is Order." tattooed on his right shoulder, seen in his coat-off outfit | Bare-knuckle boxing and hardwood baton techniques. Rushdown | Signal Flare (high arc / low roll), Baton Trip, Bull Rush, Stand Down (counter) | Lights Out: a baton swing that sends them over the horizon |
| **Hollis Fathom** (he), Guardian of the Ebony Heart, a boss, 2.01 m (his playable design, kept for reference; see below) | An olive canvas diving suit, a round brass helmet with three portholes and a faint green glow inside, a brass breastplate, lead-soled boots, rubber gloves, a harpoon gun | Deep-sea pressure. Heavy zoner and trapper | Harpoon (pulls them in), Anchor Drop (overhead from above), Bubble (slow trap), Vent (steam burst reversal) | Crush Depth: reels them in and the helmet swallows them |
| **Wren Ku** (she), 'Punt', 1.78 m | Lean, muscular and athletic, long brown hair in two braids, ocean-blue eyes, a warm smile. Five outfits (her attire, MMA gear, Muay Thai gear, a BJJ gi, and the unmarked black uniform Bram gives her in the story), chosen on the select screen. Mixed Martial Arts and Look See Do: spinning elbows, flying knee, spinning back kick, spinning heel kick, suplex, tornado kick, the 720 Hook Kick, Spirit Enhanced Strikes (a golden lightning aura). |
| **The Grandmaster** (he), Grandmaster of the Ebony Heart Tournament, 1.83 m | Lean; fair skin, golden eyes, a calm smile, very long jet-black hair that swings with him, a jet-black robe, hands behind his back | Qhi and a lifetime of martial arts: every fighter's shape (and the Fragment's) from his form library, changed even inside a combo, and three instant kills; traditional boxing when the Qhi is spent. Secret, for experts | Still Palm (Qhi bolt), Sinking Palm (Qhi along the floor), Rising Palm (anti-air), Drift (floats over and behind), Weave, Yield (parry and returning palm), Ward (breaks projectiles), Step Aside (an evade), Borrowed Face (becomes the opponent, stronger), the form library (any fighter's shape, the Fragment's), Two Fingers into One Strike, Find the Crack and Heart Stop (instant kills) | Still Water: two fingers to the brow, and they still to standing water and fall |

Rivals (the second-to-last arcade fight), from the original game: Ashka and Nix (the Drowned
Hand put out Ninefathom's beacon), Gorm and Bram (Bram once arrested him for a vat fire he did not
start), Mirela and Hollis (she sang at the funeral of the diver who never came back up). Wren and
the Grandmaster have no rival: none has been decided, so their towers have no rival rung (their
trials are written against Mirela and Wren, `trials.json` "partners").

### Roster sheet: how each one plays

From the fighters' data (`Content/Data/fighters/*.json`, the same files as the original). Walk is
forward/back in m/s; frame data are startup frames at 60 Hz.

| Fighter | Archetype and feel | Strengths | Weaknesses | Signature tools | Counterplay |
|---|---|---|---|---|---|
| Ashka | All-rounder; tall square Muay Thai stance (thumbs at the brows, a slow rock between the feet), walk 2.5/2.05, run 5.4 | 6-frame elbow and knee starters, the Lamplighter Flame at range, a 5-frame anti-air knee, clinch strikes and trips, leg and calf kicks | No armour, average range; the knee and the elbows are punishable on block | Lamplighter Flame and its Flying and Rising Knees, Spinning Elbows, Teep Kick, Front Kick | Block the elbows low-high, punish the Rising Knee's 24 recovery frames |
| Gorm | Grappler; deep wide sumo squat with a straight back and sliding feet, slow walk 1.9/1.6, heavy jump | Command grab (Vat Press), Tallow Hide armour, armoured charge, overhead hammer | Slowest walk and run, almost no zoning, big recoveries | Vat Press, Tallow Hide, Rendering Charge, Ground Slap | Stay out of grab range, zone him, jump the slap, punish the 32-frame grab whiff |
| Nix | Speed and mix-up; oblique ichimonji stance (sunk rear leg, upright spine), fastest walk 2.8/2.4, run 6.0, the quickest first blows; Power 0.88, the lowest damage | One button repeated is a whole combination (LP four times, HP five, HK three), overheads and lows from the same chain, a returning sickle, a teleport behind, a false one, a daze | The lowest damage on the roster; Sinkstep (34 frames), Undertow, Hypnosis (18) are slow and reactable | Sickle Throw, Sinkstep, Undertow, Drowned Drop, Code Sea, Hypnosis, Illusion | Block on reaction to the teleport, crouch the trip, anti-air the dive, step back from Hypnosis, punish Illusion: it does not move him |
| Mirela | Zoner; tall palm stance on bent knees, the torso twisted to the opponent, a circling walk, walk 2.4/2.0, 1040 health | Slow large wave, a scream that passes through projectiles and dazes, hover, a backward fade | Slow startups (13-14 frames), weak up close | Lament, Keen, Grave Float, Veil Step | Close the distance under the wave, punish Keen's recovery, catch the hover landing |
| Bram | Rushdown; upright Prize Ring guard (lead fist out, rear forearm across the body), walk 2.6/2.0, run 5.6 | Fast hooks, a long-reaching Bull Rush (9 active frames), high and low flares, a 3-frame counter stance | Flares are slow (13), the counter loses to throws and lows | Signal Flare high/low, Baton Trip, Bull Rush, Stand Down | Throw the counter stance, block the rush and punish, low-block the trip |
| Hollis (boss) | Hollism: long combinations through the summoned Greatsword and forms and animals; 800 to 1000 health by difficulty, 60% damage taken; walk 2.0/1.7 | Long sequences, cinematic Reckoning for those who fail to block, the pull (Harpoon) from range, a steam reversal | Low health; every sequence ends in a recovery a player can punish; from half health the Fragmented Vessel charges in the open | Ebony Heart Greatsword, The Shark, The Eel, The Anchor, Reckoning, Fragmented Vessel | Block the sequence and punish its end, jump the harpoon, break the Vessel's charge with 111 damage |
| Wren | All-rounder; bladed MMA guard (hands high, lead hand out), a light bounce, walk 2.6/2.15, run 5.6 | Reliable fundamentals at every range: a 1-2 into a head kick, a flying knee, a tornado kick anti-air, a suplex and hip throw that leave her close (6-frame recovery), a counter for highs and mids that becomes an armbar, a slip through projectiles, a feint that cancels into anything, Spirit Enhanced Strikes | The shortest reach, no projectile, average damage; the Spinning Heel Kick (-12) and Flying Knee (-9) are punishable on block; the Armbar Counter loses to lows, throws and projectiles; the Slip Step has no strike invulnerability | Spinning Heel Kick, Flying Knee, Tornado Kick, Spinning Back Kick, 720 Hook Kick, Armbar Counter, Slip Step, Feint | Low-block her strings' kicks, punish the spinning kick and the knee on block, throw or sweep the counter, hit her out of the slip |
| Grandmaster (own body) | Expert; with Qhi in him he stands upright and unafraid, chest open, chin level, both hands calmly behind his back (a hand comes forward only to strike); walk 2.1/1.9, no run (a Qhi glide, hands still behind him), lower jump | The best magical defence (magic does half), Qhi zoning high and low, a 6-frame invulnerable anti-air, a cross-up float, a free weave and parry, a ward against projectiles, an evade, economical normals, every fighter's shape on call, and three instant kills | The worst physical defence (blows do 20% more), weaker blows (95%), everything supernatural costs Qhi, and an empty meter leaves him spent | Still Palm, Sinking Palm, Rising Palm, Drift, Weave, Yield, Ward, Step Aside, Borrowed Face, the form library, Two Fingers, Find the Crack, Heart Stop | Rush him with physical blows, low-hit the weave and Two Fingers, throw the parry, hit the 24- and 30-frame shapeshifts and Find the Crack's 58-frame startup, make him spend |
| Grandmaster (borrowed) | The copied fighter's own moves and second meter: copied from the opponent facing him, Qhi-enhanced (120% damage, 85% taken, 108% speed); remembered from his library, as the fighter is | Everything that fighter does, and his own changes of shape and Heart Stop on top | The shape burns Qhi (3.6 a second; the Fragment's twice that) and every blow it takes (0.05 a point of damage); it breaks into the spent body at zero | The copied fighter's | Hit it: each blow shortens it; make it break |
| Grandmaster (spent) | Traditional boxing, upright and careful: a high guard, fists at the cheekbones, elbows in, chin down; walk 1.7/1.55, no run, a weak hop | Clean combinations (jab, cross, right hook; body jab and cross; a left hook to the body), a slip for highs, a parry for highs and mids with a counter cross | Blows do 50% more, his own do 65%; no kicks, no Qhi, no specials of Qhi, no shapeshift, until 25 Qhi has crept back (about nine seconds, sooner by blocking and parrying) | Jab, Cross, Right Hook, Left Hook to the Body, Slip, Parry | Press him: this is the moment; go low, throw him |

All eight share the original's procedural animation vocabulary, driven from the same pose data:
idle with breathing, walk forward and back, run, crouch, jump (neutral, forward, back, with
somersaults), every normal and special, block high and low, hit reactions by height, juggle,
fall, knockdown, get-up, throw and being thrown, dizzy, KO, intro, win pose and finisher. Stance,
lean, sway and step lengths differ per fighter (their pose files); the bodies are new
(`SourceArt/Fighters`).

## Grandmaster: Qhi, every shape, instant kills and the spent body

His name is his title: **Grandmaster**, Master of Qhi, with no other name given. He is an elderly
master of martial arts and Qhi whose body is failing and whose Qhi is not; Markus's canon makes him
**Neutral**. Nothing undercuts him: he is exactly what his title says. He is the expert's fighter,
with a higher ceiling than anyone and a meter that punishes carelessness. The rules below are
the owner's where he set them and coherent extensions where he asked for them; the data has them
(`Content/Data/fighters/grandmaster.json`, `grandmaster_spent.json`; `FQhiRules` in code).

- **Qhi, not a run meter.** His own body has a Qhi meter (100) and no run meter. Run + forward is a
  glide on Qhi (15 a second); a dial-a-combo link costs 35% of its run cost in Qhi; Still Palm 12,
  Sinking Palm 10, Rising Palm 10, Drift 12, Ward 10, Step Aside 12, the returning palm after a
  Yield 6 (it does not follow if the meter cannot pay). Walking, jumping, normals, Weave, Yield and
  Two Fingers are free. Recovery is slow: 3.6 a second once nothing has been spent for a second; a
  parry or weave that catches a blow gives 5, at most once every two seconds. A round starts full.
- **Damage types.** Every blow has a magical share (0 physical, 1 magical, between them mixed). His
  own body takes 120% from physical blows (the worst in the roster) and 50% from magical ones (the
  best); a mixed blow is weighed by its share.
- **Every shape, and what each costs.** A shape is another fighter's whole fighting definition
  (moves, specials, chains, body, second meter) worn on his own health; every shape keeps his
  changes of shape and Heart Stop on his own inputs; none has a finisher (when a fight is decided
  the shape is let go, at no cost and with no refund). While he wears one the Qhi burns (0.06 a
  tick, 3.6 a second, times the shape's upkeep) and every blow it takes drains 0.05 a point of
  damage; nothing refills it; at zero it breaks into the spent body. The prices:

  | Change of shape | Input | Price | Takes on | Notes |
  |---|---|---|---|---|
  | (a) **Borrowed Face**: the opponent facing him | Down, back, forward + Block | exactly half the meter (50), paid the frame it takes | frame 30 of 40 | Qhi-enhanced: 120% damage, 85% taken, 108% speed; with a run meter (full) if the opponent has one; from his own body only; a mirror (another Grandmaster) is refused; a copy is copied from its source |
  | (b) a **remembered** shape: any fighter's, from his form library | Run or Down + Run with a button (Ashka RN+LP, Bram RN+HP, Wren RN+LK, Gorm RN+HK, Nix D+RN+LP, Mirela D+RN+HP, Hollis D+RN+LK) | 30 | frame 24 of 32 | as that fighter is, not enhanced; from his own body or straight from another shape |
  | (c) **inside a combo** | any of the above, once a blow of his has landed (after its first active frame, the opponent still caught) | that shape's own price | frame 3, and he is free at once | at most two changes in one combo; the combo, its hit count and its damage scaling carry on |
  | (d) the **Fragment's** shape | Down + Run + HK | 60 remembered (50 by Borrowed Face when the Fragment faces him) | frame 24 | upkeep doubled (the Fragment's own `shape_upkeep` 2, however the shape was taken); no second meter at all (it flies); he is no boss in it: thrown, finished, his own health |
  | (e) **Own Face**, back to his own body | Down, back, forward + Block, in any shape | free; nothing refunded | frame 14 of 20 (frame 3 inside a combo) | in a borrowed shape the input is always Own Face, never a copy |

  A change broken in its startup costs 8 and nothing else; one refused (a mirror, the same shape,
  too little Qhi) costs nothing and says why under his bar. From a standstill the new body steadies
  itself for 10 frames before it can act; inside a combo it carries straight on. The form library
  is data (`grandmaster.json` "forms": a target, an input, a price, optionally an upkeep), so a
  fighter added to the game is one line there.
- **Instant kills.** Three, each ending the round whatever the victim's health, each with its
  counterplay:
  - **Two Fingers** (forward, back + HP; free) catches a high or mid blow on frames 4-9 of 34; with
    75 Qhi the catch becomes **One Strike**, an unbreakable hold that kills on its frame 40 (the 75
    is spent). Without the 75 it is only a parry. Lows, throws, projectiles and waiting beat it; the
    Fragment's body cannot be held, so it is only parried.
  - **Find the Crack** (back, forward, back, forward + HP; 90 Qhi): unblockable, a 58-frame startup,
    only begun against an opponent standing free on the ground; any blow in its startup breaks it
    (the Qhi is gone all the same), and jumping or backing out of its short reach avoids it. The
    CPUs, the Fragment's included, strike at it or back away when they see it.
  - **Heart Stop** (forward, down, forward + HP; all his Qhi, at least 30; kept in every shape):
    only to end a combo of at least five hits that used at least two of his shapes, the opponent
    still caught. The counterplay is not to be caught that long; a whiff leaves him spent. The
    trial "Two Shapes, Heart Stop" (jab, Two Fingers' chain, Ashka's shape inside the combo, her
    three-hit string, Heart Stop) is the route.
- **Spent: traditional boxing.** An empty meter drops him into his own body with no Qhi
  (`grandmaster_spent`), and he boxes: an upright, careful orthodox guard (fists at the cheekbones,
  elbows in, chin down), the jab, the cross, a right hook and a left hook to the body, combinations
  (jab into cross into the hook), a slip for highs, a parry for highs and mids with a counter
  cross. No kicks, no Qhi, no specials of Qhi, no shapeshift. Meaningfully weak: 150% from blows,
  90% from magic, 65% damage, slow feet, a weak hop. The hard way back: the meter creeps back at 2.7
  a second, and a blocked or parried blow gives 3 (at most every two seconds); he recentres at 25
  (about nine seconds), short of any shape. No drunken boxing and no
  grappling fallback.
- **His Qhi stance.** With Qhi in him he stands upright and unafraid, chest open, chin level, both
  hands calmly behind his back; a hand comes forward only to strike (the owner's direction, kept).
- **Readable.** Every change of shape has its sound, light and callout under his bar; a borrowed
  shape wears its body with a gold rim and rising motes, thinning as the Qhi runs down; the HUD
  shows his Qhi with a mark at the half, a borrowed shape's run meter with the Qhi beneath it, and a
  pulsing spent bar refilling to its mark; an instant kill is announced and closes in on the blow.
- **Every move in the game.** Through his form library every move of every fighter, the
  Fragment's included, is his; Daxion's techniques are his own now (Step Aside, Two Fingers, One
  Strike, Find the Crack). `Saved/Reports/MoveCoverage.md` (a copy is `Docs/MoveCoverage.md`) lists
  each move and how he reaches it, written by the test that fails if one cannot be reached; only
  finishers (a shape is let go when the fight is decided) and moves the game starts itself are not
  counted.
- **His CPU.** A dedicated pass (fighter AI "qhi", by difficulty): a reserve that grows when he is
  in danger and shrinks when the opponent is nearly beaten; the opponent's shape with the meter
  high, the Fragment's with it full at the best levels; remembered shapes inside combos, chosen for
  the matchup (against a fighter who keeps away, one that rushes in, and the reverse, from each
  fighter's own AI notes) and for a long string of their own; changes of shape on confirmed hits;
  Two Fingers only with 75 Qhi in hand; Find the Crack as a read on an opponent standing guard;
  Heart Stop whenever a combo allows it, cancelled into from the blow that set it up; a boxer's
  guard and retreat when spent; and heavy zoning with his palms from 1.6 m (since 2.7.2 CPUs jump it, so
  it is no longer a free win). Development
  builds count how he used his meter (`FCpuBrain::FQhiTelemetry`: Qhi spent and gained, time,
  damage dealt and taken in each form, shapes taken, changes inside combos, combos by length, each
  kill's chances, tries and kills, the meter left unused, rounds lost by form), logged by the AI
  tests and, with `-SlopAiLog`, at a match's end.

**The secret challenge.** He is not on the select screen at first: a sealed tile and a clue in
Extras ("An old master is said to watch the champions who never need a second chance"). Clear an
arcade tower, any tower, without a continue: after the Fragment's shape sinks, an old man stands on
the still water of the Drowned Chapel. Face him (a CPU tier above Master, his full kit) or walk on
to the ending. Beat him and he is unlocked for good (saved with the progress); lose and the run
ends as it would have, with nothing lost, and he waits for the next clean run.

## Hollis Fathom (boss), 2.01 m

Hollis cannot be picked. He sits before the sole entrance to the Fragmented Palace (the stage
`palace_gate`) and stands before the Fragment in every tower (Mirela meets him once, as her rival, and
their thread is kept) and on alternate boss waves in Survival. One round with no clock, of 800, 900 or
1000 health by difficulty (half for each phase in the story, 200 less in Survival), and he takes 60% of any
blow: low health, high defence. Those numbers are `FGameFlow::ServantHealth`; the `health` in his data is
only a default the boss fights replace. His old moves stay, and
everything here also reaches the Grandmaster's shape of him, except the Vessel.

**Phase one, Hollism** (`FGuardianBrain`). Long combinations through the summoned Ebony Heart Greatsword,
weapons and beasts, each ending in a recovery a player can punish, then a rest to do it in. On his feet
between them he guards as often as his difficulty allows (30, 60 and 85% of blows), never in a recovery,
and he throws whoever stands close and only guards:

| Sequence | Hits | Answer |
|---|---|---|
| Greatsword (F,F+HP) | Cleave, Backhand (mids), Reap (low), Execution (overhead, 50 frames of recovery) | Mix the guard, punish the Execution |
| Beasts (D,F+HK) | The Shark (he dissolves and lunges 4.4 m), The Eel (a low rush), The Anchor (overhead, 52 frames) | Block low for the Eel, high for the Anchor |
| Reckoning (B,F+HP) | A lunge that, if it lands, runs on by itself: the camera comes in on four blows. Blocked, it stops, 40 frames open | Block the first blow |

**Phase two, the Fragmented Vessel** (`FFighter::GuardianTick`), from half his health. He stands and
charges for 400 frames, a gauge filling on his bar. He does not flinch, but every landed blow counts:
111 damage breaks it, three solid hits, and leaves him spent for 150 frames taking half as much again;
he charges again nine seconds later. If it fills, it kills, across the whole stage, guarded or not.

## The Ebony Heart Fragment (final boss), 2.65 m

The last fight of every tower, in the Heartwater, one round with no clock, of 1400, 1650 or 1900 health. It cannot
be thrown. Data still calls it `slopbat` (the Fragment itself is archived whole in `Archive/Slopbat`).

Its body is built by `Tools/blender/build_fragment.py`: an amoeba with long pseudopods, spreading where
it meets the water, on the fighters' rig so every pose and hit box applies. Its skin
(`M_CharacterFragment`) is a window on a galaxy, read in screen space so the stars stay put while it
flows, and it breathes. It looks different to each beholder: colours in `slopbat.json` `observers`, and
to the Grandmaster, or when he wears it, a ringed worm (`slopbat_worm`, `USlopGameInstance::MeshFor`).

It flows through three forms with its wounds (`FFighter::BossFormTick`, untouchable as it changes, its
colours turning over and burning brighter), and `FBossBrain` brings out more each time. Every piece of
Ebony Energy is slow to arrive and shows itself first (`arm` frames, drawn as a ring on the water or a
thread of light):

| Form | Health | Adds | Answer |
|---|---|---|---|
| Water | above 66% | Pseudopod, Undertow (low), Rising Flow (anti-air), Engulf (a grab, breakable), The Tide (a low wave), Swim (it sinks, follows you under the water and rises) | Block low, jump the Tide, walk away from where it surfaces, punish its landing |
| Energy | above 33% | Ebony Orb (huge and slow, a mid, knocks down) and Ebony Seal (a ring under you that erupts, unblockable) | Jump or block the Orb; leave the ring |
| Ebony | below 33% | Ebony Lance (a line of light, then a beam the length of the stage, a high) | Duck it |

Below half health it also has the Ebony Cross, a seal that erupts as a tall cross. Wren holds the
Fighting Spirit of Humanity and deals it a quarter more damage. The Grandmaster can wear its shape
(below); the Fragment itself cannot be held (One Strike's catch only parries it), and Find the Crack can
kill it outright, though it strikes at the long startup when it sees it coming.

## Story mode

Wren's way through the tournament (`story.json` `story`, `FGameFlow::StartStory`): the arcade's ladder
with its own cards before each fight, at the CPU difficulty in Settings. Bram, Gorm, Nix, Mirela,
Hollis at the Palace door (Wren in phase one, then Ashka, her best friend, beside her in phase two, the player as Ashka:
their combined strength), the Fragment. Told by what the fighters are and what people will not say, with very little
dialogue. The Grandmaster appears once, after the Fragment, however many continues it took, and she may
walk past him; his line changes if she needed a second chance. Beaten or not, the story ends after him.

## Daxion (removed)

Daxion, the stranger in white of the Mortal Slopbat versions, is not in Ebony Hearts: Markus
removed him on 29 October 2026 and keeps him for *Slopperfall*. Everything that was his (data,
poses, motion capture, body, portrait, lines) is archived whole in `Archive/Slopperfall/Daxion`,
outside what the game loads or cooks; his encounter's code is in the Git history. His techniques
were carried over to the Grandmaster (Step Aside, Two Fingers, One Strike, Find the Crack); the
archive's README lists where each went, and the move-coverage report checks it.

## Stages

| Stage | Where | Look | Signature light |
|---|---|---|---|
| The Rendering Yards | Tallowmarket, Gorm's home | An iron floor between great glowing rendering vats, catwalks, chains, a brick wall with furnace mouths, smokestacks, drizzle | Green vat glow and orange furnaces |
| The Drowned Chapel | Lowmere's sunken church, Mirela's stage | A flooded nave: ankle-deep water on stone flags, broken pews, columns, a great rose window with moonlight, hundreds of candles on ledges, hanging censers | Moonlight through the rose window; candle warmth |
| Lowmere Stilt Market | The market on stilts over the bog at night, Bram's beat | A boardwalk of planks with rope rails, stalls with awnings and hanging paper lanterns, nets and fish, bog water and reeds below, distant stilt houses with lit windows, mist | Strings of amber lanterns; cold mist |
| The Roost | Beneath the Sump | A cavern of tar and bone: a ledge over a glowing lake, stalactites, tar falls, the drowned hoard glinting, green storm light | Pulsing green lightning |
| The Fragmented Palace | Its one door | Black water before a door of black stone, a line of violet light down its middle, great shards of the palace hanging apart behind: where Hollis keeps his watch, not a Versus stage | Cold starlight, a violet rim |
| The Heartwater | No shore | Shallow black water under a sky of stars, broken stone pillars far out, a band of galaxy behind: where the Fragment waits, not a Versus stage | Cold starlight, a violet rim |

## Modes

Arcade (three towers: Novice, Warrior, Grandmaster, which also set the CPU's difficulty; rival flavour
before the rival; the Reflection, a dark mirror match, in Master; the boss; a short ending
per fighter; points for every fight, from health and time left, flawless victories and
Hearted finishers, with the best total per fighter and tower kept), Trials (every fighter's combos,
checked as they land, progress kept), Survival (an endless ladder, one
round a fight, health carried on with 30% of full given back after a win and 45% after a
flawless one, three waves at each difficulty and then Master, Hollis and the Fragment by turns on every fifth wave;
most waves cleared per fighter kept), Versus (two players, any pairing including mirrors, explicit device assignment),
Training (dummy modes, move lists, reset, frame data, hitboxes, input history), Tutorial
(movement, defence, attacks, a combo, a special, a throw and a finisher), Extras (endings seen,
records, the Grandmaster's clue or, once he is unlocked, his move list), Settings.

Unlocks are cosmetic or narrative: each fighter's alternate colours (finish arcade with them),
the Roost as a versus stage (finish arcade once) and the endings gallery. The one fighter to
unlock is the Grandmaster (above).

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
  topology, UV-mapped skin, eyes, brows, lashes) shaped per fighter, on one game skeleton of 65 bones (twelve of them half joints, which keep a bent limb's width).
  Proportions follow each fighter's height and build: Gorm a 2.12 m heavyweight, Ashka lean and
  athletic, Mirela slim, Bram lean and muscular, Hollis bulked by his suit, Nix tall and muscular.
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
- **UI.** See `Docs/UI.md`. A connected controller's own buttons are shown (the ✕ ○ □ △ symbols on
  PlayStation pads), in prompts, the tutorial, trials, move lists and the input display; Settings > Controls >
  Button prompts holds them to PlayStation, Xbox or PC.
- **Audio.** The original's synthesised set, with recorded CC0 punches and thuds (Kenney) layered
  under hits, blocks and landings; footsteps by floor. Cries and screams have their own volume.
