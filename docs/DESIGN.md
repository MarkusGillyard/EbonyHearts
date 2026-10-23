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
  peaked cap, a round-helmeted diving suit, and the winged boss.
- **Palette.** Stages in deep teal-black, moss and wet stone, lit by lantern amber and furnace
  orange; supernatural power in sickly slop-green (the Sump), ember orange (Ashka), pale
  dirge-blue (Mirela), brass (Hollis), violet (the stranger). Each fighter has one dominant
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

## Roster

| Fighter | Look | Style and role | Specials (planned) | Sloptality |
|---|---|---|---|---|
| **Ashka Rook** (she), the Last Lamplighter, 1.74 m | Lean, athletic, brown skin, black ponytail, amber eyes; dark leather sleeveless vest, rust-red silk fight shorts with gold trim and waistband, bone-white hand and shin wraps, gold arm band, a small brass lamp charm | Muay Thai and lamp-fire. All-rounder | Ember Shot (fireball), Wick Knee (flying knee), Elbow Rush, Rising Knee (anti-air) | Last Light: burns the opponent to ash |
| **Gorm Tallow** (he), the Renderer, 2.12 m | Huge, bald, pale ruddy skin, heavy jaw; bare barrel chest under a thick rubberised leather apron, rolled canvas trousers, heavy boots, iron vambraces | Sumo and catch wrestling. Grappler: slow, armoured, devastating up close | Vat Press (command grab), Tallow Hide (armour), Rendering Charge (armoured shoulder), Ground Slap (low shockwave) | Rendered Down: lifts and slams, then stamps them flat |
| **Nix Hollow** (he), Hand of the Drowned, 1.78 m | Slim; charcoal and black wrapped garments, deep hood, a cloth mask over nose and mouth, slop-green sash, glowing green eyes; a kusarigama (sickle and chain) | Ninjutsu. Mix-up and teleports | Sickle Throw (returns), Sinkstep (sink through the floor and rise behind), Undertow (chain trips the legs), Drowned Drop (air dive) | Undertow: drags them down into a slop pool |
| **Mirela Sorrow** (she), the Keening Widow, 1.82 m | Tall, slender, pale; very long black hair; an ivory mourning gown with a long skirt open at the front over ivory leggings, a black lace veil over hair and shoulders, black gloves | Baguazhang palms and a voice that unmakes. Zoner | Lament (slow, large wave), Keen (scream that dazes and passes through projectiles), Grave Float (hover), Veil Step (backward fade) | The Last Note: turns them to salt that crumbles |
| **Bram Ketch** (he), Captain of the Lantern Watch, 1.86 m | Broad, weathered, short grey-brown hair and mutton-chop beard; a long navy watch coat with brass buttons and coat tails, a peaked watch cap, leather boots; a hardwood baton and a lantern at his belt | Bare-knuckle boxing and the baton. Rushdown | Signal Flare (high arc / low roll), Baton Trip, Bull Rush, Stand Down (counter) | Lights Out: a baton swing that sends them over the horizon |
| **Hollis Fathom** (he), the Diving Bell, 1.95 m | An olive canvas diving suit, a round brass helmet with three portholes and a faint green glow inside, a brass breastplate, lead-soled boots, rubber gloves, a harpoon gun | Deep-sea pressure. Heavy zoner and trapper | Harpoon (pulls them in), Anchor Drop (overhead from above), Bubble (slow trap), Vent (steam burst reversal) | Crush Depth: reels them in and the helmet swallows them |

Rivals (the second-to-last arcade fight): Ashka and Nix (the Drowned Hand put out Ninefathom's
beacon), Gorm and Bram (Bram once arrested him for a vat fire he did not start), Mirela and
Hollis (she sang at the funeral of the diver who never came back up).

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
the Roost as a versus stage (finish arcade once), the endings gallery, and the stranger.

## Audio

All sound is synthesised by `tools/assets/gen_audio.py`: layered impacts (a low thump, a
mid-body slap and a high crack), whooshes by weight, blocks, throws, landings, cloth and steps,
projectile casts, menu cues, gongs and drums for round calls, and a restrained music loop per
stage, for the menu and for the boss. There is no synthetic announcer voice; round calls are
text with stingers.
