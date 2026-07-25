"""Generates the bundled example schemes (data/examples/*.bson).

Implements the same tick rules as the game, simulates every scheme and
asserts its truth table before writing, so a scheme that ends up in the
build is guaranteed to work. Run from the repository root:

    python tools/gen_examples.py
"""
import struct
import time
import os

SWITCH, CLOCK, LAMP, BUTTON = 12, 13, 14, 15
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
    if t in (SWITCH, BUTTON): return False
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
        elif t == BUTTON:
            feed(x + 1, y); feed(x, y - 1); feed(x, y + 1); feed(x - 1, y)
            b[2] = False  # buttons are momentary: one pulse, then release

    for pos, b in blocks.items():
        if b[0] not in (SWITCH, CLOCK, BUTTON):
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


def save(board, name, cx, cy, zoom=1.0, timestamp=None):
    if timestamp is None:
        timestamp = int(time.time() * 1000)
    arr = []
    for (x, y), (t, r, a) in sorted(board.blocks.items()):
        pos = ((x << 32) | (y & 0xFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF
        if pos >= 2 ** 63:
            pos -= 2 ** 64
        arr.append({"pos": pos, "type": t, "rotation": r, "active": bool(a)})
    doc = {
        "camera": {"position": {"x": 32.0 * cx - 640.0, "y": 32.0 * cy - 360.0}, "zoom": float(zoom)},
        "meta": {"version": 1, "timestamp": timestamp},
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

def build_full_adder(b, ox, oy, cin_switch):
    """One full-adder stage. A switch at (ox,oy+8), B switch at (ox,oy-4).
    Cin: a switch at (ox-2,oy+4) when cin_switch, otherwise an angled-wire
    port at the same cell fed from (ox-3,oy+4). Sum lamp at (ox+8,oy+4),
    Cout leaves the stage at (ox+8,oy) heading right."""
    b.put(ox + 6, oy + 4, XOR, 1)  # Sum: odd number of inputs
    b.path([(ox + 7, oy + 4), (ox + 8, oy + 4)])
    b.put(ox + 8, oy + 4, LAMP)  # Sum
    b.put(ox + 6, oy, AND, 1)  # Cout: majority (at least 2 inputs)
    b.path([(ox + 7, oy), (ox + 8, oy)])  # Cout exits right

    b.put(ox, oy + 8, SWITCH)  # A
    b.path([(ox + 1, oy + 8), (ox + 6, oy + 8), (ox + 6, oy + 4)])  # A -> XOR (top)
    b.path([(ox, oy + 7), (ox, oy + 2), (ox + 6, oy + 2), (ox + 6, oy)])  # A -> AND (top)

    b.put(ox, oy - 4, SWITCH)  # B
    b.path([(ox + 1, oy - 4), (ox + 6, oy - 4), (ox + 6, oy)])  # B -> AND (bottom)
    b.path([(ox, oy - 3), (ox + 3, oy - 3), (ox + 3, oy + 3), (ox + 6, oy + 3), (ox + 6, oy + 4)],
           jumps=((ox + 3, oy + 1),))  # B -> XOR (bottom)

    if cin_switch:
        b.put(ox - 2, oy + 4, SWITCH)  # Cin
    else:
        b.put(ox - 2, oy + 4, WIRE_R, 1)  # Cin port: fans right and down
    b.path([(ox - 1, oy + 4), (ox + 6, oy + 4)], jumps=((ox - 1, oy + 4),))  # Cin -> XOR (left)
    b.path([(ox - 2, oy + 3), (ox - 2, oy), (ox + 6, oy)], jumps=((ox + 2, oy),))  # Cin -> AND (left)


def full_adder():
    b = Board()
    build_full_adder(b, 0, 0, cin_switch=True)
    b.path([(8, 0), (9, 0)])
    b.put(9, 0, LAMP)  # Cout lamp right after the exit wire

    for a in (0, 1):
        for c in (0, 1):
            for d in (0, 1):
                b.set_switch(0, 8, a)
                b.set_switch(0, -4, c)
                b.set_switch(-2, 4, d)
                settle(b.blocks)
                total = a + c + d
                assert b.lamp(8, 4) == (total % 2 == 1), f"Sum({a},{c},{d})"
                assert b.lamp(9, 0) == (total >= 2), f"Cout({a},{c},{d})"
    for x, y in ((0, 8), (0, -4), (-2, 4)):
        b.set_switch(x, y, 0)
    settle(b.blocks)
    save(b, "full-adder", 3, 2)


# --- 5. 4-bit ripple-carry adder: four full-adder stages, carry routed down ---

def adder_4bit():
    b = Board()
    STEP = 16
    for i in range(4):
        oy = -i * STEP
        build_full_adder(b, 0, oy, cin_switch=(i == 0))
        if i < 3:
            # carry: from this stage's Cout exit down and around into the
            # next stage's Cin port at (-2, oy-16+4). The horizontal run sits
            # at oy-6: one row further from the next stage's A switch, so the
            # switch cannot feed the carry bus directly.
            b.path([(8, oy), (8, oy - 6), (-4, oy - 6), (-4, oy - 12), (-2, oy - 12)])
        else:
            b.path([(8, oy), (9, oy)])
            b.put(9, oy, LAMP)  # final carry-out lamp

    def set_inputs(a, c, cin):
        for i in range(4):
            b.set_switch(0, -i * STEP + 8, (a >> i) & 1)
            b.set_switch(0, -i * STEP - 4, (c >> i) & 1)
        b.set_switch(-2, 4, cin)

    for a in range(16):
        for c in range(16):
            for cin in (0, 1):
                set_inputs(a, c, cin)
                settle(b.blocks, 220)
                total = a + c + cin
                for i in range(4):
                    assert b.lamp(8, -i * STEP + 4) == bool((total >> i) & 1), \
                        f"Sum{i}({a}+{c}+{cin})"
                assert b.lamp(9, -3 * STEP) == (total >= 16), f"Cout({a}+{c}+{cin})"
    set_inputs(0, 0, 0)
    settle(b.blocks, 220)
    save(b, "adder-4bit", 3, -18, zoom=2.2)


if __name__ == "__main__":
    blinker()
    gates()
    rs_latch()
    full_adder()
    adder_4bit()
    print("all example schemes verified")
