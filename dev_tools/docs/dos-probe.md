# DOS probe

The DOS probe runs the user's own copy of the original Pool of Radiance (PC
1.3, `START.EXE` and `GAME.OVR`) headless and records what it does, so
OpenGoldBox can be compared with the running original. It is a research tool
only. The game never uses it, nothing it runs ships with OpenGoldBox, and it is
built only on request. [TECH.md](../../docs/TECH.md) still holds: the game
reads the original data files and does not emulate the original program.

## What it is

- **A PC:** an 8086 with the 80186 additions ([cpu.cpp](../dos_probe/cpu.cpp)),
  an EGA card in mode 0Dh ([ega.cpp](../dos_probe/ega.cpp)), the timer and the
  keyboard, and just enough BIOS and DOS to run the game
  ([machine.cpp](../dos_probe/machine.cpp)). Interrupt vectors the program does
  not hook point at host traps (`0F FF n`, an opcode no real program uses).
- **Deterministic:** time is counted in instructions (one timer tick per 20,000
  instructions), so the same scenario always takes the same path, the
  original's dice included.
- **Read-only original:** drive `C:\POOLRAD` reads the game directory. Every
  file the program creates or opens for writing (the game opens its data files
  read-write) is copied to the run's `drive` folder first, and later reads see
  that copy. The installation, its `SAVE` folder included, is never written.
- **Fast:** about 30 million instructions a second; the main menu is reached in
  under a second.

## Building and running

```bash
cmake -S . -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENGOLD_BUILD_DOS_PROBE=ON
cmake --build build-probe --target opengold_dos_probe opengold_dos_probe_tests
./build-probe/dev_tools/dos_probe/opengold_dos_probe [--fresh] "$OPENGOLD_GAME_DIR" scenario.txt out/
```

`--fresh` hides the installation's `SAVE` folder, so a run starts with no
saved games or characters whatever the player has.
`opengold_dos_probe_tests` checks the CPU against documented 8086 and 80186
results; it needs no game files. `opengold_dos_probe_experiments` runs the
[experiments](#experiments) against the installed game, and is skipped without
`OPENGOLD_GAME_DIR`.

A scenario is a text file of commands, one per line (`#` starts a comment):

| Command | Effect |
| --- | --- |
| `wait [N]` | Runs until the game waits for a key (at most N thousand instructions): it has looked for one and found none for 3 million instructions. |
| `key NAME` | Presses a key and waits: a character, or `enter`, `esc`, `space`, `backspace`, `tab`, `up`, `down`, `left`, `right`, `home`, `end`, `pgup`, `pgdn`, `ins`, `del`, `f1`-`f5`, `f10`, or `kp1`-`kp9` (the keypad with Num Lock on). |
| `type TEXT` | Presses each character in turn. |
| `run N` | Runs N thousand instructions. |
| `shot NAME` | Saves the visible screen as `out/NAME.png` (320 by 200). |
| `dump NAME` | Saves the first megabyte of memory as `out/NAME.bin`. |
| `cpu N` | Runs N instructions, logging each one's address and registers to `out/cpu.log`. |
| `locate` | Finds the running ECL script again, after the party enters another area. |
| `trace NAME` | Records the running ECL script to `out/NAME.trace` until `untrace`. |
| `trace-bytes NAME` | As `trace`, with every script byte read on its own line, its value and the instruction that read it. |
| `poke ADDRESS HEX` | Writes bytes at a data-segment offset, such as a program over the script. |
| `peek ADDRESS N` | Logs N bytes from a data-segment offset to `run.log`. |
| `watch FIRST LAST NAME` | Logs every write to linear addresses FIRST to LAST (hex) with the writing instruction to `out/NAME.watch`, until `unwatch`. |
| `unwatch` | Ends every watch and any script trace. |
| `coverage SEGMENT NAME` | Counts each instruction address run in a code segment (hex, or `all`) until `uncoverage`, which writes `out/NAME.coverage`. |
| `services` | Logs how often each BIOS and DOS service was called since the last `services`. |

`out/run.log` lists each command's instruction count, whether the game was
waiting for a key, any text the program printed, and every file it opened.

In the original, Up steps forward, Left and Right turn and Down turns around;
in a list, End moves the highlight down and Home up.

For example, the original's main menu, then the first saved game:

```text
wait
key enter
shot menu
key l
key a
shot loaded
```

[fresh_party.txt](../dos_probe/experiments/fresh_party.txt) makes a new
one-character party with scripted keys and presses through Rolf's tour, leaving
it at New Phlan's west gate in the town's script; one step west from there
enters the Slums.

## Script traces

`trace` finds the running script by matching every `ECLn.DAX` record against
memory. The original loads the script at offset `9900` of its data segment, one
byte per address, so the script's own addresses are offsets in that segment.
Its variables are not: the probe found where the original's `SAVE` writes each
address by having it write distinct values, and the trace translates them back:

| Script addresses | In the data segment |
| --- | --- |
| `4A00`-`4EFF` | 16-bit cells from offset `8700`, two bytes per address |
| `6D00`-`6EFF` | 16-bit cells from offset `9100`, two bytes per address |
| `9700`-`98FF` | offset `9500` on, one byte per address |
| `9900`-`B6FF` | the script itself, at its own offset |

Like OpenGoldBox's machine, the original keeps each variable address an
independent 16-bit cell. A trace has one line per event, in order:

- `R 9A3D-9A59`: the interpreter read script bytes `9A3D` to `9A59` (an
  instruction and its operands, or several in a row).
- `W 4A04 0002`: a variable cell was written, here with 2.
- `W 9E00 0F`: a byte of a text cell or of the script itself was written.

The first line names the archive, record and data segment, such as
`# ECL3.DAX record 0, data segment 3132` for New Phlan.

## Experiments

[experiments/](../dos_probe/experiments) holds programs the original's own
interpreter runs: [vm_semantics.py](../dos_probe/experiments/vm_semantics.py)
assembles one with [eclasm.py](../dos_probe/experiments/eclasm.py), puts it
over New Phlan's movement entry after the fresh party's tour, takes one step
east, and reads its results from the trace:

```bash
python3 dev_tools/dos_probe/experiments/vm_semantics.py \
    build-probe/dev_tools/dos_probe/opengold_dos_probe "$OPENGOLD_GAME_DIR" out/
```

[battlefield.py](../dos_probe/experiments/battlefield.py) starts fights in
the Slums and compares where both sides stand with OpenGoldBox's formation
(`opengold_maps --formation`). For each case a new party of one to six walks
into the Slums along a route [route.py](../dos_probe/experiments/route.py)
plans over OpenGoldBox's map export. The Slums script's entry 0 is replaced by
EXIT, so no roaming monsters come. Its entry 1 gets a program that, on the
case's square and facing only, runs `SETUP MONSTER` with the case's distance,
`LOAD MONSTER` with its orcs, and `COMBAT`. The original reads a script's entry
addresses when it loads the script, so programs go at the entries' code, not in
the jump table. Each combatant's square is the first the original writes for it into its
combat occupancy map (50 by 25 bytes at linear `1A99B`, 0 or a combatant's
number, party first), as monsters may move before the party's first turn.

```bash
python3 dev_tools/dos_probe/experiments/battlefield.py \
    build-probe/dev_tools/dos_probe/opengold_dos_probe build-probe/opengold_maps \
    "$OPENGOLD_GAME_DIR" out/
```

[wilderness.py](../dos_probe/experiments/wilderness.py) does the same on the
wilderness map. A new party's first step in New Phlan runs a program at the
town script's entry 0 that takes it onto the central wilderness map at the
case's square, as the city's gates do (the window column to `0x49C3`, the row
to `0x49C4`, 7 to `0x6E12`, `NEW ECL 26`). The wilderness script's arrival
entry becomes EXIT and its step entry stages lizard men (MON7CHA record 57).
An arrow key's step then starts the fight on the field the original grows
from the square's terrain. That field is random, so it is read from the
running game (50 by 25 combat tiles, row by row, 7 bytes into the heap block
the far pointer at `148C:557E` names) and given to
`opengold_maps --wild-formation` with the step's facing and distance 2.

```bash
python3 dev_tools/dos_probe/experiments/wilderness.py \
    build-probe/dev_tools/dos_probe/opengold_dos_probe build-probe/opengold_maps \
    "$OPENGOLD_GAME_DIR" out/
```

## Findings

Recorded 2026-10-24 with `START.EXE` SHA-1
`0640ba4fabdc9120dc89c518fab04e388164e2a2` and `GAME.OVR`
`6f28ec7a62f5cfed1b40771c086c1834ebe35b15`:

- **Entries:** a step runs the script's entry 0, then entry 1, as
  OpenGoldBox's session does.
- **GETTABLE** leaves the flags of the last COMPARE: after `COMPARE 1,1` (or
  `2,1`) and a GETTABLE, `IF =` (or `IF >`) still holds. `ecl_tests` checks
  OpenGoldBox does the same.
- **COMPARE a, b** sets `IF <` when a is less than b.
- **RANDOM N** gives 0 to N inclusive, except that 255 gives 0 to 254: in 3,000
  rolls `RANDOM 8` gave 8 about 350 times and never more, `RANDOM 255` gave 254
  eight times and never 255, `RANDOM 0` always 0.
- **A false IF** skips the next instruction by a table of operand counts in the
  interpreter, not by decoding it. The table skips only the opcode of VERTICAL
  MENU, ON GOTO, ON GOSUB and HORIZONTAL MENU, two of SETUP MONSTER's three
  operands and one of ADD NPC's two; the bytes left over then run as
  instructions (an ON GOTO's list becomes a GOTO). OpenGoldBox skips whole
  instructions. None of the 1,936 IFs in the 29 original scripts is followed by
  one of these six commands, so both skip the same bytes on every original
  script.
- **Area crossing:** a step west from New Phlan's west gate (0,4) enters the
  Slums on (15,4) still facing west, where OpenGoldBox puts the party too;
  every battlefield route above starts from that square.
- **Entry 0 runs before the step moves the party:** it sees the old square
  (with the new facing); entry 1 runs on the new square. OpenGoldBox's session
  binds the same squares.
- **Starting positions** ([PLACE-1](../../docs/SRD-DECISIONS.md#place-1-2026-10-10-starting-positions-as-the-original-game-had-them)):
  in sixteen fights, facing each of the four directions at distances 0, 1 and
  2, with parties of one to six and groups of 1 to 30 orcs, the original stood
  every combatant on the square OpenGoldBox's formation gives. This includes
  ranks, spilling into neighbouring squares and the distance capped by the view
  ahead. A random encounter of the original's own (one orc against one
  character at Slums (14,5) facing south) matched as well.
- **Wilderness fights** (2026-10-25): in ten fights on the central wilderness
  map, on six squares of different terrain, with parties of one to six and
  groups of 4 to 30 lizard men, the original stood every combatant on the
  square OpenGoldBox's formation gives on the original's own field, when that
  formation faces the way the party stepped and uses distance 2. The original
  ignores SETUP MONSTER's distance there: 0, 1, 2 and 3 all stood the monsters
  at 2. OpenGoldBox's wilderness fights faced north and used SETUP MONSTER's
  distance; they now do as the original
  ([PLACE-2](../../docs/SRD-DECISIONS.md#place-2-2026-10-25-wilderness-fights-face-the-step)).
  The probe could not make the original step diagonally: the keypad's
  9, Page Up and the top row's 9 reach the game through INT 16h and it ignores
  them, so how a diagonal step faces is not known from the probe.
- **Fight setup checks the keyboard** between combatants it places, so a key
  pressed then cuts the setup short and the rest of the group is left out.
  The probe therefore counts the game as waiting only after 3 million
  instructions of looking for a key; with less, it once seemed that the
  original fields only 12 to 17 of a big group.

## Limits

- The PC speaker, Tandy sound and the joystick are absent; the game runs
  silently and finds no joystick.
- There is no mouse; the original does not use one.
- Video is EGA only. CGA and Tandy modes are not emulated.
