# 8-bit click adder

`click-adder.bson` is an interactive decimal calculator built entirely from
the ordinary blocks available in Logical System. It contains no hidden
counter, adder, register, decoder, or display component.

## Using it

Open **Examples → 8-bit click adder + decimal display**, set a high Turbo
simulation rate, and press Play.

There are three red momentary-button pads in one compact row above the
display groups. Every cell in a pad is clickable:

- the left button increments input `A`;
- the middle button increments input `B`;
- the right button stores `A + B` in the result register.

Each click adds one, so clicking the left button 12 times and the middle
button 34 times enters `A = 12` and `B = 34`. Click the right button once to
show `046` as the sum. Inputs wrap from 255 to 0; the result is held in a
16-bit register and can range from 0 to 510.

The three groups of three decimal seven-segment displays, from left to right,
show `A`, `B`, and the last stored sum. Leading zeroes are intentional.

Allow the signal wave to finish after each click before pressing another
button. At 65,536 TPS this is effectively immediate.

## Circuit

- two 8-bit NAND-latch input registers;
- two 8-bit ripple-carry incrementers;
- one 9-bit ripple-carry adder feeding a 16-bit result register;
- three binary-to-decimal gate ROMs;
- nine continuous seven-segment lamp displays with shared visual corners;
- timed button control paths that perform each update without manual ticks.

The generated example contains about 397,000 blocks.

## Regenerating

The checked-in BSON file is generated deterministically:

```sh
python3 tools/gen_click_adder.py
```

The regression test enters `1` and `2`, stores their sum, checks all three
registers, and verifies every lamp in the displayed `001 + 002 = 003`.
