"""Generates the bundled example schemes (data/examples/*.bson).

Implements the same tick rules as the game, simulates every scheme and
asserts its truth table before writing, so a scheme that ends up in the
build is guaranteed to work. Run from the repository root:

    python tools/gen_examples.py
"""
import struct
import time
import os

SWITCH, CLOCK, LAMP = 12, 13, 14
NOT, AND, NAND, XOR, NXOR = 7, 8, 9, 10, 11
WIRE, WIRE_R, WIRE_L, WIRE_T, WIRE_X, WIRE2, WIRE3 = 0, 1, 2, 3, 4, 5, 6

DIRS = {0: (0, 1), 1: (1, 0), 2: (0, -1), 3: (-1, 0)}


# --- BSON encoder (nlohmann::json::from_bson compatible subset) ---

def _elem(name, value):
    key = name.encode() + b"\0"
    if isinstance(value, bool):
        return b"\x08" + key + (b"\x01" if value else b"\x00")
    if isinstance(value, float):
        return b"\x01" + key + struct.pack("<d", value)
    if isinstance(value, int):
        if -2 ** 31 <= value < 2 ** 31:
            return b"\x10" + key + struct.pack("<i", value)
        return b"\x12" + key + struct.pack("<q", value)
    if isinstance(value, dict):
        return b"\x03" + key + _doc(value)
    if isinstance(value, list):
        return b"\x04" + key + _doc({str(i): v for i, v in enumerate(value)})
    raise TypeError(type(value))


def _doc(d):
    body = b"".join(_elem(k, v) for k, v in d.items())
    return struct.pack("<i", len(body) + 5) + body + b"\0"


# --- simulation (mirror of Circuit::tick) ---

def rot(r, k):
    return (r + k) % 4


def is_active(t, c):
    if t == NOT: return c == 0
    if t == AND: return c >= 2
    if t == NAND: return c < 2
    if t == XOR: return c % 2 == 1
    if t == NXOR: return c % 2 == 0
    if t == SWITCH: return False
    if t == CLOCK: return c == 0
    return c > 0


def tick(blocks):
    conn = {}

    def feed(x, y):
        if (x, y) in blocks:
            conn[(x, y)] = conn.get((x, y), 0) + 1

    def out(x, y, r, l=1):
        dx, dy = DIRS[r]
        feed(x + dx * l, y + dy * l)

    for (x, y), b in blocks.items():
        t, r, a = b
        if t == CLOCK:
            b[2] = a = not a
            if a:
                out(x, y, r)
            continue
        if not a:
            continue
        if t in (WIRE, NOT, AND, NAND, XOR, NXOR):
            out(x, y, r)
        elif t == WIRE2:
            out(x, y, r, 2)
        elif t == WIRE3:
            out(x, y, r, 3)
        elif t == WIRE_R:
            out(x, y, r); out(x, y, rot(r, 1))
        elif t == WIRE_L:
            out(x, y, r); out(x, y, rot(r, -1))
        elif t == WIRE_T:
            out(x, y, rot(r, -1)); out(x, y, rot(r, 1))
        elif t == WIRE_X:
            out(x, y, rot(r, -1)); out(x, y, r); out(x, y, rot(r, 1))
        elif t == SWITCH:
            feed(x + 1, y); feed(x, y - 1); feed(x, y + 1); feed(x - 1, y)

    for pos, b in blocks.items():
        if b[0] not in (SWITCH, CLOCK):
            b[2] = is_active(b[0], conn.get(pos, 0))


def settle(blocks, ticks=40):
    for _ in range(ticks):
        tick(blocks)


# --- board building ---

class Board:
    def __init__(self):
        self.blocks = {}

    def put(self, x, y, t, r=0, a=False):
        assert (x, y) not in self.blocks, f"collision at {(x, y)}"
        self.blocks[(x, y)] = [t, int(r), bool(a)]

    def path(self, points, jumps=()):
        """Straight wires along a polyline; `points` are corner cells, the last
        point is the consumer and is not placed. Cells listed in `jumps` get a
        2-wire that skips the next cell along the travel direction."""
        cells = []
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            dx = (x1 > x0) - (x1 < x0)
            dy = (y1 > y0) - (y1 < y0)
            assert (dx == 0) != (dy == 0), f"diagonal segment {(x0, y0)}->{(x1, y1)}"
            x, y = x0, y0
            while (x, y) != (x1, y1):
                cells.append((x, y))
                x += dx
                y += dy
        cells.append(points[-1])
        i = 0
        while i < len(cells) - 1:
            (x, y), (nx, ny) = cells[i], cells[i + 1]
            r = {(0, 1): 0, (1, 0): 1, (0, -1): 2, (-1, 0): 3}[
                ((nx > x) - (nx < x), (ny > y) - (ny < y))]
            if (x, y) in jumps:
                self.put(x, y, WIRE2, r)
                i += 2  # the skipped cell is not placed
            else:
                self.put(x, y, WIRE, r)
                i += 1

    def set_switch(self, x, y, value):
        self.blocks[(x, y)][2] = bool(value)

    def lamp(self, x, y):
        return self.blocks[(x, y)][2]


def save(board, name, cx, cy, zoom=1.0):
    arr = []
    for (x, y), (t, r, a) in sorted(board.blocks.items()):
        pos = ((x << 32) | (y & 0xFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF
        if pos >= 2 ** 63:
            pos -= 2 ** 64
        arr.append({"pos": pos, "type": t, "rotation": r, "active": bool(a)})
    doc = {
        "camera": {"position": {"x": 32.0 * cx - 640.0, "y": 32.0 * cy - 360.0}, "zoom": float(zoom)},
        "meta": {"version": 1, "timestamp": int(time.time() * 1000)},
        "blocks": arr,
    }
    os.makedirs("data/examples", exist_ok=True)
    path = f"data/examples/{name}.bson"
    with open(path, "wb") as f:
        f.write(_doc(doc))
    print(f"{path}: {len(arr)} blocks")


# --- 1. blinker: clock -> wires -> lamp ---

def blinker():
    b = Board()
    b.put(0, 0, CLOCK, 1)
    b.path([(1, 0), (3, 0)])
    b.put(3, 0, LAMP)
    settle(b.blocks, 10)
    seen = set()
    for _ in range(8):
        tick(b.blocks)
        seen.add(b.lamp(3, 0))
    assert seen == {True, False}, "blinker lamp must toggle"
    save(b, "blinker", 1.5, 0)


# --- 2. logic gates: NOT, AND, XOR rigs with switches and lamps ---

def gates():
    b = Board()
    # NOT rig (row 8)
    b.put(0, 8, SWITCH)
    b.path([(1, 8), (2, 8)])
    b.put(2, 8, NOT, 1)
    b.path([(3, 8), (4, 8)])
    b.put(4, 8, LAMP)
    # AND rig (rows 3..5)
    b.put(0, 5, SWITCH)
    b.put(0, 3, SWITCH)
    b.path([(1, 5), (3, 5), (3, 4)])
    b.path([(1, 3), (3, 3), (3, 4)])
    b.put(3, 4, AND, 1)
    b.path([(4, 4), (5, 4)])
    b.put(5, 4, LAMP)
    # XOR rig (rows -2..0)
    b.put(0, 0, SWITCH)
    b.put(0, -2, SWITCH)
    b.path([(1, 0), (3, 0), (3, -1)])
    b.path([(1, -2), (3, -2), (3, -1)])
    b.put(3, -1, XOR, 1)
    b.path([(4, -1), (5, -1)])
    b.put(5, -1, LAMP)

    for a in (0, 1):
        b.set_switch(0, 8, a)
        settle(b.blocks)
        assert b.lamp(4, 8) == (not a), f"NOT({a})"
    for a in (0, 1):
        for c in (0, 1):
            b.set_switch(0, 5, a)
            b.set_switch(0, 3, c)
            b.set_switch(0, 0, a)
            b.set_switch(0, -2, c)
            settle(b.blocks)
            assert b.lamp(5, 4) == (a and c), f"AND({a},{c})"
            assert b.lamp(5, -1) == (a != c), f"XOR({a},{c})"
    for x, y in ((0, 8), (0, 5), (0, 3), (0, 0), (0, -2)):
        b.set_switch(x, y, 0)
    settle(b.blocks)
    save(b, "gates", 2.5, 3)


# --- 3. RS latch on two cross-coupled NANDs ---

def rs_latch():
    b = Board()
    b.put(2, 4, NAND, 1)  # Q side
    b.put(2, 0, NAND, 3)  # /Q side
    # S input: switch -> NOT -> NAND1 (idle high, press = set)
    b.put(2, 7, SWITCH)
    b.put(2, 6, NOT, 2)
    b.path([(2, 5), (2, 4)])
    # R input: switch -> NOT -> NAND2
    b.put(2, -3, SWITCH)
    b.put(2, -2, NOT, 0)
    b.path([(2, -1), (2, 0)])
    # Q output branch: lamp + feedback into NAND2
    b.put(3, 4, WIRE_R, 1)  # right to lamp, down to feedback
    b.path([(4, 4), (5, 4)])
    b.put(5, 4, LAMP)  # Q
    b.path([(3, 3), (3, 1), (2, 1), (2, 0)])
    # /Q output branch: lamp + feedback into NAND1
    b.put(1, 0, WIRE_R, 3)  # left to lamp, up to feedback
    b.path([(0, 0), (-1, 0)])
    b.put(-1, 0, LAMP)  # /Q
    b.path([(1, 1), (1, 3), (2, 3), (2, 4)])

    def press(x, y):
        b.set_switch(x, y, 1)
        settle(b.blocks)
        b.set_switch(x, y, 0)
        settle(b.blocks)

    press(2, 7)  # set
    assert b.lamp(5, 4) and not b.lamp(-1, 0), "latch must hold Q after S"
    state = [b.lamp(5, 4), b.lamp(-1, 0)]
    for _ in range(10):  # must be stable, not oscillating
        tick(b.blocks)
        assert [b.lamp(5, 4), b.lamp(-1, 0)] == state, "latch oscillates"
    press(2, -3)  # reset
    assert not b.lamp(5, 4) and b.lamp(-1, 0), "latch must hold /Q after R"
    press(2, 7)  # leave it in the set state for the bundled file
    assert b.lamp(5, 4) and not b.lamp(-1, 0)
    save(b, "rs-latch", 2, 2)


# --- 4. full adder: Sum = XOR of 3 inputs (parity), Cout = AND block (>=2) ---

def full_adder():
    b = Board()
    b.put(6, 4, XOR, 1)  # Sum: odd number of inputs
    b.path([(7, 4), (8, 4)])
    b.put(8, 4, LAMP)  # Sum
    b.put(6, 0, AND, 1)  # Cout: majority (at least 2 inputs)
    b.path([(7, 0), (8, 0)])
    b.put(8, 0, LAMP)  # Cout

    b.put(0, 8, SWITCH)  # A
    b.path([(1, 8), (6, 8), (6, 4)])  # A -> XOR (top)
    b.path([(0, 7), (0, 2), (6, 2), (6, 0)])  # A -> AND (top)

    b.put(0, -4, SWITCH)  # B
    b.path([(1, -4), (6, -4), (6, 0)])  # B -> AND (bottom)
    b.path([(0, -3), (3, -3), (3, 3), (6, 3), (6, 4)], jumps=((3, 1),))  # B -> XOR (bottom)

    b.put(-2, 4, SWITCH)  # Cin
    b.path([(-1, 4), (6, 4)], jumps=((-1, 4),))  # Cin -> XOR (left)
    b.path([(-2, 3), (-2, 0), (6, 0)], jumps=((2, 0),))  # Cin -> AND (left)

    for a in (0, 1):
        for c in (0, 1):
            for d in (0, 1):
                b.set_switch(0, 8, a)
                b.set_switch(0, -4, c)
                b.set_switch(-2, 4, d)
                settle(b.blocks)
                total = a + c + d
                assert b.lamp(8, 4) == (total % 2 == 1), f"Sum({a},{c},{d})"
                assert b.lamp(8, 0) == (total >= 2), f"Cout({a},{c},{d})"
    for x, y in ((0, 8), (0, -4), (-2, 4)):
        b.set_switch(x, y, 0)
    settle(b.blocks)
    save(b, "full-adder", 3, 2)


if __name__ == "__main__":
    blinker()
    gates()
    rs_latch()
    full_adder()
    print("all example schemes verified")
