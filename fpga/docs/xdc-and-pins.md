# XDC and Board Pin Integration

## What belongs in XDC

XDC owns clock definitions, clock groups, IO timing, board pin locations, IO
standards, and physical planning. It does not replace RTL fixes for timing
violations.

## Recommended order

1. Define primary clocks.
2. Define generated clocks or use IP-generated clocks.
3. Declare asynchronous clock groups where the CDC structure requires them.
4. Add input/output delays from the board and external-device contract.
5. Assign board package pins and I/O standards.
6. Add physical planning only for a documented physical requirement.
7. Run `check_timing`, `report_clock_interaction`, and `report_cdc`.

## Example

`xdc/timing_template.xdc` is intentionally commented. Replace names only after
checking the generated BD wrapper port names and the actual board schematic.

## Rules

- Do not add a false path to hide an unsafe data path.
- Do not use an XDC pin assignment copied from another board.
- Verify clock names with `get_clocks`, not only RTL net names.
- Verify asynchronous groups cover both directions and the intended clock family.
- Keep board-specific constraints in the project, not in generic RTL.
- Re-run timing after a BD, IP, clock, reset, or pin change.
