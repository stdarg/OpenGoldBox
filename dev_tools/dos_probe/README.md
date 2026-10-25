# DOS probe

A research-only, headless PC that runs the user's own copy of the original
game deterministically, drives it with scripted keys, and records screenshots
and ECL script traces for comparison with OpenGoldBox. It never writes the
game directory. See the [guide](../docs/dos-probe.md).

```bash
cmake -S . -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENGOLD_BUILD_DOS_PROBE=ON
cmake --build build-probe --target opengold_dos_probe
./build-probe/dev_tools/dos_probe/opengold_dos_probe "$OPENGOLD_GAME_DIR" scenario.txt out/
```
