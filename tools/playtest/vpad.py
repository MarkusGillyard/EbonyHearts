#!/usr/bin/env python3
"""A virtual gamepad for playtesting without a physical controller: creates a kernel input device
through /dev/uinput that identifies as an Xbox 360 pad (so the engine's controller database maps
it), then plays a script of button presses and stick moves. It is a simulation of a controller,
not a physical one; results from it say so.

Script lines:  wait <sec> | tap <button> [ms] | hold <button> <ms> | press <button> | release <button>
               | stick <x> <y> [ms]  (values -1..1, returns to centre after ms) | dpad <up|down|left|right> [ms]
               | waitfile <path> [timeout sec]  (e.g. a screenshot xdrive takes, to keep two scripts in step)
Buttons: a b x y lb rb start back
Usage:  python3 tools/playtest/vpad.py script.txt      (needs write access to /dev/uinput)
"""
import fcntl
import os
import struct
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT = 0
BTN = {"a": 0x130, "b": 0x131, "x": 0x133, "y": 0x134, "lb": 0x136, "rb": 0x137, "back": 0x13A, "start": 0x13B, "mode": 0x13C,
       "thumbl": 0x13D, "thumbr": 0x13E}
ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y = 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x11
UI_SET_EVBIT = 0x40045564
UI_SET_KEYBIT = 0x40045565
UI_SET_ABSBIT = 0x40045567
UI_DEV_CREATE = 0x5501
UI_DEV_DESTROY = 0x5502
ABS_CNT = 64


class Pad:
    def __init__(self):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_ABS)
        for code in BTN.values():
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, code)
        absmin = [0] * ABS_CNT
        absmax = [0] * ABS_CNT
        for a in (ABS_X, ABS_Y, ABS_RX, ABS_RY):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = -32768, 32767
        for a in (ABS_Z, ABS_RZ):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = 0, 255
        for a in (ABS_HAT0X, ABS_HAT0Y):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = -1, 1
        name = b"Microsoft X-Box 360 pad (virtual)"
        # struct uinput_user_dev: name[80], input_id {bustype, vendor, product, version}, ff_effects_max,
        # absmax[64], absmin[64], absfuzz[64], absflat[64]
        dev = struct.pack("80sHHHHi", name, 0x03, 0x045E, 0x028E, 0x0114, 0)
        dev += struct.pack("%di" % ABS_CNT, *absmax) + struct.pack("%di" % ABS_CNT, *absmin)
        dev += struct.pack("%di" % ABS_CNT, *([0] * ABS_CNT)) + struct.pack("%di" % ABS_CNT, *([0] * ABS_CNT))
        os.write(self.fd, dev)
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        time.sleep(1.0)

    def emit(self, etype, code, value):
        t = time.time()
        os.write(self.fd, struct.pack("llHHi", int(t), int((t % 1) * 1e6), etype, code, value))

    def sync(self):
        self.emit(EV_SYN, SYN_REPORT, 0)

    def button(self, name, down):
        self.emit(EV_KEY, BTN[name], 1 if down else 0)
        self.sync()

    def stick(self, x, y):
        self.emit(EV_ABS, ABS_X, int(max(-1, min(1, x)) * 32767))
        self.emit(EV_ABS, ABS_Y, int(max(-1, min(1, y)) * 32767))
        self.sync()

    def hat(self, x, y):
        self.emit(EV_ABS, ABS_HAT0X, x)
        self.emit(EV_ABS, ABS_HAT0Y, y)
        self.sync()

    def close(self):
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)


def run(pad, lines):
    for raw in lines:
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        p = line.split()
        cmd, args = p[0], p[1:]
        if cmd == "wait":
            time.sleep(float(args[0]))
        elif cmd == "tap":
            pad.button(args[0], True)
            time.sleep((float(args[1]) if len(args) > 1 else 60) / 1000.0)
            pad.button(args[0], False)
        elif cmd == "hold":
            pad.button(args[0], True)
            time.sleep(float(args[1]) / 1000.0)
            pad.button(args[0], False)
        elif cmd == "press":
            pad.button(args[0], True)
        elif cmd == "release":
            pad.button(args[0], False)
        elif cmd == "stick":
            pad.stick(float(args[0]), float(args[1]))
            time.sleep((float(args[2]) if len(args) > 2 else 120) / 1000.0)
            pad.stick(0, 0)
        elif cmd == "waitfile":
            # The path may contain spaces; a trailing number is the timeout.
            limit = 60.0
            if len(args) > 1 and args[-1].replace(".", "", 1).isdigit():
                limit = float(args[-1])
                args = args[:-1]
            path = " ".join(args)
            end = time.time() + limit
            while not os.path.exists(path):
                if time.time() > end:
                    sys.exit("waitfile timed out: " + path)
                time.sleep(0.05)
        elif cmd == "dpad":
            d = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}[args[0]]
            pad.hat(*d)
            time.sleep((float(args[1]) if len(args) > 1 else 80) / 1000.0)
            pad.hat(0, 0)
        else:
            sys.exit("unknown command " + cmd)
        print(line, flush=True)


if __name__ == "__main__":
    pad = Pad()
    try:
        run(pad, open(sys.argv[1]).read().splitlines())
    finally:
        pad.close()
