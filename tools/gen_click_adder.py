"""Generate an interactive 8-bit click-input adder.

Everything in the resulting scheme is made from the ordinary game blocks:
button pulses, NAND latches, ripple-carry ALUs, decimal decoders and lamps.
"""

from dataclasses import dataclass

from gen_examples import AND, BUTTON, LAMP, NAND, NOT, WIRE, WIRE_L, WIRE_R
from gen_examples import WIRE_T, WIRE_X
from gen_examples import WIRE2, WIRE3, XOR, Board, save
from gen_cpu16 import (
    BIT_PITCH,
    BANK_X0,
    _branch_bus,
    _put_control_line,
    _put_range,
    _put_vertical_lane,
    _route_horizontal_crossings,
    _storage_cell,
)


BITS = 16
INPUT_BITS = 8
CELL_X0 = BANK_X0 + 80
BUS_PIN_X = BANK_X0 + 62
BUS_TOP = 780
BUS_BOTTOM = 180

INC_A_CARRY, INC_A_SUM, INC_A_ENABLE = 620, 630, 636
INC_B_CARRY, INC_B_SUM, INC_B_ENABLE = 650, 660, 666
ADD_CARRY, ADD_SUM, ADD_ENABLE = 690, 700, 706


@dataclass(frozen=True)
class Register:
    name: str
    y: int
    bits: int
    read_pin: tuple
    write_pin: tuple


def _replace(board, x, y, block_type, rotation):
    old = board.blocks.pop((x, y))
    board.put(x, y, block_type, rotation, old[2])


def _build_register_bank(board):
    rows = (
        Register("A", 300, 8, (BUS_PIN_X, 306), (BUS_PIN_X, 300)),
        Register("A_NEXT", 280, 8, (BUS_PIN_X, 286), (BUS_PIN_X, 280)),
        Register("B", 260, 8, (BUS_PIN_X, 266), (BUS_PIN_X, 260)),
        Register("B_NEXT", 240, 8, (BUS_PIN_X, 246), (BUS_PIN_X, 240)),
        Register("RESULT", 220, 16, (BUS_PIN_X, 226), (BUS_PIN_X, 220)),
    )

    data_specials = [dict() for _ in range(BITS)]
    bar_specials = [dict() for _ in range(BITS)]
    read_specials = [dict() for _ in range(BITS)]
    data_skipped = [set() for _ in range(BITS)]
    bar_skipped = [set() for _ in range(BITS)]
    read_skipped = [set() for _ in range(BITS)]

    for row in rows:
        board.put(*row.read_pin, WIRE, 1)
        board.put(*row.write_pin, WIRE, 1)
        re_branches = {}
        we_branches = {}
        for bit in range(row.bits):
            bx = CELL_X0 + bit * BIT_PITCH
            _storage_cell(board, bx, row.y, False)
            re_branches[bx + 11] = (WIRE_R, 1)
            we_branches[bx + 5] = (WIRE_X, 1)

            data_specials[bit][row.y + 7] = (WIRE2, 2, False)
            data_skipped[bit].add(row.y + 6)
            data_specials[bit][row.y + 4] = (WIRE_L, 2, False)
            data_specials[bit][row.y + 1] = (WIRE2, 2, False)
            data_skipped[bit].add(row.y)

            bar_specials[bit][row.y + 7] = (WIRE2, 2, False)
            bar_skipped[bit].add(row.y + 6)
            bar_specials[bit][row.y - 4] = (WIRE_R, 2, False)
            bar_specials[bit][row.y + 1] = (WIRE2, 2, False)
            bar_skipped[bit].add(row.y)

            read_specials[bit][row.y - 1] = (WIRE2, 0, False)
            read_skipped[bit].add(row.y)
            read_specials[bit][row.y + 2] = (WIRE, 0, False)
            read_specials[bit][row.y + 5] = (WIRE2, 0, False)
            read_skipped[bit].add(row.y + 6)

        line_end = CELL_X0 + (row.bits - 1) * BIT_PITCH + 16
        _put_control_line(board, BUS_PIN_X + 1, line_end, row.y + 6,
                          re_branches)
        _put_control_line(board, BUS_PIN_X + 1, line_end, row.y,
                          we_branches)

    for bit in range(BITS):
        bx = CELL_X0 + bit * BIT_PITCH
        xd, xn, xr = bx + 1, bx + 14, bx + 16
        read_specials[bit][BUS_TOP - 5] = (WIRE_L, 0, False)
        read_specials[bit][BUS_TOP] = (WIRE_L, 0, False)
        for output_y in (INC_A_SUM, INC_B_SUM, ADD_SUM):
            read_specials[bit][output_y] = (WIRE, 0, False)
        data_specials[bit][BUS_TOP] = (WIRE, 2, False)
        bar_specials[bit][BUS_TOP - 5] = (NOT, 2, True)

        _route_horizontal_crossings(board, xr - 1, xn, BUS_TOP - 5, 3)
        _route_horizontal_crossings(board, xr - 1, xd, BUS_TOP, 3)
        _put_vertical_lane(board, xd, BUS_TOP, BUS_BOTTOM, 2,
                           data_specials[bit], data_skipped[bit])
        _put_vertical_lane(board, xn, BUS_TOP - 5, BUS_BOTTOM, 2,
                           bar_specials[bit], bar_skipped[bit])
        _put_vertical_lane(board, xr, BUS_BOTTOM, BUS_TOP, 0,
                           read_specials[bit], read_skipped[bit])

    row_by_name = {row.name: row for row in rows}
    branch_rows = {
        "A": (INC_A_CARRY - 1, INC_A_SUM - 1,
              ADD_CARRY + 1, ADD_SUM + 1),
        "B": (INC_B_CARRY - 1, INC_B_SUM - 1,
              ADD_CARRY - 1, ADD_SUM - 1),
        "RESULT": (),
    }
    offsets = {"A": 18, "B": 19, "RESULT": 20}
    taps = {}

    for name in ("A", "B", "RESULT"):
        row = row_by_name[name]
        count = 9 if name == "RESULT" else 8
        taps[name] = []
        for bit in range(count):
            bx = CELL_X0 + bit * BIT_PITCH
            lane_x = bx + offsets[name]
            top_y = 820 + bit * 3 + (0 if name == "A" else
                                     30 if name == "B" else 60)
            _replace(board, bx + 9, row.y + 2, WIRE_L, 1)
            board.put(bx + 9, row.y + 3, WIRE, 1)
            _route_horizontal_crossings(
                board, bx + 10, lane_x, row.y + 3, 1)

            specials = {row.y + 3: (WIRE, 0, False),
                        top_y: (WIRE, 0, False)}
            skipped = set()
            for other in rows:
                if row.y < other.y < top_y:
                    specials[other.y - 1] = (WIRE2, 0, False)
                    skipped.add(other.y)
                    specials[other.y + 5] = (WIRE2, 0, False)
                    skipped.add(other.y + 6)
            for branch_y in branch_rows[name]:
                specials[branch_y] = (WIRE_L, 0, False)
            _put_vertical_lane(board, lane_x, row.y + 3, top_y, 0,
                               specials, skipped)
            taps[name].append((lane_x, top_y))

    return rows, taps


def _build_incrementer(board, source_taps, carry_y, sum_y, enable_y):
    branches = []
    previous_gate = None
    board.put(CELL_X0 + 2, carry_y, NOT, 1, True)

    for bit in range(INPUT_BITS):
        bx = CELL_X0 + bit * BIT_PITCH
        gate_x = bx + 8
        source_x = source_taps[bit][0]

        board.put(gate_x, carry_y - 1, WIRE, 0)
        board.put(gate_x, sum_y - 1, WIRE, 0)
        _route_horizontal_crossings(board, source_x - 1, gate_x,
                                    carry_y - 1, 3)
        _route_horizontal_crossings(board, source_x - 1, gate_x,
                                    sum_y - 1, 3)
        board.put(gate_x, carry_y, AND, 1)
        board.put(gate_x, sum_y, XOR, 1)

        branch_x = bx + 5
        board.put(branch_x, carry_y, WIRE_L, 1)
        for y in range(carry_y + 1, sum_y):
            board.put(branch_x, y, WIRE, 0)
        board.put(branch_x, sum_y, WIRE, 1)
        _route_horizontal_crossings(board, branch_x + 1, gate_x,
                                    carry_y, 1)
        _route_horizontal_crossings(board, branch_x + 1, gate_x,
                                    sum_y, 1)

        if bit == 0:
            _route_horizontal_crossings(
                board, CELL_X0 + 3, branch_x, carry_y, 1, True)
        else:
            _route_horizontal_crossings(
                board, previous_gate + 1, branch_x, carry_y, 1)
        previous_gate = gate_x

        board.put(bx + 9, sum_y, WIRE, 1)
        board.put(bx + 10, sum_y, WIRE, 1)
        board.put(bx + 11, sum_y, WIRE, 1)
        board.put(bx + 12, sum_y, AND, 1)
        board.put(bx + 13, sum_y, WIRE2, 1)
        board.put(bx + 15, sum_y, WIRE, 1)
        for y in range(sum_y + 1, enable_y):
            board.put(bx + 12, y, WIRE, 2)
        branches.append(bx + 12)

    return _control_pin(board, enable_y, branches)


def _control_pin(board, y, branches):
    return _branch_bus(board, BUS_PIN_X, y, branches, WIRE_R, 1)


def _build_adder(board, a_taps, b_taps):
    branches = []
    previous_gate = None
    board.put(CELL_X0 + 2, ADD_CARRY, WIRE, 1)  # inactive carry-in

    for bit in range(9):
        bx = CELL_X0 + bit * BIT_PITCH
        gate_x = bx + 8
        board.put(gate_x, ADD_CARRY, AND, 1)
        board.put(gate_x, ADD_SUM, XOR, 1)

        if bit < 8:
            ax = a_taps[bit][0]
            bx_tap = b_taps[bit][0]
            board.put(gate_x, ADD_CARRY + 1, WIRE, 2)
            board.put(gate_x, ADD_SUM + 1, WIRE, 2)
            board.put(gate_x, ADD_CARRY - 1, WIRE, 0)
            board.put(gate_x, ADD_SUM - 1, WIRE, 0)
            _route_horizontal_crossings(
                board, ax - 1, gate_x, ADD_CARRY + 1, 3)
            _route_horizontal_crossings(
                board, ax - 1, gate_x, ADD_SUM + 1, 3)
            _route_horizontal_crossings(
                board, bx_tap - 1, gate_x, ADD_CARRY - 1, 3)
            _route_horizontal_crossings(
                board, bx_tap - 1, gate_x, ADD_SUM - 1, 3)

        branch_x = bx + 5
        board.put(branch_x, ADD_CARRY, WIRE_L, 1)
        for y in range(ADD_CARRY + 1, ADD_SUM):
            board.put(branch_x, y, WIRE, 0)
        board.put(branch_x, ADD_SUM, WIRE, 1)
        _route_horizontal_crossings(
            board, branch_x + 1, gate_x, ADD_CARRY, 1)
        _route_horizontal_crossings(
            board, branch_x + 1, gate_x, ADD_SUM, 1)
        if bit == 0:
            _route_horizontal_crossings(
                board, CELL_X0 + 3, branch_x, ADD_CARRY, 1)
        else:
            _route_horizontal_crossings(
                board, previous_gate + 1, branch_x, ADD_CARRY, 1)
        previous_gate = gate_x

        board.put(bx + 9, ADD_SUM, WIRE, 1)
        board.put(bx + 10, ADD_SUM, WIRE, 1)
        board.put(bx + 11, ADD_SUM, WIRE, 1)
        board.put(bx + 12, ADD_SUM, AND, 1)
        board.put(bx + 13, ADD_SUM, WIRE2, 1)
        board.put(bx + 15, ADD_SUM, WIRE, 1)
        for y in range(ADD_SUM + 1, ADD_ENABLE):
            board.put(bx + 12, y, WIRE, 2)
        branches.append(bx + 12)

    return _control_pin(board, ADD_ENABLE, branches)


def _window_latch(board, x, center_y):
    # Active-low S/R NAND latch, initially reset (Q=0, /Q=1).
    board.put(x, center_y + 2, NOT, 1, True)
    board.put(x + 1, center_y + 2, WIRE, 1, True)
    board.put(x + 2, center_y + 2, NAND, 1, False)
    board.put(x + 2, center_y - 2, NAND, 0, True)
    board.put(x + 3, center_y + 2, WIRE_R, 1, False)
    board.put(x + 3, center_y + 1, WIRE2, 2, False)
    board.put(x + 3, center_y - 1, WIRE, 2, False)
    board.put(x + 3, center_y - 2, WIRE, 3, False)
    board.put(x + 2, center_y - 1, WIRE2, 0, True)
    board.put(x + 2, center_y + 1, WIRE, 0, True)
    board.put(x + 2, center_y - 4, NOT, 0, True)
    board.put(x + 2, center_y - 3, WIRE, 0, True)
    return (x, center_y + 2), (x + 2, center_y - 4), (x + 3, center_y + 2)


def _vertical_to(board, x, y0, y1, final_rotation=0, specials=None):
    specials = dict(specials or {})
    specials[y1] = (WIRE, final_rotation, False)
    _put_vertical_lane(board, x, y0, y1, 0, specials, set())


def _build_two_phase_button(board, y):
    start, middle, end = 0, -1000, -2000
    button = (2, y)
    board.put(*button, BUTTON, 3)
    board.put(1, y, WIRE, 3)
    for x in range(start, end - 1, -1):
        block_type = WIRE_R if x in (start, middle, end) else WIRE
        board.put(x, y, block_type, 3)

    center1 = y + 12
    center2 = y + 22
    set1, reset1, q1 = _window_latch(board, start, center1)
    set2, reset2, q2 = _window_latch(board, middle, center2)

    _vertical_to(board, start, y + 1, set1[1] - 1)
    branch_y = reset1[1]
    _vertical_to(board, middle, y + 1, set2[1] - 1,
                 specials={branch_y: (WIRE_R, 0, False)})
    _route_horizontal_crossings(
        board, middle + 1, reset1[0], branch_y, 1)
    _vertical_to(board, end, y + 1, reset2[1])
    _route_horizontal_crossings(
        board, end + 1, reset2[0], reset2[1], 1)
    return button, q1, q2


def _build_one_phase_button(board, y):
    start, end = 0, -1000
    button = (2, y)
    board.put(*button, BUTTON, 3)
    board.put(1, y, WIRE, 3)
    for x in range(start, end - 1, -1):
        board.put(x, y, WIRE_R if x in (start, end) else WIRE, 3)
    center = y + 12
    set_pin, reset_pin, q = _window_latch(board, start, center)
    _vertical_to(board, start, y + 1, set_pin[1] - 1)
    _vertical_to(board, end, y + 1, reset_pin[1])
    _route_horizontal_crossings(
        board, end + 1, reset_pin[0], reset_pin[1], 1)
    return button, q


def _connect_windows(board, windows):
    # First place every persistent collector so horizontal fanout can jump
    # over unrelated controls cleanly.
    for source, collector_x, targets in windows:
        source_x, source_y = source
        specials = {
            y: (WIRE_L, 2, False)
            for _, y in targets
        }
        specials[source_y] = (WIRE_R, 1, False)
        _put_vertical_lane(board, collector_x, source_y,
                           min(y for _, y in targets), 2,
                           specials, set())

    for source, collector_x, targets in windows:
        source_x, source_y = source
        _route_horizontal_crossings(
            board, source_x + 1, collector_x, source_y, 1)
        for target_x, target_y in targets:
            _route_horizontal_crossings(
                board, collector_x + 1, target_x, target_y, 1)


def _build_pulse_button(board, base_x, y, controls, front_button):
    """Route one 32-tick button pulse through timed control taps."""
    button_x, button_y, trunk_y = front_button
    button = (button_x, button_y)
    pad_left = button_x - 10
    pad_right = button_x + 9
    pad_rows = 5

    # A 20 x 5 pad is large enough to click while all nine display digits are
    # on screen. Every red cell is a real Button.
    for row in range(pad_rows):
        row_y = button_y + row * 3
        bus_y = row_y - 2
        for x in range(pad_left, pad_right + 1):
            board.put(x, row_y, BUTTON, 3)
            board.put(x, row_y - 1, WIRE, 2)
            board.put(x, bus_y, WIRE, 3)

    # All five rows merge into a local downward collector. Separate trunk
    # heights let the three pads sit directly beside one another.
    collector_x = pad_left - 1
    top_bus_y = button_y + (pad_rows - 1) * 3 - 2
    _put_vertical_lane(
        board, collector_x, top_bus_y, trunk_y, 2,
        {trunk_y: (WIRE, 3, False)}, set())
    board.put(base_x + 2, trunk_y, WIRE_L, 3)
    _route_horizontal_crossings(
        board, collector_x - 1, base_x + 2, trunk_y, 3)
    _put_vertical_lane(
        board, base_x + 2, trunk_y - 1, y + 1, 2,
        {y + 1: (WIRE, 2, False)}, set())

    for x in range(base_x + 1, base_x + 3):
        board.put(x, y, WIRE, 3)
    taps = {base_x - delay for delay, _ in controls}
    end_x = min(taps)
    for x in range(base_x, end_x - 1, -1):
        board.put(x, y, WIRE_L if x in taps else WIRE, 3)

    routes = []
    for delay, target in controls:
        tap_x = base_x - delay
        target_x, target_y = target
        _put_vertical_lane(
            board, tap_x, y - 1, target_y, 2,
            {target_y: (WIRE_L, 2, False)}, set())
        routes.append((tap_x + 1, target_x, target_y))
    return button, routes


def _connect_pulse_routes(board, routes):
    for start_x, target_x, target_y in routes:
        _route_horizontal_crossings(
            board, start_x, target_x, target_y, 1)


SEGMENTS = {
    0: "abcdef",
    1: "bc",
    2: "abdeg",
    3: "abcdg",
    4: "bcfg",
    5: "acdfg",
    6: "acdefg",
    7: "abc",
    8: "abcdefg",
    9: "abcdfg",
}
# Decoder output-bus offsets for a, b, c, d, e, f, g.
SEGMENT_X = (12, 33, 36, 18, 6, 3, 15)


def _decimal_segments(value):
    enabled = set()
    for digit, char in enumerate(f"{value:03d}"):
        for segment in SEGMENTS[int(char)]:
            enabled.add(digit * 7 + ord(segment) - ord("a"))
    return enabled


def _build_decimal_decoder(board, source_taps, bit_count, max_value,
                           base_x, route_y0):
    """Build a binary-to-three-decimal-digit ROM and seven-segment lamps.

    Each ROM row is an equality detector.  Its selected segment bits enter
    upward OR chains made from ordinary threshold-AND gates plus a local
    constant input.  Nothing here relies on a special display or arithmetic
    block: the lamps are driven entirely by the game's gates and wires.
    """
    row_top = 1000
    row_pitch = 8
    row_ys = [row_top - value * row_pitch
              for value in range(max_value + 1)]
    address_x = tuple(base_x + bit * 6 for bit in range(bit_count))
    display_origin = base_x + 70
    digit_pitch = 45
    display_y = 1120
    segment_x = tuple(
        display_origin + digit * digit_pitch + SEGMENT_X[segment]
        for digit in range(3) for segment in range(7)
    )
    segment_midpoints = []
    midpoint_offsets_x = (12, 30, 30, 18, 9, 9, 15)
    midpoint_offsets_y = (30, 22, 7, 0, 7, 22, 15)
    for index in range(21):
        digit = index // 7
        segment = index % 7
        segment_midpoints.append((
            display_origin + digit * digit_pitch +
            midpoint_offsets_x[segment],
            display_y + midpoint_offsets_y[segment],
        ))

    address_specials = [dict() for _ in range(bit_count)]
    address_skipped = [set() for _ in range(bit_count)]
    output_specials = [dict() for _ in range(21)]
    output_skipped = [set() for _ in range(21)]

    for value, y in enumerate(row_ys):
        prefix_active = True
        previous_gate_x = None
        for bit in range(bit_count):
            bus_x = address_x[bit]
            gate_x = bus_x + 4
            expected = (value >> bit) & 1
            literal_active = expected == 0

            address_specials[bit][y + 1] = (WIRE2, 2, False)
            address_skipped[bit].add(y)
            address_specials[bit][y - 2] = (WIRE_L, 2, False)

            _put_range(board, bus_x + 1, gate_x - 1, y - 2, 1)
            board.put(gate_x, y - 2, WIRE, 0)
            board.put(gate_x, y - 1,
                      WIRE if expected else NOT, 0, literal_active)

            stage_active = prefix_active and literal_active
            board.put(gate_x, y,
                      WIRE if bit == 0 else AND, 1, stage_active)
            if previous_gate_x is not None:
                _put_range(board, previous_gate_x + 1, gate_x - 1,
                           y, 1, prefix_active)
            previous_gate_x = gate_x
            prefix_active = stage_active

        selected = prefix_active
        selected_segments = _decimal_segments(value)
        select_x = address_x[-1] + 7
        _put_range(board, previous_gate_x + 1, select_x - 1,
                   y, 1, selected)
        board.put(select_x, y, WIRE_R, 1, selected)
        board.put(select_x, y - 1, WIRE, 2, selected)
        board.put(select_x, y - 2, WIRE, 1, selected)

        branches = {}
        for segment, bus_x in enumerate(segment_x):
            segment_active = selected and segment in selected_segments
            board.put(bus_x - 1, y, NOT, 1, True)
            board.put(bus_x + 1, y, WIRE, 3, segment_active)
            board.put(bus_x + 1, y - 1, WIRE, 0, segment_active)
            if segment in selected_segments:
                branches[bus_x + 1] = (WIRE_L, 1)

            # The AND has one permanent input.  The second input is either
            # this row's selection or an already-active segment below it,
            # so the vertical chain behaves as an OR.
            output_specials[segment][y] = (
                AND, 0, segment_active)
            output_specials[segment][y - 3] = (
                WIRE2, 0, False)
            output_skipped[segment].add(y - 2)

        _put_control_line(board, select_x + 1,
                          max(segment_x) + 2, y - 2,
                          branches, selected)

    bottom = row_ys[-1]
    for bit, tap in enumerate(source_taps[:bit_count]):
        source_x, source_y = tap
        route_y = route_y0 + bit * 3
        _put_vertical_lane(
            board, source_x, source_y + 1, route_y, 0,
            {route_y: (WIRE_R, 0, False)}, set())
        _route_horizontal_crossings(
            board, source_x + 1, address_x[bit], route_y, 1)
        _put_vertical_lane(
            board, address_x[bit], route_y, bottom - 3, 2,
            address_specials[bit], address_skipped[bit])

    zero_segments = _decimal_segments(0)
    for index, bus_x in enumerate(segment_x):
        digit = index // 7
        segment = index % 7
        origin = display_origin + digit * digit_pitch
        x_left, x_right = origin + 9, origin + 30
        y_bottom, y_middle, y_top = (
            display_y, display_y + 15, display_y + 30)
        midpoint_x, midpoint_y = segment_midpoints[index]
        initially_on = index in zero_segments

        if segment in (0, 3, 6):
            output_specials[index][midpoint_y - 1] = (
                WIRE, 0, initially_on)
            lane_end = midpoint_y - 1
        else:
            direction = 3 if segment in (1, 2) else 1
            output_specials[index][midpoint_y] = (
                WIRE, direction, initially_on)
            lane_end = midpoint_y

        _put_vertical_lane(
            board, bus_x, bottom, lane_end, 0,
            output_specials[index], output_skipped[index])

        if segment in (0, 3, 6):
            board.put(midpoint_x, midpoint_y,
                      WIRE_T, 0, initially_on)
            for x in range(midpoint_x - 1, x_left, -1):
                board.put(x, midpoint_y, WIRE, 3, initially_on)
            for x in range(midpoint_x + 1, x_right):
                board.put(x, midpoint_y, WIRE, 1, initially_on)
        else:
            segment_x_pos = x_right if segment in (1, 2) else x_left
            if bus_x < segment_x_pos:
                _put_range(board, bus_x + 1, segment_x_pos - 1,
                           midpoint_y, 1, initially_on)
            else:
                _put_range(board, bus_x - 1, segment_x_pos + 1,
                           midpoint_y, 3, initially_on)
            board.put(segment_x_pos, midpoint_y,
                      WIRE_T, 1, initially_on)
            low_y, high_y = (
                (y_middle, y_top) if segment in (1, 5)
                else (y_bottom, y_middle)
            )
            for yy in range(midpoint_y + 1, high_y):
                board.put(segment_x_pos, yy, WIRE, 0, initially_on)
            for yy in range(midpoint_y - 1, low_y, -1):
                board.put(segment_x_pos, yy, WIRE, 2, initially_on)

    # Shared corner lamps remove every visual gap while remaining electrical
    # sinks, so adjacent segments cannot back-feed one another.
    for digit in range(3):
        origin = display_origin + digit * digit_pitch
        x_left, x_right = origin + 9, origin + 30
        y_bottom, y_middle, y_top = (
            display_y, display_y + 15, display_y + 30)
        adjacent = (
            (0, 5), (0, 1), (4, 5, 6),
            (1, 2, 6), (3, 4), (2, 3),
        )
        corners = (
            (x_left, y_top), (x_right, y_top),
            (x_left, y_middle), (x_right, y_middle),
            (x_left, y_bottom), (x_right, y_bottom),
        )
        for corner, segments in zip(corners, adjacent):
            board.put(*corner, LAMP, 0,
                      any(digit * 7 + segment in zero_segments
                          for segment in segments))

    return tuple(segment_midpoints)


def _rows_by_name(rows):
    return {row.name: row for row in rows}


def click_adder():
    board = Board()
    rows, taps = _build_register_bank(board)
    by_name = _rows_by_name(rows)
    inc_a = _build_incrementer(
        board, taps["A"], INC_A_CARRY, INC_A_SUM, INC_A_ENABLE)
    inc_b = _build_incrementer(
        board, taps["B"], INC_B_CARRY, INC_B_SUM, INC_B_ENABLE)
    add = _build_adder(board, taps["A"], taps["B"])

    button_a, routes_a = _build_pulse_button(board, 0, 1000, (
        (0, inc_a),
        (80, by_name["A_NEXT"].write_pin),
        (500, by_name["A_NEXT"].read_pin),
        (755, by_name["A"].write_pin),
    ), (1540, 1400, 1338))
    button_b, routes_b = _build_pulse_button(board, -200, 1030, (
        (0, inc_b),
        (70, by_name["B_NEXT"].write_pin),
        (500, by_name["B_NEXT"].read_pin),
        (783, by_name["B"].write_pin),
    ), (1570, 1400, 1342))
    button_sum, routes_sum = _build_pulse_button(board, 200, 1060, (
        (0, add),
        (50, by_name["RESULT"].write_pin),
    ), (1600, 1400, 1346))
    _connect_pulse_routes(board, routes_a + routes_b + routes_sum)

    lamps = {
        "A": _build_decimal_decoder(
            board, taps["A"], 8, 255, 1250, 1250),
        "B": _build_decimal_decoder(
            board, taps["B"], 8, 255, 1550, 1280),
        "RESULT": _build_decimal_decoder(
            board, taps["RESULT"], 9, 510, 1850, 1310),
    }

    save(board, "click-adder", 1595, 1270, zoom=6.3, timestamp=0)
    return board, rows, taps, (button_a, button_b, button_sum), lamps


if __name__ == "__main__":
    generated, _, _, buttons, _ = click_adder()
    print(f"click-adder: {len(generated.blocks)} blocks, buttons={buttons}")
