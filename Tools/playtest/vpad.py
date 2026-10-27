#!/usr/bin/env python3
"""A virtual gamepad for playtesting without touching a physical controller: a kernel input device
made through /dev/uinput, then a script of presses and stick moves. It identifies as a DualShock 4
(Sony 054c:09cc, PlayStation button layout; the default) or an Xbox 360 pad, so the engine's SDL
maps it as it maps those pads read through evdev. It is a simulation: it exercises the kernel,
SDL and the game's input path, not a real pad's Bluetooth link, rumble or latency.

Script lines:  wait <sec> | tap <button> [ms] | hold <button> <ms> | press <button> | release <button>
               | stick <x> <y> [ms]  (-1..1, back to centre after ms; y +1 is down) | dpad <up|down|left|right> [ms]
               | trigger <l|r> <0..1> [ms] | waitlog <text> [timeout sec]  (wait for a line in the game's log,
               whose path is $SLOP_LOG)
               | unplug | replug  (destroy and recreate the device, as a cable pulled and put back)
               | focus <window title text>  (raise the game window with wmctrl: SDL drops pad events
                 while the window is in the background, as it would for a player's unfocused window)
Buttons: cross circle square triangle l1 r1 l2 r2 share options ps l3 r3 (Xbox names a b x y lb rb back start also work)
Usage:  python3 -I Tools/playtest/vpad.py [--xbox] script.txt      (needs write access to /dev/uinput)
"""
import fcntl
import os
import struct
import subprocess
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT = 0
# Linux gamepad codes. On a DualShock 4 (hid-playstation): south cross, east circle, north
# triangle, west square.
SOUTH, EAST, NORTH, WEST = 0x130, 0x131, 0x133, 0x134
TL, TR, TL2, TR2, SELECT, START, MODE, THUMBL, THUMBR = 0x136, 0x137, 0x138, 0x139, 0x13A, 0x13B, 0x13C, 0x13D, 0x13E
BTN = {"cross": SOUTH, "circle": EAST, "triangle": NORTH, "square": WEST, "l1": TL, "r1": TR, "l2": TL2, "r2": TR2,
       "share": SELECT, "options": START, "ps": MODE, "l3": THUMBL, "r3": THUMBR,
       "a": SOUTH, "b": EAST, "y": NORTH, "x": WEST, "lb": TL, "rb": TR, "back": SELECT, "start": START}
ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y = 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x11
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
ABS_CNT = 64


class Pad:
    def __init__(self, xbox=False):
        self.xbox = xbox
        self.create()

    def create(self):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_ABS)
        for code in sorted(set(BTN.values())):
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, code)
        absmin, absmax = [0] * ABS_CNT, [0] * ABS_CNT
        # DualShock 4 sticks and triggers report 0..255; the Xbox 360 pad's sticks are signed 16-bit.
        lo, hi = (-32768, 32767) if self.xbox else (0, 255)
        for a in (ABS_X, ABS_Y, ABS_RX, ABS_RY):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = lo, hi
        for a in (ABS_Z, ABS_RZ):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = 0, 255
        for a in (ABS_HAT0X, ABS_HAT0Y):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = -1, 1
        if self.xbox:
            name, vendor, product, version = b"Microsoft X-Box 360 pad (virtual)", 0x045E, 0x028E, 0x0114
        else:
            name, vendor, product, version = b"Sony Interactive Entertainment Wireless Controller (virtual)", 0x054C, 0x09CC, 0x8111
        dev = struct.pack("80sHHHHi", name, 0x03, vendor, product, version, 0)
        dev += struct.pack("%di" % ABS_CNT, *absmax) + struct.pack("%di" % ABS_CNT, *absmin)
        dev += struct.pack("%di" % ABS_CNT, *([0] * ABS_CNT)) + struct.pack("%di" % ABS_CNT, *([0] * ABS_CNT))
        os.write(self.fd, dev)
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        time.sleep(1.0)
        self.centre()

    def emit(self, etype, code, value):
        t = time.time()
        os.write(self.fd, struct.pack("llHHi", int(t), int((t % 1) * 1e6), etype, code, value))

    def sync(self):
        self.emit(EV_SYN, SYN_REPORT, 0)

    def axis(self, v):
        v = max(-1.0, min(1.0, v))
        return int(v * 32767) if self.xbox else int(round(127.5 + v * 127.5))

    def centre(self):
        for a in (ABS_X, ABS_Y, ABS_RX, ABS_RY):
            self.emit(EV_ABS, a, self.axis(0.0))
        self.sync()

    def button(self, name, down):
        self.emit(EV_KEY, BTN[name], 1 if down else 0)
        self.sync()

    def stick(self, x, y):
        self.emit(EV_ABS, ABS_X, self.axis(x))
        self.emit(EV_ABS, ABS_Y, self.axis(y))
        self.sync()

    def trigger(self, side, v):
        self.emit(EV_ABS, ABS_Z if side == "l" else ABS_RZ, int(max(0.0, min(1.0, v)) * 255))
        self.sync()

    def hat(self, x, y):
        self.emit(EV_ABS, ABS_HAT0X, x)
        self.emit(EV_ABS, ABS_HAT0Y, y)
        self.sync()

    def close(self):
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)


def waitlog(path, text, limit):
    end = time.time() + limit
    while time.time() < end:
        try:
            with open(path, errors="replace") as f:
                if text in f.read():
                    return
        except OSError:
            pass
        time.sleep(0.1)
    sys.exit(f"waitlog timed out: {text!r} in {path}")


def run(pad, lines):
    for raw in lines:
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        p = line.split()
        cmd, args = p[0], p[1:]
        ms = lambda i, d: (float(args[i]) if len(args) > i else d) / 1000.0
        if cmd == "wait":
            time.sleep(float(args[0]))
        elif cmd == "tap":
            pad.button(args[0], True)
            time.sleep(ms(1, 70))
            pad.button(args[0], False)
        elif cmd == "hold":
            pad.button(args[0], True)
            time.sleep(ms(1, 200))
            pad.button(args[0], False)
        elif cmd == "press":
            pad.button(args[0], True)
        elif cmd == "release":
            pad.button(args[0], False)
        elif cmd == "stick":
            pad.stick(float(args[0]), float(args[1]))
            time.sleep(ms(2, 150))
            pad.stick(0, 0)
        elif cmd == "trigger":
            pad.trigger(args[0], float(args[1]))
            time.sleep(ms(2, 200))
            pad.trigger(args[0], 0.0)
        elif cmd == "dpad":
            d = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}[args[0]]
            pad.hat(*d)
            time.sleep(ms(1, 90))
            pad.hat(0, 0)
        elif cmd == "waitlog":
            limit = 120.0
            if args[-1].replace(".", "", 1).isdigit():
                limit = float(args[-1])
                args = args[:-1]
            waitlog(os.environ["SLOP_LOG"], " ".join(args), limit)
        elif cmd == "focus":
            subprocess.run(["wmctrl", "-a", " ".join(args)], check=False)
            time.sleep(0.5)
        elif cmd == "unplug":
            pad.close()
        elif cmd == "replug":
            pad.create()
        else:
            sys.exit("unknown command " + cmd)
        print(f"{time.strftime('%H:%M:%S')} {line}", flush=True)


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    pad = Pad(xbox="--xbox" in sys.argv)
    try:
        run(pad, open(args[0]).read().splitlines())
    finally:
        try:
            pad.close()
        except OSError:
            pass
