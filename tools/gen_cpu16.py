"""Gate-level 16-bit SUBLEQ computer example.

The finished scheme uses only the sixteen ordinary game blocks.  Its memory
is four banks of 128 x 16 writable NAND-latch cells (512 words / 1024 bytes).
This file is deliberately separate from the small hand-laid examples: the
computer contains hundreds of thousands of blocks and is generated from
regular standard-cell geometry.
"""

from dataclasses import dataclass

from gen_examples import (
    AND,
    LAMP,
    NAND,
    NOT,
    WIRE,
    WIRE_L,
    WIRE_R,
    WIRE_T,
    WIRE_X,
    WIRE2,
    WIRE3,
    Board,
    save,
)


WORD_BITS = 16
ADDRESS_BITS = 9
BANKS = 4
ROWS_PER_BANK = 128
WORDS = BANKS * ROWS_PER_BANK
BYTES = WORDS * 2

ROW_PITCH = 20
BIT_PITCH = 22
BANK_PITCH = 520
BANK_X0 = 700
RAM_Y0 = -200


def _put_range(board, x0, x1, y, rotation, active=False):
    """Place a directed, inclusive horizontal run using 3-cell jump wires."""
    step = 1 if x1 >= x0 else -1
    assert rotation == (1 if step > 0 else 3)
    x = x0
    while True:
        remaining = abs(x1 - x)
        if remaining == 0:
            board.put(x, y, WIRE, rotation, active)
            break
        length = min(3, remaining)
        board.put(x, y, (WIRE, WIRE2, WIRE3)[length - 1], rotation, active)
        x += step * length


def _put_vertical_lane(board, x, y0, y1, rotation, specials, skipped):
    """Place a compact vertical bus with explicit branch/jump cells."""
    step = 1 if y1 >= y0 else -1
    assert rotation == (0 if step > 0 else 2)
    y = y0
    while True:
        assert y not in skipped
        if y in specials:
            t, r, active = specials[y]
            board.put(x, y, t, r, active)
            length = 2 if t == WIRE2 and r == rotation else 1
        else:
            if y == y1:
                board.put(x, y, WIRE, rotation)
                break
            max_length = min(3, abs(y1 - y))
            length = max_length
            for candidate in range(max_length, 0, -1):
                landing = y + step * candidate
                between = [y + step * n for n in range(1, candidate)]
                if (landing in skipped or
                        (landing != y1 and (x, landing) in board.blocks) or
                        any(pos in specials for pos in between)):
                    continue
                length = candidate
                break
            board.put(x, y, (WIRE, WIRE2, WIRE3)[length - 1], rotation)
        if y == y1:
            break
        y += step * length


def _put_control_line(board, x0, x1, y, branches, active=False):
    x = x0
    while True:
        if x in branches:
            t, r = branches[x]
            board.put(x, y, t, r, active)
            length = 1
        else:
            if x == x1:
                board.put(x, y, WIRE, 1, active)
                break
            max_length = min(3, x1 - x)
            length = max_length
            for candidate in range(max_length, 0, -1):
                if any((x + n) in branches for n in range(1, candidate)):
                    continue
                length = candidate
                break
            board.put(x, y, (WIRE, WIRE2, WIRE3)[length - 1], 1, active)
        if x == x1:
            break
        x += length


def _storage_cell(board, bx, y, value):
    """One writable/readable bit.

    Inputs are the vertical D and /D buses at bx+1 and bx+14, plus horizontal
    read-enable and write-enable lines at y+6 and y.  The cross-coupled NAND
    pair stores Q; an AND gate places Q on the upward read bus at bx+16.
    """
    value = bool(value)

    # D -> write-set NAND.
    _put_range(board, bx + 2, bx + 6, y + 4, 1)
    board.put(bx + 7, y + 4, NAND, 2, True)
    board.put(bx + 7, y + 3, WIRE, 2, True)

    # /D -> write-reset NAND.
    _put_range(board, bx + 13, bx + 8, y - 4, 3)
    board.put(bx + 7, y - 4, NAND, 0, True)
    board.put(bx + 7, y - 3, WIRE, 0, True)

    # Write-enable fans both upward and downward.  The upward arm jumps over
    # the D trace at y+4.
    board.put(bx + 5, y + 1, WIRE, 0)
    board.put(bx + 5, y + 2, WIRE, 0)
    board.put(bx + 5, y + 3, WIRE2, 0)
    board.put(bx + 5, y + 5, WIRE, 1)
    board.put(bx + 6, y + 5, WIRE, 1)
    board.put(bx + 7, y + 5, WIRE, 2)

    board.put(bx + 5, y - 1, WIRE, 2)
    board.put(bx + 5, y - 2, WIRE, 2)
    board.put(bx + 5, y - 3, WIRE, 2)
    board.put(bx + 5, y - 4, WIRE, 2)
    board.put(bx + 5, y - 5, WIRE, 1)
    board.put(bx + 6, y - 5, WIRE, 1)
    board.put(bx + 7, y - 5, WIRE, 0)

    # The state-holding pair.  Active feedback paths are saved explicitly so
    # a million-block scheme starts in a valid latch state on its first tick.
    board.put(bx + 7, y + 2, NAND, 1, value)       # Q
    board.put(bx + 7, y - 2, NAND, 0, not value)   # /Q

    board.put(bx + 8, y + 2, WIRE_R, 1, value)
    board.put(bx + 8, y + 1, WIRE2, 2, value)
    board.put(bx + 8, y - 1, WIRE, 2, value)
    board.put(bx + 8, y - 2, WIRE, 3, value)

    board.put(bx + 7, y - 1, WIRE2, 0, not value)
    board.put(bx + 7, y + 1, WIRE, 0, not value)

    # Q + read-enable -> shared upward read bus.  The output jumps over /D.
    board.put(bx + 9, y + 2, WIRE, 1, value)
    board.put(bx + 10, y + 2, WIRE, 1, value)
    board.put(bx + 11, y + 2, AND, 1)
    board.put(bx + 12, y + 2, WIRE, 1)
    board.put(bx + 13, y + 2, WIRE2, 1)
    board.put(bx + 15, y + 2, WIRE, 1)

    board.put(bx + 11, y + 5, WIRE, 2)
    board.put(bx + 11, y + 4, WIRE, 2)
    board.put(bx + 11, y + 3, WIRE, 2)


@dataclass(frozen=True)
class BankPorts:
    address: tuple
    mem_read: int
    mem_write: int
    data: tuple
    data_bar: tuple
    read: tuple
    top_y: int
    bottom_y: int


@dataclass(frozen=True)
class RegisterRow:
    name: str
    y: int
    read_pin: tuple
    write_pin: tuple


def _build_bank(board, bank, words):
    base = BANK_X0 + bank * BANK_PITCH
    row_ys = [RAM_Y0 - row * ROW_PITCH for row in range(ROWS_PER_BANK)]
    top = RAM_Y0 + 14
    bottom = row_ys[-1] - 8

    address_x = tuple(base + bit * 6 for bit in range(ADDRESS_BITS))
    mem_read_x = base + 64
    mem_write_x = base + 66
    cell_x0 = base + 80
    data_x = tuple(cell_x0 + bit * BIT_PITCH + 1 for bit in range(WORD_BITS))
    data_bar_x = tuple(cell_x0 + bit * BIT_PITCH + 14 for bit in range(WORD_BITS))
    read_x = tuple(cell_x0 + bit * BIT_PITCH + 16 for bit in range(WORD_BITS))

    address_specials = [dict() for _ in range(ADDRESS_BITS)]
    address_skipped = [set() for _ in range(ADDRESS_BITS)]
    read_control_specials = {}
    write_control_specials = {}
    read_control_skipped = set()
    write_control_skipped = set()

    data_specials = [dict() for _ in range(WORD_BITS)]
    data_bar_specials = [dict() for _ in range(WORD_BITS)]
    read_specials = [dict() for _ in range(WORD_BITS)]
    data_skipped = [set() for _ in range(WORD_BITS)]
    data_bar_skipped = [set() for _ in range(WORD_BITS)]
    read_skipped = [set() for _ in range(WORD_BITS)]

    for row, y in enumerate(row_ys):
        absolute_word = bank * ROWS_PER_BANK + row
        word = words.get(absolute_word, 0) & 0xFFFF
        decode_y = y + 10

        # Nine-literal equality detector for the complete 9-bit word address.
        prefix_active = True
        previous_gate_x = None
        for bit in range(ADDRESS_BITS):
            bus_x = address_x[bit]
            gate_x = bus_x + 4
            expected = (absolute_word >> bit) & 1
            literal_active = expected == 0  # reset MAR is zero

            address_specials[bit][decode_y + 1] = (WIRE2, 2, False)
            address_skipped[bit].add(decode_y)
            address_specials[bit][decode_y - 2] = (WIRE_L, 2, False)

            _put_range(board, bus_x + 1, gate_x - 1, decode_y - 2, 1)
            board.put(gate_x, decode_y - 2, WIRE, 0)
            board.put(gate_x, decode_y - 1,
                      WIRE if expected else NOT, 0, literal_active)

            stage_active = prefix_active and literal_active
            board.put(gate_x, decode_y,
                      WIRE if bit == 0 else AND, 1, stage_active)
            if previous_gate_x is not None:
                _put_range(board, previous_gate_x + 1, gate_x - 1,
                           decode_y, 1, prefix_active)
            previous_gate_x = gate_x
            prefix_active = stage_active

        # Row-select fans to separately qualified read and write gates.
        last_x = address_x[-1] + 4
        _put_range(board, last_x + 1, base + 55, decode_y, 1, prefix_active)
        board.put(base + 56, decode_y, WIRE_R, 1, prefix_active)

        _put_range(board, base + 57, base + 61, decode_y, 1, prefix_active)
        board.put(base + 62, decode_y, WIRE, 2, prefix_active)
        for yy in range(decode_y - 1, y + 6, -1):
            board.put(base + 62, yy, WIRE, 2, prefix_active)
        board.put(base + 62, y + 6, AND, 1)

        for yy in range(decode_y - 1, y + 1, -1):
            board.put(base + 56, yy, WIRE, 2, prefix_active)
        _put_range(board, base + 56, base + 61, y + 1, 1, prefix_active)
        board.put(base + 62, y + 1, WIRE, 2, prefix_active)
        board.put(base + 62, y, AND, 1)

        # Global MEM-READ and MEM-WRITE buses enter the row gates from below.
        read_control_specials[y + 7] = (WIRE2, 2, False)
        read_control_skipped.add(y + 6)
        read_control_specials[y + 4] = (WIRE_R, 2, False)
        read_control_specials[y + 1] = (WIRE2, 2, False)
        read_control_skipped.add(y)
        board.put(base + 63, y + 4, WIRE, 3)
        board.put(base + 62, y + 4, WIRE, 0)
        board.put(base + 62, y + 5, WIRE, 0)

        write_control_specials[y + 7] = (WIRE2, 2, False)
        write_control_skipped.add(y + 6)
        write_control_specials[y + 1] = (WIRE2, 2, False)
        write_control_skipped.add(y)
        write_control_specials[y - 2] = (WIRE_R, 2, False)
        board.put(base + 65, y - 2, WIRE2, 3)
        board.put(base + 63, y - 2, WIRE, 3)
        board.put(base + 62, y - 2, WIRE, 0)
        board.put(base + 62, y - 1, WIRE, 0)

        re_branches = {}
        we_branches = {}
        for bit in range(WORD_BITS):
            bx = cell_x0 + bit * BIT_PITCH
            value = (word >> bit) & 1
            _storage_cell(board, bx, y, value)
            re_branches[bx + 11] = (WIRE_R, 1)
            we_branches[bx + 5] = (WIRE_X, 1)

            data_specials[bit][y + 7] = (WIRE2, 2, False)
            data_skipped[bit].add(y + 6)
            data_specials[bit][y + 4] = (WIRE_L, 2, False)
            data_specials[bit][y + 1] = (WIRE2, 2, False)
            data_skipped[bit].add(y)

            data_bar_specials[bit][y + 7] = (WIRE2, 2, False)
            data_bar_skipped[bit].add(y + 6)
            data_bar_specials[bit][y - 4] = (WIRE_R, 2, False)
            data_bar_specials[bit][y + 1] = (WIRE2, 2, False)
            data_bar_skipped[bit].add(y)

            read_specials[bit][y - 1] = (WIRE2, 0, False)
            read_skipped[bit].add(y)
            read_specials[bit][y + 2] = (WIRE, 0, False)
            read_specials[bit][y + 5] = (WIRE2, 0, False)
            read_skipped[bit].add(y + 6)

        line_end = cell_x0 + (WORD_BITS - 1) * BIT_PITCH + 16
        _put_control_line(board, base + 63, line_end, y + 6, re_branches)
        _put_control_line(board, base + 63, line_end, y, we_branches)

    for bit in range(ADDRESS_BITS):
        _put_vertical_lane(board, address_x[bit], top, bottom, 2,
                           address_specials[bit], address_skipped[bit])
    _put_vertical_lane(board, mem_read_x, top, bottom, 2,
                       read_control_specials, read_control_skipped)
    _put_vertical_lane(board, mem_write_x, top, bottom, 2,
                       write_control_specials, write_control_skipped)

    for bit in range(WORD_BITS):
        _put_vertical_lane(board, data_x[bit], top, bottom, 2,
                           data_specials[bit], data_skipped[bit])
        _put_vertical_lane(board, data_bar_x[bit], top, bottom, 2,
                           data_bar_specials[bit], data_bar_skipped[bit])
        _put_vertical_lane(board, read_x[bit], bottom, top, 0,
                           read_specials[bit], read_skipped[bit])

    return BankPorts(
        address=address_x,
        mem_read=mem_read_x,
        mem_write=mem_write_x,
        data=data_x,
        data_bar=data_bar_x,
        read=read_x,
        top_y=top,
        bottom_y=bottom,
    )


def _route_horizontal_crossings(board, x0, x1, y, rotation, active=False):
    """Route to an existing consumer, jumping over one or two occupied cells."""
    step = 1 if x1 > x0 else -1
    assert rotation == (1 if step > 0 else 3)
    x = x0
    while x != x1:
        assert (x, y) not in board.blocks, f"route starts in occupied {(x, y)}"
        remaining = abs(x1 - x)
        chosen = None
        for length in range(min(3, remaining), 0, -1):
            landing = x + step * length
            if landing == x1 or (landing, y) not in board.blocks:
                chosen = length
                break
        assert chosen is not None
        board.put(x, y, (WIRE, WIRE2, WIRE3)[chosen - 1], rotation, active)
        x += step * chosen


def _register_rows(board, hub):
    names = ("PC", "PC_NEXT", "MAR", "I0", "I1", "I2", "A", "B")
    rows = [RegisterRow(name, 300 - i * ROW_PITCH,
                        (BANK_X0 + 62, 306 - i * ROW_PITCH),
                        (BANK_X0 + 62, 300 - i * ROW_PITCH))
            for i, name in enumerate(names)]

    data_specials = [dict() for _ in range(WORD_BITS)]
    data_bar_specials = [dict() for _ in range(WORD_BITS)]
    read_specials = [dict() for _ in range(WORD_BITS)]
    data_skipped = [set() for _ in range(WORD_BITS)]
    data_bar_skipped = [set() for _ in range(WORD_BITS)]
    read_skipped = [set() for _ in range(WORD_BITS)]

    cell_x0 = BANK_X0 + 80
    for row in rows:
        y = row.y
        board.put(*row.read_pin, WIRE, 1)
        board.put(*row.write_pin, WIRE, 1)
        re_branches = {}
        we_branches = {}
        for bit in range(WORD_BITS):
            bx = cell_x0 + bit * BIT_PITCH
            _storage_cell(board, bx, y, False)
            re_branches[bx + 11] = (WIRE_R, 1)
            we_branches[bx + 5] = (WIRE_X, 1)

            data_specials[bit][y + 7] = (WIRE2, 2, False)
            data_skipped[bit].add(y + 6)
            data_specials[bit][y + 4] = (WIRE_L, 2, False)
            data_specials[bit][y + 1] = (WIRE2, 2, False)
            data_skipped[bit].add(y)

            data_bar_specials[bit][y + 7] = (WIRE2, 2, False)
            data_bar_skipped[bit].add(y + 6)
            data_bar_specials[bit][y - 4] = (WIRE_R, 2, False)
            data_bar_specials[bit][y + 1] = (WIRE2, 2, False)
            data_bar_skipped[bit].add(y)

            read_specials[bit][y - 1] = (WIRE2, 0, False)
            read_skipped[bit].add(y)
            read_specials[bit][y + 2] = (WIRE, 0, False)
            read_specials[bit][y + 5] = (WIRE2, 0, False)
            read_skipped[bit].add(y + 6)

        line_end = cell_x0 + (WORD_BITS - 1) * BIT_PITCH + 16
        _put_control_line(board, BANK_X0 + 63, line_end, y + 6, re_branches)
        _put_control_line(board, BANK_X0 + 63, line_end, y, we_branches)

    top = 500
    bottom = hub.top_y + 1
    for bit in range(WORD_BITS):
        xd = hub.data[bit]
        xn = hub.data_bar[bit]
        xr = hub.read[bit]
        read_y = 80 + bit * 4
        data_y = -20 - bit * 4
        data_bar_y = -90 - bit * 4

        # Read-bus feedback: direct D at y=500, inverted /D at y=495.
        read_specials[bit][495] = (WIRE_L, 0, False)
        read_specials[bit][500] = (WIRE_L, 0, False)
        read_specials[bit][read_y] = (WIRE_L, 0, False)
        data_specials[bit][data_y] = (WIRE_L, 2, False)
        data_bar_specials[bit][data_bar_y] = (WIRE_L, 2, False)
        _route_horizontal_crossings(board, xr - 1, xn, 495, 3)
        data_bar_specials[bit][495] = (NOT, 2, True)
        _route_horizontal_crossings(board, xr - 1, xd, 500, 3)
        data_specials[bit][500] = (WIRE, 2, False)

        _put_vertical_lane(board, xd, top, bottom, 2,
                           data_specials[bit], data_skipped[bit])
        _put_vertical_lane(board, xn, 495, bottom, 2,
                           data_bar_specials[bit], data_bar_skipped[bit])
        _put_vertical_lane(board, xr, bottom, top, 0,
                           read_specials[bit], read_skipped[bit])

    row_by_name = {row.name: row for row in rows}
    taps = {"PC": [], "MAR": [], "A": [], "B": []}

    def replace(x, y, block_type, rotation):
        key = (x, y)
        old = board.blocks.pop(key)
        board.put(x, y, block_type, rotation, old[2])

    # MAR drives the nine physical address buses continuously.
    mar_y = row_by_name["MAR"].y
    for bit in range(ADDRESS_BITS):
        bx = cell_x0 + bit * BIT_PITCH
        lane_x = bx + 17
        target_y = 540 + bit * 4
        replace(bx + 9, mar_y + 2, WIRE_L, 1)
        board.put(bx + 9, mar_y + 3, WIRE, 1)

        lane_specials = {
            mar_y + 3: (WIRE, 0, False),
            target_y: (WIRE_T, 0, False),
        }
        lane_skipped = set()
        for row in rows:
            if mar_y < row.y < target_y:
                lane_specials[row.y - 1] = (WIRE2, 0, False)
                lane_skipped.add(row.y)
                lane_specials[row.y + 5] = (WIRE2, 0, False)
                lane_skipped.add(row.y + 6)
        _put_vertical_lane(board, lane_x, mar_y + 3, target_y, 0,
                           lane_specials, lane_skipped)
        _route_horizontal_crossings(
            board, bx + 10, lane_x, mar_y + 3, 1)
        taps["MAR"].append((lane_x, target_y))

    # Continuous Q taps used by the incrementer and subtractor.  They live in
    # the two-cell gaps between bit slices and jump over every control line.
    for name, lane_offset, target_y, upward in (
            ("PC", 20, 430, True),
            ("A", 18, 100, False),
            ("B", 20, 100, False)):
        source_y = row_by_name[name].y
        for bit in range(WORD_BITS):
            bx = cell_x0 + bit * BIT_PITCH
            lane_x = bx + lane_offset
            replace(bx + 9, source_y + 2,
                    WIRE_L if upward else WIRE_R, 1)

            if upward:
                lane_specials = {source_y + 3: (WIRE, 0, False)}
                lane_skipped = set()
                for row in rows:
                    if row.y + 6 > source_y + 3:
                        lane_specials[row.y + 5] = (WIRE2, 0, False)
                        lane_skipped.add(row.y + 6)
                _put_vertical_lane(board, lane_x, source_y + 3, target_y, 0,
                                   lane_specials, lane_skipped)
                board.put(bx + 9, source_y + 3, WIRE, 1)
                _route_horizontal_crossings(
                    board, bx + 10, lane_x, source_y + 3, 1)
            else:
                lane_specials = {source_y - 1: (WIRE, 2, False)}
                lane_skipped = set()
                for row in rows:
                    if target_y < row.y < source_y:
                        lane_specials[row.y + 7] = (WIRE2, 2, False)
                        lane_skipped.add(row.y + 6)
                        lane_specials[row.y + 1] = (WIRE2, 2, False)
                        lane_skipped.add(row.y)
                _put_vertical_lane(board, lane_x, source_y - 1, target_y, 2,
                                   lane_specials, lane_skipped)
                board.put(bx + 9, source_y + 1, WIRE2, 2)
                board.put(bx + 9, source_y - 1, WIRE, 1)
                _route_horizontal_crossings(
                    board, bx + 10, lane_x, source_y - 1, 1)
            taps[name].append((lane_x, target_y))

    return rows, taps


def _connect_banks(board, ports):
    """Merge four read buses and broadcast D and /D from bank zero."""
    for bit in range(WORD_BITS):
        read_y = 80 + bit * 4
        data_y = -20 - bit * 4
        data_bar_y = -90 - bit * 4

        # Extend non-hub bank lanes to their horizontal connection levels.
        for bank in range(1, BANKS):
            p = ports[bank]
            _put_vertical_lane(board, p.read[bit], p.top_y + 1, read_y, 0,
                               {read_y: (WIRE_L, 0, False)}, set())
            _put_vertical_lane(board, p.data[bit], data_y, p.top_y + 1, 2,
                               {data_y: (WIRE_L, 2, False)}, set())
            _put_vertical_lane(board, p.data_bar[bit], data_bar_y, p.top_y + 1, 2,
                               {data_bar_y: (WIRE_L, 2, False)}, set())

    for bit in range(WORD_BITS):
        read_y = 80 + bit * 4
        data_y = -20 - bit * 4
        data_bar_y = -90 - bit * 4
        # Read travels right-to-left; write data travels left-to-right.
        for bank in range(BANKS - 1, 0, -1):
            _route_horizontal_crossings(
                board, ports[bank].read[bit] - 1,
                ports[bank - 1].read[bit], read_y, 3)
        for bank in range(BANKS - 1):
            _route_horizontal_crossings(
                board, ports[bank].data[bit] + 1,
                ports[bank + 1].data[bit], data_y, 1)
            _route_horizontal_crossings(
                board, ports[bank].data_bar[bit] + 1,
                ports[bank + 1].data_bar[bit], data_bar_y, 1)


def _branch_bus(board, pin_x, y, branch_xs, branch_type, branch_rotation):
    """A right-going control bus crossing vertical datapath lanes."""
    board.put(pin_x, y, WIRE, 1)
    for x in branch_xs:
        board.put(x, y, branch_type, branch_rotation)
    previous = pin_x
    for x in branch_xs:
        _route_horizontal_crossings(board, previous + 1, x, y, 1)
        previous = x
    return pin_x, y


def _connect_address_buses(board, ports, mar_taps):
    for bit in range(ADDRESS_BITS):
        mar_x, hub_y = mar_taps[bit]
        for bank, port in enumerate(ports):
            special = (WIRE, 2, False) if bank == 0 else (WIRE_L, 2, False)
            _put_vertical_lane(
                board, port.address[bit], hub_y, port.top_y + 1, 2,
                {hub_y: special}, set())

        _route_horizontal_crossings(
            board, mar_x - 1, ports[0].address[bit], hub_y, 3)
        _route_horizontal_crossings(
            board, mar_x + 1, ports[1].address[bit], hub_y, 1)
        for bank in range(1, BANKS - 1):
            _route_horizontal_crossings(
                board, ports[bank].address[bit] + 1,
                ports[bank + 1].address[bit], hub_y, 1)


def _build_alu(board, taps, hub):
    cell_x0 = BANK_X0 + 80

    # --- PC + 1 -----------------------------------------------------------
    inc_carry_y = 450
    inc_sum_y = 460
    inc_enable_y = 464
    inc_branches = []
    previous_carry_gate = None

    # Constant carry-in = 1.
    board.put(cell_x0 + 2, inc_carry_y, NOT, 1, True)

    for bit in range(WORD_BITS):
        bx = cell_x0 + bit * BIT_PITCH
        pc_x, pc_y = taps["PC"][bit]
        assert pc_y == 430

        # PC fan-out to the sum and carry gates; the trunk jumps the carry bus.
        pc_specials = {
            445: (WIRE_L, 0, False),
            449: (WIRE2, 0, False),
            459: (WIRE_L, 0, False),
        }
        _put_vertical_lane(board, pc_x, 431, 461, 0,
                           pc_specials, {inc_carry_y})
        board.put(bx + 8, 445, WIRE, 0)
        board.put(bx + 8, 446, WIRE, 0)
        board.put(bx + 8, 447, WIRE, 0)
        board.put(bx + 8, 448, WIRE, 0)
        board.put(bx + 8, 449, WIRE, 0)
        _route_horizontal_crossings(board, pc_x - 1, bx + 8, 445, 3)

        board.put(bx + 8, 459, WIRE, 0)
        _route_horizontal_crossings(board, pc_x - 1, bx + 8, 459, 3)

        carry_branch_x = bx + 6
        board.put(carry_branch_x, inc_carry_y, WIRE_L, 1)
        board.put(carry_branch_x + 1, inc_carry_y, WIRE, 1)
        board.put(carry_branch_x, 451, WIRE, 0)
        board.put(carry_branch_x, 452, WIRE, 0)
        board.put(carry_branch_x, 453, WIRE, 0)
        board.put(carry_branch_x, 454, WIRE, 0)
        board.put(carry_branch_x, 455, WIRE, 0)
        board.put(carry_branch_x, 456, WIRE, 0)
        board.put(carry_branch_x, 457, WIRE, 0)
        board.put(carry_branch_x, 458, WIRE, 0)
        board.put(carry_branch_x, 459, WIRE, 0)
        board.put(carry_branch_x, 460, WIRE, 1)
        board.put(carry_branch_x + 1, 460, WIRE, 1)

        carry_gate_x = bx + 8
        board.put(carry_gate_x, inc_carry_y, AND, 1)
        board.put(carry_gate_x, inc_sum_y, 10, 1)  # XOR(PC, carry)

        if bit == 0:
            _route_horizontal_crossings(
                board, cell_x0 + 3, carry_branch_x, inc_carry_y, 1, True)
        else:
            _route_horizontal_crossings(
                board, previous_carry_gate + 1,
                carry_branch_x, inc_carry_y, 1)
        previous_carry_gate = carry_gate_x

        # Qualify the increment result and inject it into the shared read bus.
        board.put(bx + 9, inc_sum_y, WIRE, 1)
        board.put(bx + 10, inc_sum_y, WIRE, 1)
        board.put(bx + 11, inc_sum_y, WIRE, 1)
        board.put(bx + 12, inc_sum_y, AND, 1)
        board.put(bx + 13, inc_sum_y, WIRE2, 1)
        board.put(bx + 15, inc_sum_y, WIRE, 1)
        board.put(bx + 12, 461, WIRE, 2)
        board.put(bx + 12, 462, WIRE, 2)
        board.put(bx + 12, 463, WIRE, 2)
        inc_branches.append(bx + 12)

    inc_pin = _branch_bus(
        board, BANK_X0 + 62, inc_enable_y,
        inc_branches, WIRE_R, 1)

    # --- B - A ------------------------------------------------------------
    sub_carry_y = 30
    sub_sum_y = 40
    sub_enable_y = 46
    sub_zero_y = 60
    sub_branches = []
    sum_taps = []
    previous_carry_gate = None

    board.put(cell_x0 + 2, sub_carry_y, NOT, 1, True)  # +1

    for bit in range(WORD_BITS):
        bx = cell_x0 + bit * BIT_PITCH
        a_x, a_y = taps["A"][bit]
        b_x, b_y = taps["B"][bit]
        assert a_y == b_y == 100

        # A -> NOT -> /A trunk.
        _put_vertical_lane(board, a_x, 99, 91, 2, {}, set())
        board.put(a_x, 90, NOT, 2, True)
        not_a_specials = {
            41: (WIRE2, 2, False),
            39: (WIRE_R, 2, False),
            31: (WIRE2, 2, False),
            29: (WIRE_R, 2, False),
        }
        _put_vertical_lane(board, a_x, 89, 28, 2,
                           not_a_specials, {sub_sum_y, sub_carry_y})

        board.put(bx + 8, 39, WIRE, 0)
        _route_horizontal_crossings(board, a_x - 1, bx + 8, 39, 3)
        board.put(bx + 8, 29, WIRE, 0)
        _route_horizontal_crossings(board, a_x - 1, bx + 8, 29, 3)

        # B trunk and its two left-going branches.
        b_specials = {
            42: (WIRE_R, 2, False),
            41: (WIRE2, 2, False),
            32: (WIRE_R, 2, False),
            31: (WIRE2, 2, False),
        }
        _put_vertical_lane(board, b_x, 99, 28, 2,
                           b_specials, {sub_sum_y, sub_carry_y})
        board.put(bx + 8, 42, WIRE, 2)
        board.put(bx + 8, 41, WIRE, 2)
        _route_horizontal_crossings(board, b_x - 1, bx + 8, 42, 3)
        board.put(bx + 8, 32, WIRE, 2)
        board.put(bx + 8, 31, WIRE, 2)
        _route_horizontal_crossings(board, b_x - 1, bx + 8, 32, 3)

        carry_branch_x = bx + 6
        board.put(carry_branch_x, sub_carry_y, WIRE_L, 1)
        board.put(carry_branch_x + 1, sub_carry_y, WIRE, 1)
        board.put(carry_branch_x, 31, WIRE2, 0)
        board.put(carry_branch_x, 33, WIRE, 0)
        board.put(carry_branch_x, 34, WIRE, 0)
        board.put(carry_branch_x, 35, WIRE, 0)
        board.put(carry_branch_x, 36, WIRE, 0)
        board.put(carry_branch_x, 37, WIRE, 0)
        board.put(carry_branch_x, 38, WIRE2, 0)
        board.put(carry_branch_x, 40, WIRE, 1)
        board.put(carry_branch_x + 1, 40, WIRE, 1)

        carry_gate_x = bx + 8
        board.put(carry_gate_x, sub_carry_y, AND, 1)
        board.put(carry_gate_x, sub_sum_y, 10, 1)  # XOR(B, /A, carry)

        if bit == 0:
            _route_horizontal_crossings(
                board, cell_x0 + 3, carry_branch_x, sub_carry_y, 1, True)
        else:
            _route_horizontal_crossings(
                board, previous_carry_gate + 1,
                carry_branch_x, sub_carry_y, 1)
        previous_carry_gate = carry_gate_x

        board.put(bx + 9, sub_sum_y, WIRE, 1)
        board.put(bx + 10, sub_sum_y, WIRE_L, 1)  # result + zero-detect tap
        board.put(bx + 11, sub_sum_y, WIRE, 1)
        board.put(bx + 12, sub_sum_y, AND, 1)
        board.put(bx + 13, sub_sum_y, WIRE2, 1)
        board.put(bx + 15, sub_sum_y, WIRE, 1)

        # Enable enters from above, jumping the B-input trace at y=42.
        board.put(bx + 12, 45, WIRE, 2)
        board.put(bx + 12, 44, WIRE, 2)
        board.put(bx + 12, 43, WIRE2, 2)
        board.put(bx + 12, 41, WIRE, 2)
        sub_branches.append(bx + 12)

        # Every result bit contributes to a wired-OR nonzero detector.
        tap_specials = {
            41: (WIRE2, 0, False),
            43: (WIRE2, 0, False),
            sub_zero_y: (WIRE, 1, False),
        }
        if bit == WORD_BITS - 1:
            tap_specials[50] = (WIRE_R, 0, False)
        _put_vertical_lane(board, bx + 10, 41, sub_zero_y, 0,
                           tap_specials, {42, sub_enable_y})
        sum_taps.append(bx + 10)

    sub_pin = _branch_bus(
        board, BANK_X0 + 62, sub_enable_y,
        sub_branches, WIRE_R, 1)

    # Wired OR of all result bits; NOT gives zero, sign comes from bit 15.
    for x in sum_taps:
        # The vertical tap already placed the right-going cell at y=60.
        pass
    for left, right in zip(sum_taps, sum_taps[1:]):
        _route_horizontal_crossings(board, left + 1, right, sub_zero_y, 1)

    condition_x = sum_taps[-1] + 30
    board.put(condition_x, sub_zero_y, NOT, 2, True)
    _route_horizontal_crossings(
        board, sum_taps[-1] + 1, condition_x, sub_zero_y, 1)
    for y in range(sub_zero_y - 1, 50, -1):
        board.put(condition_x, y, WIRE, 2)
    board.put(condition_x, 50, WIRE, 1)  # zero -> condition OR

    # Sign bit branches from the bit-15 result tap.
    sign_x = sum_taps[-1]
    _route_horizontal_crossings(
        board, sign_x + 1, condition_x, 50, 1)
    # The merge wire normalizes sign OR zero and exposes the LEQ pin.

    return {
        "INC_ENABLE": inc_pin,
        "SUB_ENABLE": sub_pin,
        "LEQ": (condition_x, 50),
    }


def _memory_control_ports(board, ports):
    targets = {}
    for name, attr, hub_y in (
            ("MEM_RE", "mem_read", 620),
            ("MEM_WE", "mem_write", 610)):
        xs = [getattr(port, attr) for port in ports]
        for x, port in zip(xs, ports):
            _put_vertical_lane(
                board, x, hub_y, port.top_y + 1, 2,
                {hub_y: (WIRE_R, 1, False)}, set())
        for left, right in zip(xs, xs[1:]):
            _route_horizontal_crossings(board, left + 1, right, hub_y, 1)
        targets[name] = (xs[0], hub_y)
    return targets


def _snake_cycle(width, rows, x0=-600, y0=700):
    assert rows % 2 == 0
    points = []
    for row in range(rows):
        y = y0 - row * 2
        xs = range(x0, x0 + width) if row % 2 == 0 else range(
            x0 + width - 1, x0 - 1, -1)
        points.extend((x, y) for x in xs)
        if row != rows - 1:
            points.append((points[-1][0], y - 1))
    bottom_y = y0 - (rows - 1) * 2
    points.append((x0 - 1, bottom_y))
    points.extend((x0 - 1, y) for y in range(bottom_y + 1, y0 + 1))
    assert len(points) == rows * width + 3 * rows - 2
    assert len(points) == len(set(points))
    return points


def _direction(a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    return {(0, 1): 0, (1, 0): 1, (0, -1): 2, (-1, 0): 3}[(dx, dy)]


def _build_controller(board, registers, alu, memory_controls):
    rows_by_name = {row.name: row for row in registers}
    phase_controls = {
        0: ("PC_RE", "MAR_WE"),
        1: ("MEM_RE", "I0_WE"),
        2: ("INC_ENABLE", "PC_NEXT_WE"),
        3: ("PC_NEXT_RE", "PC_WE"),
        4: ("PC_RE", "MAR_WE"),
        5: ("MEM_RE", "I1_WE"),
        6: ("INC_ENABLE", "PC_NEXT_WE"),
        7: ("PC_NEXT_RE", "PC_WE"),
        8: ("PC_RE", "MAR_WE"),
        9: ("MEM_RE", "I2_WE"),
        10: ("INC_ENABLE", "PC_NEXT_WE"),
        11: ("PC_NEXT_RE", "PC_WE"),
        12: ("I0_RE", "MAR_WE"),
        13: ("MEM_RE", "A_WE"),
        14: ("I1_RE", "MAR_WE"),
        15: ("MEM_RE", "B_WE"),
        16: ("SUB_ENABLE", "MEM_WE"),
        17: ("I2_RE", "BRANCH"),
    }
    destinations = {
        "PC_RE": rows_by_name["PC"].read_pin,
        "MAR_WE": rows_by_name["MAR"].write_pin,
        "MEM_RE": memory_controls["MEM_RE"],
        "I0_WE": rows_by_name["I0"].write_pin,
        "INC_ENABLE": alu["INC_ENABLE"],
        "PC_NEXT_WE": rows_by_name["PC_NEXT"].write_pin,
        "PC_NEXT_RE": rows_by_name["PC_NEXT"].read_pin,
        "PC_WE": rows_by_name["PC"].write_pin,
        "I1_WE": rows_by_name["I1"].write_pin,
        "I2_WE": rows_by_name["I2"].write_pin,
        "I0_RE": rows_by_name["I0"].read_pin,
        "A_WE": rows_by_name["A"].write_pin,
        "I1_RE": rows_by_name["I1"].read_pin,
        "B_WE": rows_by_name["B"].write_pin,
        "SUB_ENABLE": alu["SUB_ENABLE"],
        "MEM_WE": memory_controls["MEM_WE"],
        "I2_RE": rows_by_name["I2"].read_pin,
    }
    control_names = tuple(destinations) + ("BRANCH",)

    edges = []
    for phase in range(18):
        for control in phase_controls[phase]:
            edges.append((phase, control))
    edge_y = {
        edge: 860 + index * 3
        for index, edge in enumerate(edges)
    }
    matrix_top = max(edge_y.values()) + 10

    # Control collectors are persistent wired-OR trunks.  BRANCH lives to the
    # right of the ALU so its conditional gate can remain local to LEQ.
    condition_x, condition_y = alu["LEQ"]
    branch_gate_x = condition_x + 12
    collector_x = {
        name: (branch_gate_x if name == "BRANCH" else 80 + i * 6)
        for i, name in enumerate(control_names)
    }

    for name in control_names:
        x = collector_x[name]
        collector_specials = {
            edge_y[(phase, control)]: (WIRE, 2, False)
            for phase, control in edges if control == name
        }
        if name == "BRANCH":
            _put_vertical_lane(
                board, x, matrix_top, condition_y + 1, 2,
                collector_specials, set())
            continue
        target_y = destinations[name][1]
        collector_specials[target_y] = (WIRE_L, 2, False)
        _put_vertical_lane(
            board, x, matrix_top, target_y, 2,
            collector_specials, set())

    # A circulating 3500-cell high window visits eighteen phase taps.  The
    # 1424-cell dead band absorbs unequal route lengths and prevents overlap.
    cycle = _snake_cycle(484, 182)
    assert len(cycle) == 88632 and len(cycle) % 18 == 0
    phase_spacing = len(cycle) // 18
    high_width = 3500
    tap_indices = []
    for phase in range(18):
        index = phase * phase_spacing
        while cycle[index][1] != cycle[(index + 1) % len(cycle)][1]:
            index += 1
        tap_indices.append(index)
    tap_by_index = {index: phase for phase, index in enumerate(tap_indices)}

    def initially_active(index):
        return index == tap_indices[0] or index >= len(cycle) - high_width + 1

    for index, point in enumerate(cycle):
        nxt = cycle[(index + 1) % len(cycle)]
        rotation = _direction(point, nxt)
        if index in tap_by_index:
            assert rotation in (1, 3)
            block_type = WIRE_L if rotation == 1 else WIRE_R
        else:
            block_type = WIRE
        board.put(point[0], point[1], block_type, rotation,
                  initially_active(index))

    # Every phase has an upward distributor.  Its two matrix branches feed
    # the corresponding control collectors.
    phase_columns = {}
    for phase, index in enumerate(tap_indices):
        tap_x, tap_y = cycle[index]
        gap_y = tap_y + 1
        phase_x = -90 + phase * 4
        phase_columns[phase] = phase_x
        specials = {
            edge_y[(phase, control)]: (WIRE_R, 0, False)
            for control in phase_controls[phase]
        }
        _put_vertical_lane(board, phase_x, gap_y, matrix_top, 0,
                           specials, set())
        _route_horizontal_crossings(board, tap_x, phase_x, gap_y, 1,
                                    initially_active(index))

    # Matrix edges: each is a single horizontal connection; all unrelated
    # vertical signals are crossed with Wire 2/3 jumpers.
    for phase, control in edges:
        y = edge_y[(phase, control)]
        _route_horizontal_crossings(
            board, phase_columns[phase] + 1,
            collector_x[control], y, 1)

    # Route ordinary collector outputs into register, ALU and memory pins.
    for name, target in destinations.items():
        x = collector_x[name]
        if name == "I2_RE":
            # Conditional PC writes pass through the LEQ gate and a long
            # return route. Delay I2's read-enable by roughly the same amount
            # so its data remains valid until PC.WE has closed.
            turn_right = 650
            turn_left = 200
            board.put(turn_right, target[1], WIRE_L, 1)
            board.put(turn_right, target[1] + 1, WIRE, 0)
            board.put(turn_right, target[1] + 2, WIRE_R, 3)
            board.put(turn_left, target[1] + 2, WIRE_R, 3)
            board.put(turn_left, target[1] + 3, WIRE, 0)
            board.put(turn_left, target[1] + 4, WIRE_R, 0)
            board.put(target[0], target[1] + 4, WIRE_R, 1)
            for y in range(target[1] + 3, target[1], -1):
                board.put(target[0], y, WIRE, 2)
            _route_horizontal_crossings(
                board, x + 1, turn_right, target[1], 1)
            _route_horizontal_crossings(
                board, turn_right - 1, turn_left, target[1] + 2, 3)
            _route_horizontal_crossings(
                board, turn_left + 1, target[0], target[1] + 4, 1)
            continue
        _route_horizontal_crossings(board, x + 1, target[0], target[1], 1)

    # Conditional branch: phase 17 AND LEQ.  The result approaches PC.WE
    # from below, while normal PC writes enter the same normalizer from left.
    board.put(branch_gate_x, condition_y, AND, 1)
    _route_horizontal_crossings(
        board, condition_x + 1, branch_gate_x, condition_y, 1)

    branch_out_x = branch_gate_x + 1
    branch_route_y = rows_by_name["PC"].y - 4
    branch_specials = {
        condition_y: (WIRE, 0, False),
        branch_route_y: (WIRE_L, 0, False),
    }
    _put_vertical_lane(
        board, branch_out_x, condition_y, branch_route_y, 0,
        branch_specials, set())
    board.put(rows_by_name["PC"].write_pin[0], branch_route_y, WIRE, 0)
    board.put(rows_by_name["PC"].write_pin[0], branch_route_y + 1, WIRE, 0)
    board.put(rows_by_name["PC"].write_pin[0], branch_route_y + 2, WIRE, 0)
    board.put(rows_by_name["PC"].write_pin[0], branch_route_y + 3, WIRE, 0)
    _route_horizontal_crossings(
        board, branch_out_x - 1,
        rows_by_name["PC"].write_pin[0], branch_route_y, 3)

    return {
        "phase_spacing": phase_spacing,
        "high_width": high_width,
        "tap_indices": tuple(tap_indices),
    }


def demo_memory():
    # SUBLEQ ONE, COUNTER, DONE
    #        ZERO, ZERO, 0
    # DONE:  ZERO, ZERO, DONE
    return {
        0: 100,
        1: 101,
        2: 6,
        3: 102,
        4: 102,
        5: 0,
        6: 102,
        7: 102,
        8: 6,
        100: 1,
        101: 5,
        102: 0,
    }


def cpu16():
    board = Board()
    words = demo_memory()
    ports = [_build_bank(board, bank, words) for bank in range(BANKS)]
    registers, taps = _register_rows(board, ports[0])
    _connect_banks(board, ports)
    _connect_address_buses(board, ports, taps["MAR"])
    alu = _build_alu(board, taps, ports[0])
    memory_controls = _memory_control_ports(board, ports)
    controller = _build_controller(board, registers, alu, memory_controls)

    # Temporary capacity/status lamps above each bank.  They also make it
    # immediately obvious in the editor that all four 128-word banks exist.
    for bank, port in enumerate(ports):
        board.put(BANK_X0 + bank * BANK_PITCH + 180, 20, LAMP)

    assert WORDS == 512 and BYTES == 1024
    save(board, "cpu16-1k", 1300, -1300, zoom=8.0, timestamp=0)
    return board, ports, registers, taps, alu, controller


if __name__ == "__main__":
    board, _, _, _, _, _ = cpu16()
    print(f"cpu16-1k: {len(board.blocks)} blocks, {WORDS} x {WORD_BITS} bits")
