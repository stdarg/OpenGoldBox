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
| `wait [N]` | Runs until the game waits for a key (at most N thousand instructions). |
| `key NAME` | Presses a key and waits: a character, or `enter`, `esc`, `space`, `backspace`, `tab`, `up`, `down`, `left`, `right`, `home`, `end`, `pgup`, `pgdn`, `ins`, `del`, `f1`-`f5`, `f10`. |
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

`out/run.log` lists each command's instruction count, whether the game was
waiting for a key, any text the program printed, and every file it opened.

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

## Limits

- The PC speaker, Tandy sound and the joystick are absent; the game runs
  silently and finds no joystick.
- There is no mouse; the original does not use one.
- Video is EGA only. CGA and Tandy modes are not emulated.
