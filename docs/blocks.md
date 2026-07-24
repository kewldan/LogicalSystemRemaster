# Block reference

Every block occupies one cell of the infinite grid and has a rotation
(0 = up, 1 = right, 2 = down, 3 = left). Each simulation tick has two phases:

1. every **active** block *emits* into the cells its type points at, counting
   one *connection* per incoming signal;
2. every block (except Switch, Clock and Button) recomputes its active state
   from the number of connections it received and the counters reset.

A signal therefore travels one block per tick. Merging two wires into the same
cell acts as an **OR**; the input count is what gates evaluate.

| id | Name | Emits to | Becomes active when |
|----|------|----------|---------------------|
| 0 | Wire straight | facing cell | ≥ 1 input |
| 1 | Wire angled right | facing + clockwise neighbor | ≥ 1 input |
| 2 | Wire angled left | facing + counter-clockwise neighbor | ≥ 1 input |
| 3 | Wire T | both side neighbors | ≥ 1 input |
| 4 | Wire cross | facing + both sides | ≥ 1 input |
| 5 | Wire 2 | cell **2** away (jumps over 1) | ≥ 1 input |
| 6 | Wire 3 | cell **3** away (jumps over 2) | ≥ 1 input |
| 7 | NOT | facing cell | exactly 0 inputs |
| 8 | AND | facing cell | ≥ 2 inputs |
| 9 | NAND | facing cell | < 2 inputs |
| 10 | XOR | facing cell | odd number of inputs |
| 11 | NXOR | facing cell | even number of inputs |
| 12 | Switch | all 4 neighbors | toggled by clicking, ignores inputs |
| 13 | Clock | facing cell | toggles itself every tick |
| 14 | Lamp | nothing | ≥ 1 input |
| 15 | Button | all 4 neighbors | click = a single-tick pulse, then it releases itself |

Because gates count inputs rather than reading two fixed pins, some circuits
collapse dramatically: a full adder is just **one XOR** (sum = parity of
A+B+Cin) and **one AND** (carry = at least two of three), see the bundled
examples. Wire 2/Wire 3 double as crossovers: they jump over cells another
line runs through.

## File format (`.bson`)

A BSON document (`nlohmann::json::to_bson`):

```
camera: { position: { x: float, y: float }, zoom: float }
meta:   { version: 1, timestamp: int64 (ms) }
blocks: [ { pos: int64, type: int, rotation: int, active: bool }, ... ]
```

`pos` packs the cell coordinates as `(x << 32) | (y & 0xFFFFFFFF)`.
Unknown block types and rotations are rejected on load.

The legacy `.ls` format is 16 bytes of header (camera x/y/zoom as floats,
block count as int32) followed by 11-byte records: 8 bytes `pos`, one byte
each of type, rotation, active.

## Clipboard string

Copy/paste and Export/Import share one format: the same 11-byte records
(positions relative to the cursor for copy, absolute for export),
compressed with zlib and encoded as base64. Any scheme can be shared as
plain text — paste the string and press `Ctrl+V` in the app.

Bundled examples are produced by `tools/gen_examples.py`, which simulates
every scheme with the same rules and asserts its truth table before
writing the file.
