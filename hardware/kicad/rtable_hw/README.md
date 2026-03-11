щл# rtable_hw (KiCad)

Files:
- `rtable_hw.kicad_pro` - KiCad project file
- `rtable_hw.kicad_sch` - full wiring schematic with symbols and nets

Open:
1. Start KiCad
2. Open `rtable_hw.kicad_pro`
3. Open schematic editor and load `rtable_hw.kicad_sch`

Implemented schematic blocks:
- `J1` M5Stamp-C3 header
- `U1` ULN2003A
- `J2` 28BYJ-48 motor connector
- `J3` Servo connector
- `J4` Button connector
- `J5` +5V input connector

Implemented net mapping:
- GPIO4/5/6/7 -> ULN2003 IN1/IN2/IN3/IN4
- ULN2003 O1/O2/O3/O4 -> motor coils `COIL_A/B/C/D`
- ULN2003 `COM` -> `+5V`, ULN2003 `GND` -> `GND`
- GPIO8 -> Servo signal
- GPIO10 -> Button (active-low)
- External 5V for ULN2003 + Servo
- Common GND across all blocks
