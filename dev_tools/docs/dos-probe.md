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
./build-probe/dev_tools/dos_probe/opengold_dos_probe "$OPENGOLD_GAME_DIR" scenario.txt out/
```

`opengold_dos_probe_tests` checks the CPU against documented 8086 and 80186
results; it needs no game files.

A scenario is a text file of commands, one per line (`#` starts a comment):

| Command | Effect |
| --- | --- |
| `wait [N]` | Runs until the game waits for a key (at most N thousand instructions). |
| `key NAME` | Presses a key and waits: a character, or `enter`, `esc`, `space`, `backspace`, `tab`, `up`, `down`, `left`, `right`, `home`, `end`, `pgup`, `pgdn`, `ins`, `del`, `f1`-`f5`, `f10`. |
| `type TEXT` | Presses each character in turn. |
| `run N` | Runs N thousand instructions. |
| `shot NAME` | Saves the visible screen as `out/NAME.png` (320 by 200). |
| `dump NAME` | Saves the first megabyte of memory as `out/NAME.bin`. |
| `trace NAME` | Finds the running ECL script and records it to `out/NAME.trace` until `untrace`. |

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

## Script traces

`trace` finds the running script by matching every `ECLn.DAX` record against
memory. The original loads the script at offset `9900` of its data segment,
so the script's addresses are offsets in that segment: the town's flags at
`4A00`-`4BFF`, its working cells at `6D00`-`7FFF`, its text cells at
`9700`-`98FF`. A trace has one line per event, in order:

- `R 9A3D-9A59`: the interpreter read script bytes `9A3D` to `9A59` (an
  instruction and its operands, or several in a row).
- `W 4A04 02`: a byte of a variable cell, or of the script itself, was written.

The first line names the archive, record and data segment, such as
`# ECL3.DAX record 0, data segment 3132` for New Phlan.

## Limits

- The PC speaker, Tandy sound and the joystick are absent; the game runs
  silently and finds no joystick.
- There is no mouse; the original does not use one.
- Video is EGA only. CGA and Tandy modes are not emulated.
