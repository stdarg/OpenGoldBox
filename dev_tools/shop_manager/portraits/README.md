Place newly generated PNG portraits here. The Shop Manager lists these files and
the existing portraits in `art/portraits/NPCs/`. It exports a chosen portrait
from here to `data/art/portraits/shops/` and saves it in the location's JSON
record as `shops/<name>.png`.

The three temple portraits are default selections for their JSON records:

- `temple-sune.png`: Black woman, rose-red Sunite vestments and Sune's
  red-haired face symbol. Prompt theme: a warm, welcoming priest of love and
  beauty in a flower-filled temple.
- `temple-tempus.png`: East Asian man, mail and crimson tabard bearing Tempus's
  upright flaming sword. Prompt theme: a steady veteran battle-priest in a
  torchlit armory-temple.
- `temple-ilmater.png`: older South Asian woman, humble gray robes and hands
  bound with red cord emblem. Prompt theme: a compassionate healer in a quiet
  hidden sanctuary.

Each was generated as an opaque 1145 × 1374, late-1990s fantasy RPG pixel-art
portrait, with one attendant, no text or border. The deity themes and symbols
follow the [D&D Basic Rules deity table](https://www.dndbeyond.com/sources/dnd/basic-rules-2014/appendix-b-gods-of-the-multiverse).
The exact generation prompts are in [`PROMPTS.md`](PROMPTS.md).
