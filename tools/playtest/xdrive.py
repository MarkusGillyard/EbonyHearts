#!/usr/bin/env python3
"""Drives the running game through real X11 keyboard events (XTest) and screenshots its window.

Used for hands-on playtesting on an X11 desktop. Commands come from a script file or stdin,
one per line:

    wait 0.5            sleep seconds
    press d             key down (held until released)
    release d           key up
    tap u [ms]          press and release, holding for ms (default 40)
    hold s 300          hold a key for ms
    chord u j [ms]      press several keys together
    shot name           save <outdir>/<name>.png of the game window
    step n              advance n frames in the game's frame-step mode (F6 n times)
    focus               re-focus the game window

Usage: xdrive.py --window "Mortal Slopbat" --out /tmp/shots script.txt
Requires python3-xlib and Pillow.
"""
import argparse
import os
import sys
import time

from PIL import ImageGrab
from Xlib import X, XK, display, protocol
from Xlib.ext import xtest

KEYS = {
    "esc": "Escape", "enter": "Return", "tab": "Tab", "space": "space",
    "left": "Left", "right": "Right", "up": "Up", "down": "Down",
    "kp1": "KP_1", "kp2": "KP_2", "kp4": "KP_4", "kp5": "KP_5", "kp6": "KP_6",
}
KEYS.update({"f%d" % i: "F%d" % i for i in range(1, 13)})


class Driver:
    def __init__(self, title, out):
        self.d = display.Display()
        self.root = self.d.screen().root
        self.out = out
        self.title = title
        self.win = self.find()
        if self.win is None:
            sys.exit("window not found: " + title)
        self.focus()

    def find(self):
        def walk(w):
            try:
                name = w.get_wm_name()
            except Exception:
                name = None
            if name and self.title in str(name):
                return w
            try:
                children = w.query_tree().children
            except Exception:
                return None
            for c in children:
                r = walk(c)
                if r is not None:
                    return r
            return None
        return walk(self.root)

    def focus(self):
        atom = self.d.intern_atom("_NET_ACTIVE_WINDOW")
        ev = protocol.event.ClientMessage(window=self.win, client_type=atom, data=(32, [1, X.CurrentTime, 0, 0, 0]))
        self.root.send_event(ev, event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
        self.win.set_input_focus(X.RevertToParent, X.CurrentTime)
        self.d.sync()
        time.sleep(0.15)

    def keycode(self, name):
        sym = XK.string_to_keysym(KEYS.get(name, name))
        if sym == 0:
            sys.exit("unknown key " + name)
        return self.d.keysym_to_keycode(sym)

    def key(self, name, down):
        xtest.fake_input(self.d, X.KeyPress if down else X.KeyRelease, self.keycode(name))
        self.d.sync()

    def shot(self, name):
        g = self.win.get_geometry()
        pos = self.win.translate_coords(self.root, 0, 0)
        x, y = -pos.x, -pos.y
        img = ImageGrab.grab(bbox=(x, y, x + g.width, y + g.height), xdisplay=os.environ.get("DISPLAY", ":0"))
        path = os.path.join(self.out, name + ".png")
        img.save(path)
        print("shot", path, flush=True)

    def run(self, lines):
        for raw in lines:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            cmd, args = parts[0], parts[1:]
            if cmd == "wait":
                time.sleep(float(args[0]))
            elif cmd == "press":
                for k in args:
                    self.key(k, True)
            elif cmd == "release":
                for k in args:
                    self.key(k, False)
            elif cmd == "tap":
                self.key(args[0], True)
                time.sleep((float(args[1]) if len(args) > 1 else 40) / 1000.0)
                self.key(args[0], False)
            elif cmd == "hold":
                self.key(args[0], True)
                time.sleep(float(args[1]) / 1000.0)
                self.key(args[0], False)
            elif cmd == "chord":
                ms = 40
                keys = args
                if args and args[-1].isdigit():
                    ms = int(args[-1])
                    keys = args[:-1]
                for k in keys:
                    self.key(k, True)
                time.sleep(ms / 1000.0)
                for k in keys:
                    self.key(k, False)
            elif cmd == "step":
                for _ in range(int(args[0])):
                    self.key("f6", True)
                    time.sleep(0.015)
                    self.key("f6", False)
                    time.sleep(0.03)
            elif cmd == "shot":
                self.shot(args[0])
            elif cmd == "focus":
                self.focus()
            else:
                sys.exit("unknown command " + cmd)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--window", default="Mortal Slopbat")
    ap.add_argument("--out", default="/tmp/slopbat_shots")
    ap.add_argument("script", nargs="?")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    drv = Driver(a.window, a.out)
    lines = open(a.script).read().splitlines() if a.script else sys.stdin.read().splitlines()
    drv.run(lines)


if __name__ == "__main__":
    main()
