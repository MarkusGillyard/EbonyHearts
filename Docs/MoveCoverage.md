# The Grandmaster's move coverage

Written by the automation test `Slop.Fighters.Grandmaster.EveryMoveIsCovered` from the loaded data (Source/MortalSlopbat/Combat/SlopCoverage.cpp). Every move of every fighter in Ebony Hearts, and how the Grandmaster reaches it: **own** (his own body), **spent** (his body with no Qhi), **form** (the shape from his form library, its input, then the move's own input), **carried** (a technique of a fighter no longer in the game, now his). Each fighter he can face can also be copied with Borrowed Face (D,B,F+Block), Qhi-enhanced. **excluded** rows are not counted: finishers (the fight's end lets a shape go, so a shape never performs its source's finisher) and moves the game starts itself.

**247 of 247 counted moves covered** (17 excluded).

| Fighter | Move | Name | Via | How |
|---|---|---|---|---|
| grandmaster | `borrowed_face` | Borrowed Face | own | D,B,F+BL |
| grandmaster | `crouch_hk` | Crouching High Kick | own | D+HK |
| grandmaster | `crouch_lk` | Crouching Low Kick | own | D+LK |
| grandmaster | `crouch_lp` | Crouching Punch | own | D+LP |
| grandmaster | `drift` | Drift | own | D,B+LK |
| grandmaster | `find_the_crack` | Find the Crack | own | B,F,B,F+HP |
| grandmaster | `gm_two_fingers` | Old Hands: two fingers | own |  |
| grandmaster | `heart_stop` | Heart Stop | own | F,D,F+HP |
| grandmaster | `hk` | High Kick | own | HK |
| grandmaster | `hp` | Palm | own | HP |
| grandmaster | `hp2` | High Punch (second) | own |  |
| grandmaster | `jump_kick` | Jump Kick | own | Air HK |
| grandmaster | `jump_kick_n` | Straight Jump Kick | own | Air HK |
| grandmaster | `jump_punch` | Jump Punch | own | Air HP |
| grandmaster | `lk` | Low Kick | own | LK |
| grandmaster | `lp` | Low Punch | own | LP |
| grandmaster | `lp2` | Low Punch (second) | own |  |
| grandmaster | `one_strike` | One Strike | own |  |
| grandmaster | `own_face` | Own Face | own | D,B,F+BL |
| grandmaster | `qhi_recentre` | Recentre | excluded | started by the game, not an input |
| grandmaster | `qhi_ward` | Ward | own | D,D+BL |
| grandmaster | `returning_palm` | Yield: returning palm | own |  |
| grandmaster | `rising_palm` | Rising Palm | own | D,U+HP |
| grandmaster | `roundhouse` | Roundhouse | own | B+HK |
| grandmaster | `shape_ashka` | Shape: Ashka Rook | own | RN+LP |
| grandmaster | `shape_bram` | Shape: Bram Ketch | own | RN+HP |
| grandmaster | `shape_gorm` | Shape: Gorm Tallow | own | RN+HK |
| grandmaster | `shape_hollis` | Shape: Hollis Fathom | own | D+RN+LK |
| grandmaster | `shape_mirela` | Shape: Mirela Sorrow | own | D+RN+HP |
| grandmaster | `shape_nix` | Shape: Nix Hollow | own | D+RN+LP |
| grandmaster | `shape_slopbat` | Shape: the Ebony Heart Fragment | own | D+RN+HK |
| grandmaster | `shape_template` | Shape | excluded | started by the game, not an input |
| grandmaster | `shape_wren` | Shape: Wren | own | RN+LK |
| grandmaster | `sinking_palm` | Sinking Palm | own | D,F+LP |
| grandmaster | `stage_vat` | Into the Vat | own | his finisher |
| grandmaster | `step_aside` | Step Aside | own | B,B+LK |
| grandmaster | `still_palm` | Still Palm | own | D,F+HP |
| grandmaster | `still_water` | Still Water | own | his finisher |
| grandmaster | `sweep` | Sweep | own | B+LK |
| grandmaster | `throw_b` | Back Throw | own | B+LP |
| grandmaster | `throw_f` | Throw | own | F+LP |
| grandmaster | `two_fingers` | Two Fingers | own | F,B+HP |
| grandmaster | `uppercut` | Uppercut | own | D+HP |
| grandmaster | `weave` | Weave | own | B,D+LP |
| grandmaster | `yield` | Yield | own | B,D+HP |
| grandmaster_spent | `box_body_hook` | Left Hook to the Body | spent | LK |
| grandmaster_spent | `box_counter` | Parry: counter cross | spent |  |
| grandmaster_spent | `box_hook` | Right Hook | spent | HK |
| grandmaster_spent | `box_parry` | Parry | spent | B,D+HP |
| grandmaster_spent | `box_slip` | Slip | spent | B,D+LP |
| grandmaster_spent | `crouch_lp` | Crouching Punch | spent | D+LP |
| grandmaster_spent | `hp` | Jab | spent | HP |
| grandmaster_spent | `hp2` | Cross | spent |  |
| grandmaster_spent | `jump_punch` | Jump Punch | spent | Air HP |
| grandmaster_spent | `lp` | Body Jab | spent | LP |
| grandmaster_spent | `lp2` | Body Cross | spent |  |
| grandmaster_spent | `qhi_spent` | Spent | excluded | started by the game, not an input |
| grandmaster_spent | `stage_vat` | Into the Vat | own | his finisher |
| grandmaster_spent | `throw_b` | Back Throw | spent | B+LP |
| grandmaster_spent | `throw_f` | Throw | spent | F+LP |
| grandmaster_spent | `uppercut` | Uppercut | spent | D+HP |
| ashka | `ashka_elbow_chain` | Lantern Lash: elbow | form | RN+LP, then  |
| ashka | `ashka_head_chain` | Temple Bell: head kick | form | RN+LP, then  |
| ashka | `ashka_knee_chain` | Temple Bell: knee | form | RN+LP, then  |
| ashka | `ashka_spin_chain` | Lantern Lash: spinning heel | form | RN+LP, then  |
| ashka | `ashka_teep` | Teep | form | RN+LP, then F+LK |
| ashka | `crouch_hk` | Crouching High Kick | form | RN+LP, then D+HK |
| ashka | `crouch_lk` | Crouching Low Kick | form | RN+LP, then D+LK |
| ashka | `crouch_lp` | Crouching Punch | form | RN+LP, then D+LP |
| ashka | `elbow_rush` | Elbow Rush | form | RN+LP, then F,F+LP |
| ashka | `ember_shot` | Ember Shot | form | RN+LP, then D,F+HP |
| ashka | `hk` | High Kick | form | RN+LP, then HK |
| ashka | `hp` | High Punch | form | RN+LP, then HP |
| ashka | `hp2` | High Punch (second) | form | RN+LP, then  |
| ashka | `jump_kick` | Jump Kick | form | RN+LP, then Air HK |
| ashka | `jump_kick_n` | Straight Jump Kick | form | RN+LP, then Air HK |
| ashka | `jump_punch` | Jump Punch | form | RN+LP, then Air HP |
| ashka | `last_light` | Last Light | excluded | a finisher: the fight's end lets the shape go |
| ashka | `lk` | Low Kick | form | RN+LP, then LK |
| ashka | `lp` | Low Punch | form | RN+LP, then LP |
| ashka | `lp2` | Low Punch (second) | form | RN+LP, then  |
| ashka | `rising_knee` | Rising Knee | form | RN+LP, then D,U+HK |
| ashka | `roundhouse` | Roundhouse | form | RN+LP, then B+HK |
| ashka | `shin_check` | Shin Check | form | RN+LP, then B,D+LK |
| ashka | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| ashka | `sweep` | Sweep | form | RN+LP, then B+LK |
| ashka | `throw_b` | Back Throw | form | RN+LP, then B+LP |
| ashka | `throw_f` | Throw | form | RN+LP, then F+LP |
| ashka | `uppercut` | Uppercut | form | RN+LP, then D+HP |
| ashka | `wick_knee` | Wick Knee | form | RN+LP, then B,F+HK |
| gorm | `crouch_hk` | Crouching High Kick | form | RN+HK, then D+HK |
| gorm | `crouch_lk` | Crouching Low Kick | form | RN+HK, then D+LK |
| gorm | `crouch_lp` | Crouching Punch | form | RN+HK, then D+LP |
| gorm | `gorm_belly` | Vat Stir: belly | form | RN+HK, then  |
| gorm | `gorm_hammer` | Tallow Mill: hammer | form | RN+HK, then  |
| gorm | `ground_slap` | Ground Slap | form | RN+HK, then D,D+HP |
| gorm | `hk` | High Kick | form | RN+HK, then HK |
| gorm | `hp` | High Punch | form | RN+HK, then HP |
| gorm | `hp2` | High Punch (second) | form | RN+HK, then  |
| gorm | `jump_kick` | Jump Kick | form | RN+HK, then Air HK |
| gorm | `jump_kick_n` | Straight Jump Kick | form | RN+HK, then Air HK |
| gorm | `jump_punch` | Jump Punch | form | RN+HK, then Air HP |
| gorm | `lk` | Low Kick | form | RN+HK, then LK |
| gorm | `lp` | Low Punch | form | RN+HK, then LP |
| gorm | `lp2` | Low Punch (second) | form | RN+HK, then  |
| gorm | `rendered_down` | Rendered Down | excluded | a finisher: the fight's end lets the shape go |
| gorm | `rendering_charge` | Rendering Charge | form | RN+HK, then B,F+HK |
| gorm | `roundhouse` | Roundhouse | form | RN+HK, then B+HK |
| gorm | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| gorm | `sweep` | Sweep | form | RN+HK, then B+LK |
| gorm | `tallow_hide` | Tallow Hide | form | RN+HK, then D,D+BL |
| gorm | `throw_b` | Back Throw | form | RN+HK, then B+LP |
| gorm | `throw_f` | Throw | form | RN+HK, then F+LP |
| gorm | `uppercut` | Uppercut | form | RN+HK, then D+HP |
| gorm | `vat_press` | Vat Press | form | RN+HK, then B,F+LP |
| nix | `crouch_hk` | Crouching High Kick | form | D+RN+LP, then D+HK |
| nix | `crouch_lk` | Crouching Low Kick | form | D+RN+LP, then D+LK |
| nix | `crouch_lp` | Crouching Punch | form | D+RN+LP, then D+LP |
| nix | `drowned_drop` | Drowned Drop | form | D+RN+LP, then Air D+HK |
| nix | `hk` | High Kick | form | D+RN+LP, then HK |
| nix | `hp` | High Punch | form | D+RN+LP, then HP |
| nix | `hp2` | High Punch (second) | form | D+RN+LP, then  |
| nix | `jump_kick` | Jump Kick | form | D+RN+LP, then Air HK |
| nix | `jump_kick_n` | Straight Jump Kick | form | D+RN+LP, then Air HK |
| nix | `jump_punch` | Jump Punch | form | D+RN+LP, then Air HP |
| nix | `lk` | Low Kick | form | D+RN+LP, then LK |
| nix | `lp` | Low Punch | form | D+RN+LP, then LP |
| nix | `lp2` | Low Punch (second) | form | D+RN+LP, then  |
| nix | `nix_axe` | Black Water: axe kick | form | D+RN+LP, then  |
| nix | `nix_low` | Drowned Hand: hook | form | D+RN+LP, then  |
| nix | `nix_slash` | Drowned Hand: slash | form | D+RN+LP, then  |
| nix | `nix_spin` | Black Water: spin kick | form | D+RN+LP, then  |
| nix | `roundhouse` | Roundhouse | form | D+RN+LP, then B+HK |
| nix | `sickle_throw` | Sickle Throw | form | D+RN+LP, then B,F+LP |
| nix | `sinkstep` | Sinkstep | form | D+RN+LP, then D,B+LK |
| nix | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| nix | `sweep` | Sweep | form | D+RN+LP, then B+LK |
| nix | `throw_b` | Back Throw | form | D+RN+LP, then B+LP |
| nix | `throw_f` | Throw | form | D+RN+LP, then F+LP |
| nix | `undertow` | Undertow | form | D+RN+LP, then B,B+HP |
| nix | `undertow_pool` | Undertow | excluded | a finisher: the fight's end lets the shape go |
| nix | `uppercut` | Uppercut | form | D+RN+LP, then D+HP |
| mirela | `crouch_hk` | Crouching High Kick | form | D+RN+HP, then D+HK |
| mirela | `crouch_lk` | Crouching Low Kick | form | D+RN+HP, then D+LK |
| mirela | `crouch_lp` | Crouching Punch | form | D+RN+HP, then D+LP |
| mirela | `grave_float` | Grave Float | form | D+RN+HP, then D,U+LP |
| mirela | `hk` | High Kick | form | D+RN+HP, then HK |
| mirela | `hp` | High Punch | form | D+RN+HP, then HP |
| mirela | `hp2` | High Punch (second) | form | D+RN+HP, then  |
| mirela | `jump_kick` | Jump Kick | form | D+RN+HP, then Air HK |
| mirela | `jump_kick_n` | Straight Jump Kick | form | D+RN+HP, then Air HK |
| mirela | `jump_punch` | Jump Punch | form | D+RN+HP, then Air HP |
| mirela | `keen` | Keen | form | D+RN+HP, then B,F+HP |
| mirela | `lament` | Lament | form | D+RN+HP, then D,F+HP |
| mirela | `last_note` | The Last Note | excluded | a finisher: the fight's end lets the shape go |
| mirela | `lk` | Low Kick | form | D+RN+HP, then LK |
| mirela | `lp` | Low Punch | form | D+RN+HP, then LP |
| mirela | `lp2` | Low Punch (second) | form | D+RN+HP, then  |
| mirela | `mirela_circle` | Mourning Palms: circling palm | form | D+RN+HP, then  |
| mirela | `mirela_crescent` | Veil Kick: crescent | form | D+RN+HP, then  |
| mirela | `mirela_double` | Mourning Palms: double palm | form | D+RN+HP, then  |
| mirela | `roundhouse` | Roundhouse | form | D+RN+HP, then B+HK |
| mirela | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| mirela | `sweep` | Sweep | form | D+RN+HP, then B+LK |
| mirela | `throw_b` | Back Throw | form | D+RN+HP, then B+LP |
| mirela | `throw_f` | Throw | form | D+RN+HP, then F+LP |
| mirela | `uppercut` | Uppercut | form | D+RN+HP, then D+HP |
| mirela | `veil_step` | Veil Step | form | D+RN+HP, then B,B+LK |
| bram | `baton_trip` | Baton Trip | form | RN+HP, then F,B+LP |
| bram | `bram_baton` | Night Stick: rising baton | form | RN+HP, then  |
| bram | `bram_body` | Watchman's Four: body blow | form | RN+HP, then  |
| bram | `bram_drive` | Prize Ring Drive | form | RN+HP, then F,F+HP |
| bram | `bram_hook` | Watchman's Four: hook | form | RN+HP, then  |
| bram | `bull_rush` | Bull Rush | form | RN+HP, then F,F+HK |
| bram | `crouch_hk` | Crouching High Kick | form | RN+HP, then D+HK |
| bram | `crouch_lk` | Crouching Low Kick | form | RN+HP, then D+LK |
| bram | `crouch_lp` | Crouching Punch | form | RN+HP, then D+LP |
| bram | `flare_high` | Signal Flare (high) | form | RN+HP, then D,B+HP |
| bram | `flare_low` | Signal Flare (low) | form | RN+HP, then D,B+LP |
| bram | `hk` | High Kick | form | RN+HP, then HK |
| bram | `hp` | High Punch | form | RN+HP, then HP |
| bram | `hp2` | High Punch (second) | form | RN+HP, then  |
| bram | `jump_kick` | Jump Kick | form | RN+HP, then Air HK |
| bram | `jump_kick_n` | Straight Jump Kick | form | RN+HP, then Air HK |
| bram | `jump_punch` | Jump Punch | form | RN+HP, then Air HP |
| bram | `lights_out` | Lights Out | excluded | a finisher: the fight's end lets the shape go |
| bram | `lk` | Low Kick | form | RN+HP, then LK |
| bram | `lp` | Low Punch | form | RN+HP, then LP |
| bram | `lp2` | Low Punch (second) | form | RN+HP, then  |
| bram | `roundhouse` | Roundhouse | form | RN+HP, then B+HK |
| bram | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| bram | `stand_down` | Stand Down | form | RN+HP, then B,D+HP |
| bram | `stand_down_strike` | Stand Down: baton | form | RN+HP, then  |
| bram | `sweep` | Sweep | form | RN+HP, then B+LK |
| bram | `throw_b` | Cross-buttock | form | RN+HP, then B+LP |
| bram | `throw_f` | Chancery | form | RN+HP, then F+LP |
| bram | `uppercut` | Uppercut | form | RN+HP, then D+HP |
| hollis | `anchor_drop` | Anchor Drop | form | D+RN+LK, then D,D+HK |
| hollis | `bubble` | Bubble | form | D+RN+LK, then D,F+LP |
| hollis | `crouch_hk` | Crouching High Kick | form | D+RN+LK, then D+HK |
| hollis | `crouch_lk` | Crouching Low Kick | form | D+RN+LK, then D+LK |
| hollis | `crouch_lp` | Crouching Punch | form | D+RN+LK, then D+LP |
| hollis | `crush_depth` | Crush Depth | excluded | a finisher: the fight's end lets the shape go |
| hollis | `harpoon` | Harpoon | form | D+RN+LK, then B,F+LP |
| hollis | `hk` | High Kick | form | D+RN+LK, then HK |
| hollis | `hollis_boot` | Lead Boot | form | D+RN+LK, then  |
| hollis | `hollis_butt` | Pressure Hull: helmet butt | form | D+RN+LK, then  |
| hollis | `hp` | High Punch | form | D+RN+LK, then HP |
| hollis | `hp2` | High Punch (second) | form | D+RN+LK, then  |
| hollis | `jump_kick` | Jump Kick | form | D+RN+LK, then Air HK |
| hollis | `jump_kick_n` | Straight Jump Kick | form | D+RN+LK, then Air HK |
| hollis | `jump_punch` | Jump Punch | form | D+RN+LK, then Air HP |
| hollis | `lk` | Low Kick | form | D+RN+LK, then LK |
| hollis | `lp` | Low Punch | form | D+RN+LK, then LP |
| hollis | `lp2` | Low Punch (second) | form | D+RN+LK, then  |
| hollis | `roundhouse` | Roundhouse | form | D+RN+LK, then B+HK |
| hollis | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| hollis | `sweep` | Sweep | form | D+RN+LK, then B+LK |
| hollis | `throw_b` | Back Throw | form | D+RN+LK, then B+LP |
| hollis | `throw_f` | Throw | form | D+RN+LK, then F+LP |
| hollis | `uppercut` | Uppercut | form | D+RN+LK, then D+HP |
| hollis | `vent` | Vent | form | D+RN+LK, then D,B+HP |
| wren | `bar_the_door` | Bar the Door | form | RN+LK, then B,D+HP |
| wren | `crouch_hk` | Crouching High Kick | form | RN+LK, then D+HK |
| wren | `crouch_lk` | Crouching Low Kick | form | RN+LK, then D+LK |
| wren | `crouch_lp` | Crouching Punch | form | RN+LK, then D+LP |
| wren | `flicker_back` | Flicker Step (back) | form | RN+LK, then B,B+LK |
| wren | `flicker_step` | Flicker Step | form | RN+LK, then F,F+LK |
| wren | `flying_cross` | Flying Cross | form | RN+LK, then B,F+HP |
| wren | `hk` | High Kick | form | RN+LK, then HK |
| wren | `hp` | High Punch | form | RN+LK, then HP |
| wren | `hp2` | High Punch (second) | form | RN+LK, then  |
| wren | `jump_kick` | Jump Kick | form | RN+LK, then Air HK |
| wren | `jump_kick_n` | Straight Jump Kick | form | RN+LK, then Air HK |
| wren | `jump_punch` | Jump Punch | form | RN+LK, then Air HP |
| wren | `led_astray` | Led Astray | excluded | a finisher: the fight's end lets the shape go |
| wren | `lifting_light` | Lifting Light | form | RN+LK, then D,U+LK |
| wren | `lk` | Thigh Kick | form | RN+LK, then LK |
| wren | `lp` | Low Punch | form | RN+LK, then LP |
| wren | `lp2` | Low Punch (second) | form | RN+LK, then  |
| wren | `roundhouse` | Roundhouse | form | RN+LK, then B+HK |
| wren | `stage_vat` | Into the Vat | excluded | a finisher: the fight's end lets the shape go |
| wren | `sweep` | Sweep | form | RN+LK, then B+LK |
| wren | `throw_b` | Hip Throw | form | RN+LK, then B+LP |
| wren | `throw_f` | Reaping Throw | form | RN+LK, then F+LP |
| wren | `turning_light` | Turning Light | form | RN+LK, then B,F+HK |
| wren | `uppercut` | Uppercut | form | RN+LK, then D+HP |
| wren | `wisp_feint` | Wisp Feint | form | RN+LK, then D,B+LK |
| wren | `wren_armlock` | Bar the Door: armlock | form | RN+LK, then  |
| wren | `wren_backfist` | One-Two: backfist | form | RN+LK, then  |
| wren | `wren_cross` | One-Two: cross | form | RN+LK, then  |
| wren | `wren_head_kick` | Head Kick | form | RN+LK, then  |
| slopbat | `bite` | Bite | form | D+RN+HK, then F,F+LP |
| slopbat | `claw` | Claw | form | D+RN+HK, then HP |
| slopbat | `claw2` | Raking Claw | form | D+RN+HK, then  |
| slopbat | `dive` | The Dive | form | D+RN+HK, then D,U+HK |
| slopbat | `low_claw` | Low Rake | form | D+RN+HK, then D+LK |
| slopbat | `rising_claw` | Rising Talon | form | D+RN+HK, then D+HP |
| slopbat | `storm_shroud` | Storm Shroud | form | D+RN+HK, then B,B+HP |
| slopbat | `tar_spit` | Tar Spit | form | D+RN+HK, then D,F+HP |
| slopbat | `wing_gust` | Wing Gust | form | D+RN+HK, then B,F+HK |
| daxion | `dax_evade` | Step Aside | carried | step_aside (B,B+LK) |
| daxion | `dax_palm` | One Strike | carried | one_strike () |
| daxion | `dax_crack` | Find the Crack | carried | find_the_crack (B,F,B,F+HP) |
