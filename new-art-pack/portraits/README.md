# Character portraits

The catalog contains 972 pixel-art portraits: three for every combination of
12 classes, nine in-game races, and three genders. The first 324 established
complete coverage, the second 324 added new character designs, and the latest
324 show each class in action. The Character Review demo, character pool, and
party previews use this catalog. The selector can filter by gender, class, and
race.

Every PNG is a 1254 x 1254 square on a black background. Faces stay readable
at the portrait picker's smaller size. The action set keeps both eyes visible
while varying camera angle, pose, clothing, and class activity. Its close-up
framing gives priority to the face and includes hands, tools, weapons, or spell
effects where the action calls for them. Nonbinary portraits are briefed for
androgynous presentation. All characters are adults and fully clothed.

`portraits.json` maps each PNG basename to `Class`, `Gender`, and `Race`, using
the character creator's display names. `prompts.json`,
`roster-prompts.json`, `class-expansion-prompts.json`, and
`full-coverage-prompts.json` record prompts for the first 324.
`second-set-prompts.json` records the second 324. `action-set-prompts.json`
records the action portraits and names the two earlier portraits for the same
race, class, and gender combination in each entry's `prior_portraits` field.
The previous PNGs and their catalog entries remain unchanged.

The four original Orc portraits were revised to visibly green skin at the
user's request; `orc-green-revision-prompts.json` records those edit prompts.
Later Orc portraits were generated with green skin from the start.

All portraits use the built-in image generator. Pixel-art styling does not
imply a mathematically uniform low-resolution pixel grid or a separate,
hand-tuned 88 x 88 asset. Race and gender metadata describe picker options;
they do not impose character-creation rules.
