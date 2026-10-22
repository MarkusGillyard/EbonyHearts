#!/usr/bin/env python3
"""Plays Mortal Slopbat as player 1 through real X11 keyboard input, deciding from the game's live
trace (run the game with -- --trace=<file> --trace-live). Between fights, when the trace goes
quiet, it presses Enter, which walks through results, continues, face-offs and endings.

It is a playtesting aid with simple tactics (approach, block what it sees coming, combo up close,
anti-air jumpers, avoid the Slopbat's dive and storm, finish dizzy opponents); it uses the
default left-keyboard controls (WASD, U I O / J K L).

Usage: autoplayer.py --trace /tmp/t.jsonl --seconds 600 [--finisher "F,F,D+HP" --range sweep]
       [--shots DIR --every 20]   (screenshots of the game window every N seconds)
"""
import argparse
import json
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from xdrive import Driver  # noqa: E402

HP, BL, HK, LP, RN, LK = "u", "i", "o", "j", "k", "l"
UP, DOWN, LEFT, RIGHT = "w", "s", "a", "d"
RANGES = {"close": (0.0, 1.1), "sweep": (1.2, 2.2), "far": (2.7, 4.5)}


class Player:
    def __init__(self, drv, finisher, frange):
        self.d = drv
        self.held = set()
        self.finisher = finisher
        self.frange = RANGES.get(frange, RANGES["close"])
        self.busy_until = 0.0
        self.rng = random.Random(5)

    def hold(self, keys):
        keys = set(keys)
        for k in list(self.held - keys):
            self.d.key(k, False)
        for k in keys - self.held:
            self.d.key(k, True)
        self.held = keys

    def release_all(self):
        self.hold([])

    def tap(self, key, ms=45):
        self.d.key(key, True)
        time.sleep(ms / 1000.0)
        self.d.key(key, False)

    def seq(self, steps):
        """Steps: (keys_to_hold_list, button_or_None, ms)."""
        for keys, btn, ms in steps:
            self.hold(keys)
            if btn:
                self.tap(btn, 40)
            time.sleep(ms / 1000.0)

    def motion(self, notation, fwd, back):
        """Enters arcade notation like F,F,D+HP using the current forward and back keys."""
        dirs, _, btn = notation.partition("+")
        keymap = {"F": [fwd], "B": [back], "D": [DOWN], "U": [UP], "DF": [DOWN, fwd], "DB": [DOWN, back]}
        btnmap = {"HP": HP, "LP": LP, "HK": HK, "LK": LK, "BL": BL, "RN": RN}
        steps = []
        for dd in dirs.split(","):
            steps.append((keymap[dd], None, 35))
            steps.append(([], None, 25))
        steps[-1] = (keymap[dirs.split(",")[-1]], btnmap[btn], 60)
        self.seq(steps)
        self.release_all()

    def act(self, s):
        me, op = s["f"][0], s["f"][1]
        facing = s.get("facing", [1, -1])[0]
        fwd, back = (RIGHT, LEFT) if facing > 0 else (LEFT, RIGHT)
        dx = op[4] - me[4]
        dist = abs(dx)
        phase = s.get("phase", 2)
        my_state, op_state, op_move, op_mf = me[0], op[0], op[1], op[2]
        op_air = op[5] > 0.3
        if phase == 3:  # FINISH
            if op_state != "DIZZY":
                self.release_all()
                return
            lo, hi = self.frange
            if dist > hi - 0.1:
                self.hold([fwd])
            elif dist < lo + 0.1:
                self.hold([back])
            else:
                self.release_all()
                time.sleep(0.05)
                self.motion(self.finisher, fwd, back)
                time.sleep(0.6)
            return
        if phase not in (2,) or my_state in ("DOWN", "KO", "WAKEUP", "THROWN", "HITSTUN", "JUGGLE", "FALL"):
            self.release_all()
            return
        if op[5] > 2.5:  # the Slopbat is up for its dive: get away from under it
            self.hold([back, RN] if self.rng.random() < 0.5 else [back])
            return
        if op_move == "storm_shroud" and dist < 3.2:
            self.hold([back])
            return
        if op_state == "MOVE" and op_mf < 10 and dist < 2.2 and op_move not in ("dive",):
            low = op_move in ("sweep", "crouch_lk", "low_claw", "nix_low", "baton_trip", "crouch_lp")
            self.hold([BL, DOWN] if low else [BL])
            return
        if op_air and dist < 1.8 and op[5] < 1.6:
            self.release_all()
            self.seq([([DOWN], HP, 60)])
            return
        if dist > 3.2:
            self.hold([fwd, RN] if self.rng.random() < 0.4 else [fwd])
            return
        if dist > 1.05:
            r = self.rng.random()
            if r < 0.08:
                self.release_all()
                self.seq([([UP, fwd], None, 260), ([], HK, 300)])
            elif r < 0.16:
                self.release_all()
                self.seq([([back], LK, 120)])
            else:
                self.hold([fwd])
            return
        r = self.rng.random()
        self.release_all()
        if r < 0.45:
            self.seq([([], HP, 90), ([], HP, 90), ([], LP, 90), ([back], HK, 260)])
        elif r < 0.65:
            self.seq([([DOWN], HP, 300)])
        elif r < 0.8:
            self.seq([([back], LK, 260)])
        elif r < 0.9:
            self.seq([([fwd], LP, 400)])
        else:
            self.seq([([DOWN], LK, 120), ([DOWN], LK, 120)])
        self.release_all()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace", required=True)
    ap.add_argument("--seconds", type=float, default=300)
    ap.add_argument("--finisher", default="F,F,D+HP")
    ap.add_argument("--range", default="sweep")
    ap.add_argument("--shots", default="")
    ap.add_argument("--every", type=float, default=20.0)
    ap.add_argument("--window", default="Mortal Slopbat")
    a = ap.parse_args()
    drv = Driver(a.window, a.shots or "/tmp")
    p = Player(drv, a.finisher, a.range)
    start = time.time()
    last_line = time.time()
    last_enter = 0.0
    last_shot = 0.0
    state = None
    pos = 0
    shots = 0
    while time.time() - start < a.seconds:
        if os.path.exists(a.trace):
            with open(a.trace) as f:
                f.seek(pos)
                data = f.read()
                pos = f.tell()
            lines = [ln for ln in data.splitlines() if ln.strip()]
            if lines:
                try:
                    state = json.loads(lines[-1])
                    last_line = time.time()
                except json.JSONDecodeError:
                    pass
        quiet = time.time() - last_line
        if quiet > 1.2:
            p.release_all()
            if time.time() - last_enter > 1.6:
                p.tap("Return", 60)
                last_enter = time.time()
        elif state:
            p.act(state)
        if a.shots and time.time() - last_shot > a.every:
            drv.shot("auto_%03d" % shots)
            shots += 1
            last_shot = time.time()
        time.sleep(0.016)
    p.release_all()


if __name__ == "__main__":
    main()
