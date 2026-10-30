# Ebony Hearts — design

The game was called Mortal Slopbat until 2.7.0, when Markus renamed it Ebony Hearts; the
project, its module and the save folder keep the old name inside, and the Slopbat remains the
boss at the top of every tower.

## Premise

The Ebony Heart is said to bestow Divinity upon whoever holds it. It has been lost for exactly
five hundred years. The Outer Gods, bored, have staged a tournament for their entertainment: one
worthy mortal may claim the Heart. The fighters gather in the Sump, the low country where every
river in the world ends (black water, reed forests and stilt-towns sinking into living mud), and
at the top of every tower waits a **Fragment of the Ebony Heart, wearing the shape of the
Slopbat**: the final boss is still the Slopbat, and what it is now is the Fragment.

**Canon, as Markus has fixed it (29 October 2026), and what is not.** Fixed: the Ebony Heart, its
five hundred years, the Outer Gods' tournament for one worthy mortal, the Fragment in the
Slopbat's shape; Ashka is **Pure Good**; the Grandmaster is **Neutral**. Not decided, so stated
nowhere in the game: who the protagonist is (Ashka or Wren), every other fighter's alignment,
and Wren's and the Grandmaster's reasons for fighting (their bios and endings say only what they
are). The six original fighters' bios and endings are the original game's text and still speak of
asking the Slopbat for something; they read with the Fragment wearing its shape, and whether they
stand under the new canon is Markus's call. Names: **Wren** and **Grandmaster**, no surnames.

The game takes these stakes seriously. The Slopbat's name is the joke, along with a few chosen
moments (the finishers' name, the Reflection). There is no fourth-wall commentary and no
AI-themed material.

## Tone and art direction: bog-gothic arcade

- **Silhouette first.** Every fighter is recognisable as a black shape: a lean kickboxer, a
  giant, a hooded assassin trailing a chain, a tall veiled mourner, a long-coated constable in a
  peaked cap, a round-helmeted diving suit, a braided fighter in a short belted jacket, a bald old
  master in a long robe, and the winged boss.
- **Palette.** Stages in deep teal-black, moss and wet stone, lit by lantern amber and furnace
  orange; supernatural power in sickly slop-green (the Sump), ember orange (Ashka), pale
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
| **Gorm Tallow** (he), the Renderer, 2.12 m | Huge, bald, pale ruddy skin, heavy jaw; bare barrel chest under a thick rubberised leather apron, rolled canvas trousers, heavy boots, iron vambraces | Sumo and catch wrestling. Grappler: slow, armoured, devastating up close | Vat Press (command grab), Tallow Hide (armour), Rendering Charge (armoured shoulder), Ground Slap (low shockwave), Tallow Slaps (a stepping thrust string that shoves), Rising Belly (armoured anti-air), Belly Slide (under high projectiles), Belly-to-Belly (a breakable second command grab, thrown behind him) | Rendered Down: lifts and slams, then stamps them flat |
| **Nix Hollow** (he), Hand of the Drowned, 1.78 m | Slim; charcoal and black wrapped garments, deep hood, a cloth mask over nose and mouth, slop-green sash, glowing green eyes; a kusarigama (sickle and chain) | Ninjutsu. Mix-up and teleports | Sickle Throw (returns), Sinkstep (sink through the floor and rise behind), Undertow (chain trips the legs), Drowned Drop (air dive) | Undertow: drags them down into a slop pool |
| **Mirela Sorrow** (she), the Keening Widow, 1.82 m | Tall, slender, pale; very long black hair; an ivory mourning gown with a long skirt open at the front over ivory leggings, a black lace veil over hair and shoulders, black gloves | Baguazhang palms and a voice that unmakes. Zoner | Lament (slow, large wave), Keen (scream that dazes and passes through projectiles), Grave Float (hover), Veil Step (backward fade) | The Last Note: turns them to salt that crumbles |
| **Bram Ketch** (he), Captain of the Lantern Watch, 1.86 m | Broad, weathered, short grey-brown hair and mutton-chop beard; a long navy watch coat with brass buttons and coat tails, a peaked watch cap, leather boots; a hardwood baton and a lantern at his belt | Bare-knuckle boxing and the baton. Rushdown | Signal Flare (high arc / low roll), Baton Trip, Bull Rush, Stand Down (counter) | Lights Out: a baton swing that sends them over the horizon |
| **Hollis Fathom** (he), the Diving Bell, 1.95 m | An olive canvas diving suit, a round brass helmet with three portholes and a faint green glow inside, a brass breastplate, lead-soled boots, rubber gloves, a harpoon gun | Deep-sea pressure. Heavy zoner and trapper | Harpoon (pulls them in), Anchor Drop (overhead from above), Bubble (slow trap), Vent (steam burst reversal) | Crush Depth: reels them in and the helmet swallows them |
| **Wren** (she), the All-Rounder, 1.68 m | Young, lean and strong; light skin, dark-blonde hair in tight braids; a short, heavy-weave charcoal jacket with cross-collar lapels and forearm sleeves, belted with a faded red belt, over a dark rash-guard top; ankle-length grappling trousers with reinforced knees, laced wrestling shoes, open-finger gloves; a little teal glass at the belt | Mixed martial arts (kickboxing, taekwondo kicks, boxing, judo, an armlock) and marsh-light. All-rounder | Turning Light (spinning back kick), Flying Cross (lunging cross), Lifting Light (spinning crescent anti-air), Bar the Door (catches a high or mid blow into an armlock), Flicker Step (a short blink, forward or back, through projectiles), Wisp Feint (a lit feint that cancels into anything); judo throws | Led Astray: the marsh-lights lead them off into the fog |
| **Grandmaster** (he), Master of Qhi; no other name, 1.70 m | Elderly, East Asian, thin and wiry; a shaved head, heavy brow, high cheekbones over hollow cheeks, deep-set eyes, a grey-white goatee; a long ink-blue cross-collar robe over a bone-grey inner robe, pale-gold cloud embroidery at hem, cuffs and collar, a broad black sash with a jade pendant, black cloth shoes | Qhi and a lifetime of martial arts: every fighter's shape (and the Fragment's) from his form library, changed even inside a combo, and three instant kills; traditional boxing when the Qhi is spent. Secret, for experts | Still Palm (Qhi bolt), Sinking Palm (Qhi along the floor), Rising Palm (anti-air), Drift (floats over and behind), Weave, Yield (parry and returning palm), Ward (breaks projectiles), Step Aside (an evade), Borrowed Face (becomes the opponent, stronger), the form library (any fighter's shape, the Fragment's), Two Fingers into One Strike, Find the Crack and Heart Stop (instant kills) | Still Water: two fingers to the brow, and they still to standing water and fall |

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
| Ashka | All-rounder; tall square Muay Thai stance (thumbs at the brows, a slow rock between the feet), walk 2.5/2.05, run 5.4 | 6-frame elbow and knee starters, a fireball, a 5-frame anti-air knee | No armour, average range; the knee and rush are punishable on block | Ember Shot, Wick Knee, Elbow Rush, Rising Knee | Block the rush low-high, punish Rising Knee's 24 recovery frames |
| Gorm | Grappler; deep wide sumo squat with a straight back and sliding feet, slow walk 1.9/1.6, heavy jump | Command grab (Vat Press), Tallow Hide armour, armoured charge, overhead hammer | Slowest walk and run, almost no zoning, big recoveries | Vat Press, Tallow Hide, Rendering Charge, Ground Slap | Stay out of grab range, zone him, jump the slap, punish the 32-frame grab whiff |
| Nix | Mix-up; slim oblique ichimonji stance (sunk rear leg, upright spine), fastest walk 2.7/2.3, run 5.8 | Overheads and lows from the same chain, a returning sickle, a teleport behind | Low damage per hit; Sinkstep (34 frames) and Undertow are slow and reactable | Sickle Throw, Sinkstep, Undertow, Drowned Drop | Block on reaction to the teleport, crouch the trip, anti-air the dive |
| Mirela | Zoner; tall palm stance on bent knees, the torso twisted to the opponent, a circling walk, walk 2.4/2.0, 1040 health | Slow large wave, a scream that passes through projectiles and dazes, hover, a backward fade | Slow startups (13-14 frames), weak up close | Lament, Keen, Grave Float, Veil Step | Close the distance under the wave, punish Keen's recovery, catch the hover landing |
| Bram | Rushdown; upright Prize Ring guard (lead fist out, rear forearm across the body), walk 2.6/2.0, run 5.6 | Fast hooks, a long-reaching Bull Rush (9 active frames), high and low flares, a 3-frame counter stance | Flares are slow (13), the counter loses to throws and lows | Signal Flare high/low, Baton Trip, Bull Rush, Stand Down | Throw the counter stance, block the rush and punish, low-block the trip |
| Hollis | Heavy zoner and trapper; planted suit (wide flat feet, the helmet turning only with the chest), walk 2.0/1.7, 1080 health | The pull (Harpoon) from range, an overhead from above, a slow trapping bubble, a steam reversal | Slowest normals (9-10 frames), slow projectiles, poor mobility | Harpoon, Anchor Drop, Bubble, Vent | Jump the harpoon, dash in behind the bubble, bait Vent and punish its 30 recovery frames |
| Wren | All-rounder; bladed MMA guard (hands high, lead hand out), a light bounce, walk 2.6/2.15, run 5.6 | Reliable fundamentals at every range: a 1-2 into a head kick, a lunging cross, a crescent anti-air, judo throws that leave her close (6-frame recovery), a parry for highs and mids that becomes an armlock, a blink through projectiles, a feint that cancels into anything | The shortest reach, no projectile, average damage; Turning Light (-12) and Flying Cross (-9) are punishable on block; Bar the Door loses to lows, throws and projectiles; Flicker Step has no strike invulnerability | Turning Light, Flying Cross, Lifting Light, Bar the Door, Flicker Step, Wisp Feint | Low-block her strings' kicks, punish the spinning kick and the cross on block, throw or sweep the parry, hit her out of the flicker |
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
  guard and retreat when spent; and zoning with his palms from 1.6 m, his strength. Development
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

## The Ebony Heart Fragment, in the Slopbat's shape (final boss), 2.65 m

A Fragment of the Ebony Heart wearing the Slopbat's shape, named "Ebony Heart Fragment" on its bar,
"The Slopbat" beneath. The shape: a gaunt, towering sovereign of tar: glossy black-green skin traced with glowing green veins, long arms
and claws, a bat's ears and a crown of horn, glowing green eyes, a tattered membrane cloak, and
great bat wings folded at its back. It cannot be thrown and has much more health (1400, 1650 or 1900 by tower), but every
pattern can be learned: claw swipes; Tar Spit (high, duck it); Wing Gust (pushes, no damage);
the Dive (it flies off the top of the screen, its shadow follows you for a second, then it comes
down where the shadow is; move away, and punish while it is stuck in the mud); and below half
health the **Storm Shroud** (inspired by Vessimund's lightning burst in Slopperfall): it gathers
green lightning for most of a second, then lightning strikes all around it. Get out of range, or
hit it while it gathers to break the charge. Its defeat collapses the shape into slop and the
waters recede; then the champion's ending plays. The Grandmaster can wear its shape (below); the
Fragment itself cannot be held (One Strike's catch only parries it), and Find the Crack can kill
it outright, though it claws or gusts at the long startup when it sees it coming.

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
