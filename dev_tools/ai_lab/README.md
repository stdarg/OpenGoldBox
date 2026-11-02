# AI lab

Replays real campaign fights through the game's own combat and computer play, so
a change to the computer's play can be measured in seconds. It is a command-line
research tool: the game never uses it. See the [guide](../docs/ai-lab.md).

```bash
dev_tools/ai_lab/run.sh --party-of 0026 --min-foes 4 --seeds 16
```

The first run builds the lab and makes its scenario pack from your original files
(`OPENGOLD_GAME_DIR`, or `~/POOLRAD`). `--new-pack` makes the pack again.
