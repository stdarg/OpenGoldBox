# OpenGoldBox new art

The optional new art for OpenGoldBox, downloaded apart from the game. Choose
this folder in the game's Options (Interface, **New art folder**); the game
then shows its walls, close-ups, monster figures and portraits in place of the
original game's, as the Options screen's art settings say. Without it, or for
any piece that cannot be loaded, the game shows the original art (the log names
a file it could not load).

- `new-art-pack.json`: what this folder is (`format`) and its `version`.
- `walls/`: the 3D view's wall pieces, one folder per original appearance.
- `closeups/`: animated close-ups drawn from the original's descriptions.
- `missiles/`: missiles in a fight, such as `boulder.png`.
- `monsters/`: each exported creature's weapon-set poses, in the folder named
  like its `data/monsters` record (`monsters/mon2cha_dax_4/<set>/ready.png`).
  The creature's name, gear and size stay with the game in `data/monsters`.
- `portraits/`: the characters' portraits (`portraits.json` lists them),
  `NPCs/` for the characters a script brings in, and `shops/` for the
  shopkeepers and temple priests.

The developer tools publish here.
