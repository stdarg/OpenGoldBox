# Development installation

This guide sets up a machine to work on OpenGoldBox: the tools to install, the
project's git hooks, the original game files and every developer tool in
`dev_tools/`. When it is done, [BUILD.md](BUILD.md) covers the builds
themselves.

| System | What builds there today |
| --- | --- |
| macOS | Everything: native core and tests, the game, the demos, every dev tool. |
| Windows | Native core and tests, the game, the demos, most dev tools (see [the dev tools](#developer-tools)). |
| Linux (Bazzite, Ubuntu) | Native core and tests, the game and its package (x86_64), and the dev tools whose extensions list Linux. |

The macOS tools and versions match the development Mac, which runs the .NET
Godot build. The Windows hook check has not been run yet. The Linux steps were
run on Bazzite 44 on 2026-11-07, where the native core and its tests build with
the system's GCC 16.1, and the game with Homebrew's GCC 16.2.

## What to install

| Tool | Version | Used for |
| --- | --- | --- |
| C++20 compiler | Apple Clang (Xcode Command Line Tools), MSVC from Visual Studio Build Tools 2026 (version 18), or GCC | everything |
| CMake | 3.24 or newer | every build |
| Ninja | any recent | the `default` and `macos-universal` presets, the dev tools |
| Python | 3.8 or newer, standard library only | generating godot-cpp, `tools/*.py`, localization checks |
| Git | any recent | the repository, fetching godot-cpp, the hooks |
| LLVM | 23.x (`clang-format`, `git-clang-format`, `clang-tidy`) | the pre-commit hook and [the coding style](CODING_STYLE.md) |
| Godot | 4.7.2, the standard build (not .NET), and its export templates | the game, the demos, the Godot checks, the dev tools |
| ccache | optional | reusing compiled objects across build directories and worktrees |
| distrobox and podman | any recent, Linux only | [the Linux package for players](BUILD.md#which-linux-systems-it-runs-on), built in an AlmaLinux 8 container |
| cage | any recent, Linux only, optional | the windowed Godot checks [without a screen](TESTING.md); Bazzite ships it, `sudo apt-get install cage` on Ubuntu |

The project has no C# code, so the standard Godot build is enough. The .NET
build also works if it is already installed.

The pre-commit hook refuses any clang-format other than major version 23,
because another release lays code out differently.

## macOS

1. Install Apple's compiler:

   ```bash
   xcode-select --install
   ```

2. Install [Homebrew](https://brew.sh), then the tools:

   ```bash
   brew install cmake ninja python git llvm ccache
   ```

3. Homebrew keeps LLVM off `PATH` (it would hide Apple's clang). Add its tools
   at the end of `PATH`, so Apple's compiler still builds the code, in
   `~/.zprofile`:

   ```bash
   export PATH="$PATH:$(brew --prefix llvm)/bin"
   ```

   Homebrew's `llvm` follows the newest release. When LLVM 24 ships, install
   `llvm@23` instead and point `PATH` at `$(brew --prefix llvm@23)/bin`.

4. Install Godot 4.7.2: download `Godot_v4.7.2-stable_macos.universal.zip` from
   [the 4.7.2 release](https://github.com/godotengine/godot/releases/tag/4.7.2-stable)
   and move `Godot.app` to `/Applications`. CMake finds it there. The dev
   tools look for `GODOT_BIN`, then `godot` on `PATH`, then the .NET app's
   path, and three of them skip `PATH`, so set `GODOT_BIN` in `~/.zprofile`:

   ```bash
   export GODOT_BIN=/Applications/Godot.app/Contents/MacOS/Godot
   ```

5. Install the export templates the app build needs: open Godot, then
   **Editor > Manage Export Templates > Download and Install**. They land in
   `~/Library/Application Support/Godot/export_templates/4.7.2.stable/`.

6. Turn on [the git hooks](#git-hooks) and [point at the game files](#original-game-files).

7. Check the setup with a full build and every test:

   ```bash
   ./build.sh --test-all
   ```

## Windows

1. Install [Visual Studio Build Tools 2026](https://visualstudio.microsoft.com/downloads/)
   (version 18) with the **Desktop development with C++** workload. The helper
   scripts expect it at
   `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` and use its
   bundled CMake and Ninja, so they run from a plain PowerShell or VS Code
   terminal.

2. Install Git (with Git Bash), Python, LLVM and ccache:

   ```powershell
   winget install --id Git.Git -e
   winget install --id Python.Python.3.12 -e
   winget install --id LLVM.LLVM -e --version <a 23.x version>
   winget install --id Ccache.Ccache -e
   ```

   `winget show LLVM.LLVM --versions` lists the versions to pick from. Add
   `C:\Program Files\LLVM\bin` to `PATH` if the installer did not. The `python`
   on `PATH` may be the Microsoft Store stub, which builds cannot use; the
   install above replaces it, or pass `-DPython3_EXECUTABLE=C:/path/to/python.exe`
   when configuring. Open a new terminal afterwards so every tool is on `PATH`.

3. Install Godot 4.7.2: download `Godot_v4.7.2-stable_win64.exe.zip` from
   [the 4.7.2 release](https://github.com/godotengine/godot/releases/tag/4.7.2-stable)
   and unzip it into a folder of its own, such as `C:\Tools\Godot`. CMake looks
   for `godot_console`, `godot` or `Godot` on `PATH`, so rename the two
   executables to `godot.exe` and `godot_console.exe` and add the folder to
   `PATH`. For the dev tools, also set `GODOT_BIN` to `C:\Tools\Godot\godot.exe`.

4. Install the export templates: download
   `Godot_v4.7.2-stable_export_templates.tpz` from the same release (it is a
   zip). Copy `windows_debug_x86_64.exe` and `windows_debug_x86_64_console.exe`
   from its `templates` folder into `build\export-templates\` in the checkout,
   where the game build looks for them.

5. Install the VS Code extensions `.vscode/extensions.json` recommends.

6. Turn on [the git hooks](#git-hooks) and [point at the game files](#original-game-files).

7. Check the setup:

   ```powershell
   .\build.cmd
   .\build-opengoldbox.cmd
   ```

   `build.cmd` builds and tests the native core; `build-opengoldbox.cmd`
   builds the game into `win-package\` and runs its tests.

## Linux: Bazzite and Ubuntu

One set of steps covers both, with every tool from
[Homebrew](https://brew.sh). Bazzite ships Homebrew; on Ubuntu, step 1
installs it.

1. Ubuntu only: install Homebrew's own requirements, then Homebrew, as its
   install page says:

   ```bash
   sudo apt-get install build-essential procps curl file git
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

   Then follow the installer's last lines, which add `brew shellenv` to
   `~/.bashrc`.

2. Install the tools:

   ```bash
   brew install gcc cmake ninja python git llvm ccache
   ```

3. Homebrew keeps LLVM off `PATH`. Add its tools at the end of `PATH` in
   `~/.bashrc`, so GCC still builds the code:

   ```bash
   export PATH="$PATH:$(brew --prefix llvm)/bin"
   ```

   As on macOS, use `llvm@23` once LLVM 24 ships.

4. Check that the compiler builds a C++20 program against the system's
   headers. This is the step most likely to differ on Bazzite, whose base
   image carries no development packages:

   ```bash
   printf '#include <cstdio>\n#include <span>\nint main(){std::puts("ok");}\n' > /tmp/check.cpp
   g++ -std=c++20 /tmp/check.cpp -o /tmp/check && /tmp/check
   ```

   It prints `ok`. Report any other result before going on. On Bazzite 44
   the system's own `g++` (GCC 16.1) passes, and CMake builds the native core
   with it rather than Homebrew's `g++-16`. The game needs Homebrew's: see
   step 7.

5. Install Godot 4.7.2: download `Godot_v4.7.2-stable_linux.x86_64.zip` from
   [the 4.7.2 release](https://github.com/godotengine/godot/releases/tag/4.7.2-stable),
   unzip it, and put the executable on `PATH` as `godot`:

   ```bash
   mkdir -p ~/.local/bin
   mv Godot_v4.7.2-stable_linux.x86_64 ~/.local/bin/godot
   chmod +x ~/.local/bin/godot
   ```

   CMake and the dev tools run Godot from the command line, which needs an
   executable on `PATH` rather than a Flatpak or Steam copy. Set `GODOT_BIN`
   in `~/.bashrc` too, for the tools that skip `PATH`:

   ```bash
   export GODOT_BIN="$HOME/.local/bin/godot"
   ```

   For the game, install the Linux export templates: download
   `Godot_v4.7.2-stable_export_templates.tpz` from the same release (a zip of
   about 1.3 GB) and unpack its two x86_64 Linux templates where Godot looks
   for them:

   ```bash
   templates=~/.local/share/godot/export_templates/4.7.2.stable
   mkdir -p "$templates"
   unzip -j Godot_v4.7.2-stable_export_templates.tpz \
       'templates/linux_*.x86_64' templates/version.txt -d "$templates"
   ```

6. Turn on [the git hooks](#git-hooks) and [point at the game files](#original-game-files).

7. Check the setup with the native core and its tests:

   ```bash
   ./build.sh default --test
   ```

   Then build the game and its package. The first configure picks the compiler,
   and the game needs one with the static libstdc++, which Bazzite's own GCC
   lacks, so name Homebrew's that once:

   ```bash
   CC=gcc-16 CXX=g++-16 ./build.sh --test-all
   ```

8. To make the package for players, which runs on older distributions too,
   build it in its AlmaLinux 8 container. It needs
   [distrobox](https://distrobox.it) and podman, which Bazzite ships; on
   Ubuntu 24.04, `sudo apt-get install distrobox podman`. The first run creates the
   container and installs its tools:

   ```bash
   tools/linux_release_build.sh --test-all
   ```

   See [which Linux systems the package runs on](BUILD.md#which-linux-systems-it-runs-on).

## Git hooks

`tools/git-hooks/` holds two hooks. Turn them on once per clone; every
worktree of the clone shares the setting:

```bash
git config core.hooksPath tools/git-hooks
```

- `pre-commit` formats the staged C++ lines with `git clang-format` and
  re-stages them, then runs `clang-tidy` on each staged C++ file and refuses a
  missing brace. It needs `clang-format` and `git-clang-format` from LLVM 23 on
  `PATH`, and stops the commit without them. Braces are checked only when
  `clang-tidy` is on `PATH` and one of `build`, `build-game`, `build/debug`,
  `build/macos-universal` or `build/linux` holds a `compile_commands.json` (any configure with
  Ninja writes one); otherwise it says so and lets the commit through.
- `commit-msg` refuses a `Co-Authored-By` line naming Claude or Anthropic.

The hooks are shell scripts. On Windows, Git runs them with the `sh` that Git
for Windows bundles, so they work from PowerShell, VS Code or Git Bash.

Check the hook's tools from the shell Git uses (Git Bash on Windows):

```bash
clang-format --version      # must say version 23
git clang-format -h         # must print usage, not "not a git command"
clang-tidy --version
```

On Windows, if `git clang-format` is "not a git command", the LLVM install did
not put `git-clang-format` on Git Bash's `PATH`; this has not been checked on
Windows yet. [CODING_STYLE.md](CODING_STYLE.md#pre-commit-hook) describes
everything the hook does.

## Original game files

Nothing in the repository holds the original Pool of Radiance files. Anything
that reads them uses `OPENGOLD_GAME_DIR`, pointed at the folder with the
`.DAX` files. Set it in the shell profile (`~/.zprofile`, `~/.bashrc`):

```bash
export OPENGOLD_GAME_DIR="$HOME/POOLRAD"
```

or for Windows, persistently:

```powershell
setx OPENGOLD_GAME_DIR "C:\Games\POOLRAD"
```

Most dev tools fall back to `~/POOLRAD` when it is unset. Tests that need the
files skip without it, so set it before a full test run.

Do not commit original SSI game files or derived assets. Keep local copies in
ignored directories such as `testdata/local/` or `user-data/`.

## Developer tools

Each tool builds what it needs on first launch; none needs the game build
first, except the creature edits importer, which is part of it. Run the
commands from the repository root. Every launcher is a bash script: on Windows,
run them from Git Bash. Each tool reads the game folder from its first
argument, then `OPENGOLD_GAME_DIR`, then `~/POOLRAD`, and Godot from
`GODOT_BIN`. The [dev tools index](../dev_tools/docs/README.md) says what each
one does.

On Windows, start Git Bash inside the Visual Studio build environment, so that
CMake finds the compiler and Ninja, and set `GODOT_BIN`:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
set GODOT_BIN=C:/bin/godot.exe
"C:\Program Files\Git\bin\bash.exe"
```

| Tool | macOS | Windows | Linux |
| --- | --- | --- | --- |
| Tile Art Viewer | yes | Git Bash | yes |
| Replacement Art Creator | yes | Git Bash | yes |
| Combat Sprite Catalog | yes | Git Bash | extension lists Linux, launch not documented |
| Shop & Temple Manager | yes | Git Bash | extension lists Linux, launch not documented |
| Party tester | yes | Git Bash | no: extension lists no Linux library |
| Combat tester | yes | Git Bash | no: extension lists no Linux library |
| Creature Stats Editor | yes | Git Bash | no: extension lists no Linux library |
| Creature edits importer | yes | yes | no: part of the game build |
| AI lab | yes | Git Bash | not documented |
| DOS probe | yes | not documented | not documented |

### Tile Art Viewer

Needs CMake, Ninja, Python, Git and Godot. Builds into `build/tile-art-viewer`.

```bash
./dev_tools/tile_art_viewer/run.sh [POOLRAD_DIRECTORY]
```

The first run imports the Godot project headless; Godot may crash as that
import exits, which is harmless. Checks, such as
`godot --headless --path dev_tools/tile_art_viewer/godot -- --viewer-check=/tmp/opengold-waterfront.png`,
are in its [README](../dev_tools/tile_art_viewer/README.md).

### Replacement Art Creator

Same needs as the Tile Art Viewer, and shares its build
(`build/tile-art-viewer`). The game files are optional: only the Reference tab
reads them.

```bash
./dev_tools/replacement_art_creator/run.sh [POOLRAD_DIRECTORY]
```

Checks, including `python3 dev_tools/replacement_art_creator/assign_used_art.py --check-pack`,
are in its [README](../dev_tools/replacement_art_creator/README.md).

### Combat Sprite Catalog

Builds into `build/combat-sprite-catalog` and needs the game files.

```bash
./dev_tools/combat_sprite_catalog/run.sh [POOLRAD_DIRECTORY]
```

Check: `godot --headless --path dev_tools/combat_sprite_catalog/godot -- --catalog-check`
with `OPENGOLD_GAME_DIR` set; more in its
[README](../dev_tools/combat_sprite_catalog/README.md).

### Shop & Temple Manager

Needs CMake, Ninja and Godot, shares `build/tile-art-viewer`, and needs the
game files.

```bash
./dev_tools/shop_manager/run.sh [POOLRAD_DIRECTORY]
```

Check, after launching it once:
`godot --headless --path dev_tools/shop_manager/godot -- --shop-check` with
`OPENGOLD_GAME_DIR` set. See its [README](../dev_tools/shop_manager/README.md).

### Party tester

Compiles the game's own code into `build/party-tester`, which takes a few
minutes the first time. Git Bash has no `rsync`, so on Windows the launcher
copies with `cp`.

```bash
dev_tools/party_tester/run.sh
dev_tools/party_tester/check.sh      # headless; passes on "Party tester passed"
```

See its [README](../dev_tools/party_tester/README.md) for the sweep and its
settings.

### Combat tester

Like the party tester: the game's code in `build/combat-tester`.

```bash
dev_tools/combat_tester/run.sh
```

Its headless checks run against the staged project after one launch; see its
[README](../dev_tools/combat_tester/README.md).

### Creature Stats Editor

Builds godot-cpp and the game's creature data into
`build/creature-stats-editor` on first run, which takes a few minutes.

```bash
dev_tools/creature_stats_editor/run.sh
dev_tools/creature_stats_editor/check.sh    # headless
```

See [the editor's guide](../dev_tools/docs/creature-stats-editor.md).

### Creature edits importer

Part of the game build: `opengold_import_edits` lands in the game's build
tree.

```bash
build/macos-universal/opengold_import_edits --check
build/game/opengold_import_edits.exe --check            # Windows
cmake --build build/macos-universal --target opengoldbox_import_edits
```

See [creature edits](../dev_tools/docs/creature-edits-import.md).

### AI lab

A command-line tool with its own Release build in `build/ai-lab`. The first
run makes a scenario pack from the game files, about a minute and a half;
replays after that need no game files.

```bash
dev_tools/ai_lab/run.sh --list
dev_tools/ai_lab/run.sh --party-of 0026 --min-foes 4 --seeds 16
```

See [the AI lab guide](../dev_tools/docs/ai-lab.md).

### DOS probe

A command-line emulator for the original game, with its own CPU and EGA
emulation: no DOSBox needed. It builds into `build-probe`:

```bash
cmake -S . -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENGOLD_BUILD_DOS_PROBE=ON
cmake --build build-probe --target opengold_dos_probe opengold_dos_probe_tests
./build-probe/dev_tools/dos_probe/opengold_dos_probe "$OPENGOLD_GAME_DIR" scenario.txt out/
```

See [the DOS probe guide](../dev_tools/docs/dos-probe.md).
