# 16-bit SUBLEQ computer

`cpu16-1k.bson` is a complete, autonomous computer built only from the
ordinary blocks available in Logical System. It does not use a hidden CPU,
register, memory, or word-level arithmetic component.

## Architecture

- 16-bit words and arithmetic
- 9-bit addresses
- 512 writable words = 1024 bytes
- 8192 cross-coupled NAND-latch memory cells
- eight 16-bit registers: `PC`, `PC_NEXT`, `MAR`, `I0`, `I1`, `I2`, `A`, `B`
- ripple-carry incrementer and subtractor
- autonomous 18-phase hard-wired control unit

The instruction set has one instruction:

```text
SUBLEQ A, B, C
RAM[B] = RAM[B] - RAM[A]
if signed(RAM[B]) <= 0: PC = C
```

Otherwise execution continues with the next three-word instruction. One
instruction takes 88,632 simulation ticks because signals physically travel
through wires one block per tick.

## Included program

The initial memory image repeatedly decrements word 101 from 5 to zero:

```text
0:  100, 101, 6    # subtract RAM[100] (=1) from RAM[101]
3:  102, 102, 0    # unconditional branch to address 0
6:  102, 102, 6    # halt loop

100: 1
101: 5
102: 0
```

Open **Examples → 16-bit SUBLEQ + 1 KiB RAM**, select a Turbo simulation rate
(up to 65,536 TPS), and press Play. The example contains about 968,000 blocks,
so loading and rendering it is substantially heavier than the smaller
examples.

## Regenerating

The checked-in BSON file is generated deterministically:

```sh
python3 tools/gen_cpu16.py
```

The regression test loads the generated circuit, verifies the first `5 → 4`
subtraction, then runs the full program through zero and its conditional
halt branch.
