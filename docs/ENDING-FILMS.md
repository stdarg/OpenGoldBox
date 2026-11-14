# Ending films

Each game ends with a film of about 20 seconds when the party wins: the heroes
defeat the game's final foe, and then the film shows how the game's story
ends, a satisfying conclusion to it. A welcome by the people they saved is
one such ending, where the story has one; it is not a rule (the user's
direction, 2026-11-14). This document plans the seven films: the facts behind each
finale, a storyboard, prompts for the video and music services, and how to
put a film into the game.

The game plays `src/OpenGoldBox/godot/endings/<FOLDER>.ogv` (Theora video,
Vorbis audio), with FOLDER one of `POOLRAD`, `CURSE`, `SECRET`, `DARKNESS`,
`GATEWAY`, `TREASURE`, `UA`. If the file is missing, the game shows only its
closing words ("You have won.").

## Shared look and method

**The heroes never show a face.** AI video can't keep the same six people
from one shot to the next, so the party only ever appears in silhouette,
from behind, or in deep shadow against a bright light (fire, a spell, the
sun, a cheering street). Faces belong to the foe and the other people only.

**One look for all seven films.** Every prompt ends with the same style
words, changing only the palette:

> painted epic-fantasy realism in the manner of a late-1980s fantasy book
> cover, 35mm anamorphic lens, shallow depth of field, soft film grain,
> strong golden rim light, deep blue-violet shadows, the heroes only as dark
> silhouettes seen from behind, 24 fps, cinematic, no text

**The party.** It's the same six heroes in every film, dressed for each
game's setting. Keep their outlines fixed, because outlines are all the
viewer sees:

1. **Fighter**: tall and broad, a crested helm, a kite shield on the left
   arm and a longsword.
2. **Cleric**: a plate breastplate under a tabard, a flanged mace raised high
   and a round holy symbol on a chain.
3. **Magic-user**: slim, in a long hooded robe, holding a tall staff topped
   with a glowing crystal.
4. **Thief**: small and lithe, in a short hooded cloak, with a dagger in each
   hand.
5. **Elf ranger**: tall and slender, with long loose hair, a longbow and a
   quiver on the back.
6. **Dwarf fighter**: half the height of the others, very broad, a horned
   helm, a braided beard showing at the sides and a double-bitted axe.

For Runway Gen-4 (and Kling's image-to-video), first make one reference
still per game: the six from behind, in a line, in that game's clothes,
backlit. Feed it to every shot that shows the party.

**Which service for which shot.** Veo 3 is the default, for wide shots,
the crowd scenes and atmosphere. Kling is for the action shots (blows,
spells, the foe falling). Runway Gen-4 is for shots that must match the
reference still. Sora is a fallback for any of these. Generate clips of 5 to
8 seconds and cut each one down to the length the storyboard gives. Veo 3
adds its own sound: strip it (`-an` below), because the music replaces it.

**Assembly.** Use 4 or 5 shots, cut together with half-second cross-fades.
The story's ending always comes last and lasts longest. End on a fade to
black as the music resolves. First trim each clip to its shot length and
bring it to one size and frame rate:

```sh
ffmpeg -ss 1 -i raw1.mp4 -t 4.5 -vf "scale=1920:1080,fps=24" -an s1.mp4
```

Then join the five shots (four of 4.5 s, the last of 5 s), add the music
and fade out at the end. The offset for the cross-fade into shot k is
4 × (k − 1):

```sh
ffmpeg -i s1.mp4 -i s2.mp4 -i s3.mp4 -i s4.mp4 -i s5.mp4 -i music.wav \
  -filter_complex "[0:v][1:v]xfade=transition=fade:duration=0.5:offset=4[a];\
[a][2:v]xfade=transition=fade:duration=0.5:offset=8[b];\
[b][3:v]xfade=transition=fade:duration=0.5:offset=12[c];\
[c][4:v]xfade=transition=fade:duration=0.5:offset=16,fade=t=out:st=20:d=1,format=yuv420p[v];\
[5:a]afade=t=out:st=19:d=2[m]" \
  -map "[v]" -map "[m]" -t 21 film.mp4
```

DaVinci Resolve (free) does the same by hand, if that is easier.

**Music.** Make an original instrumental with Suno (or a similar tool).
Suno usually makes longer tracks, so take the best 20 to 25 seconds, with
the turn from battle to triumph lined up with the cut to the crowd. Every
music prompt below asks for that arc. Name no composers or film scores in
the prompts.

**Into the game.** Convert the film and place it:

```sh
ffmpeg -i film.mp4 -c:v libtheora -q:v 7 -c:a libvorbis -q:a 4 SECRET.ogv
```

The file goes in `src/OpenGoldBox/godot/endings/` under its folder name
(`POOLRAD.ogv`, `CURSE.ogv`, …). `./build.sh` packs it into the app.

**Terms.** Before you rely on any of these services, check its current terms
for commercial use and ownership of the output. That means Veo/Google, Kling,
Runway, Sora/OpenAI and Suno. Free tiers often exclude commercial use.

**Names in prompts.** The storyboards name things plainly. The prompts
describe instead of naming (no Tyranthraxus, Zhentarim, balor or beholder),
because a description works as well and keeps trademarked names out of what
goes to the services.

**Shared negative prompt** (add each game's extra words):

> visible faces of the heroes, hero facial features, text, captions,
> subtitles, watermark, logo, modern clothing, guns, cars, cartoon, anime,
> 3D game render, extra limbs, distorted hands, flicker, morphing bodies

---

## Pool of Radiance — `POOLRAD`

### Finale facts

- **Foe:** Tyranthraxus the Flamed One, a fire spirit that possesses bodies.
  He holds court by the Pool of Radiance in the two-storey inner tower of
  Valjevo Castle, which stands inside a maze of poisonous hedges, with his
  guards around him. (*Cluebook.pdf*, Valjevo Castle; *Adventurers
  Journal.pdf*, the entries on the spirit of fire.) In the game itself he
  fights in a bronze dragon's body, which the docs don't describe.
- **Welcome:** the party marches back to the civilized part of Phlan. At
  City Hall the council thanks them and gives them the recovered Crown
  Jewels of ancient Phlan. (*Cluebook.pdf*, Finale.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Low and wide, from behind the party, slow push-in | A dark tower chamber. The pool blazes with white-gold light. A huge bronze-scaled dragon, wreathed in flame, rears up over it. The six heroes stand black against the glare. |
| 2 | 4.5 s | Over the fighter's shoulder, handheld | Fire washes over the raised shield. The magic-user's staff sends a blue-white bolt into the dragon's chest. |
| 3 | 4.5 s | Medium shot, slow motion | The dragon falls into the pool. A shrieking spirit of fire tears out of its body and blows apart into steam, and the pool's light goes out. |
| 4 | 4.5 s | Behind the party, walking | Dawn over a walled harbor city, half ruin and half rebuilt. Townsfolk spill out of the gate toward the heroes. |
| 5 | 5 s | Crane up from behind the party | The steps of City Hall and a packed square: cheering, petals in the air, banners. Robed councillors hold out an open chest of jewels. The sun rises behind the hall. |

### Prompts

1. *(Veo 3)* Inside a dark stone tower chamber, a pool blazes with white-gold
   light. A colossal bronze-scaled dragon wrapped in living flame rises from
   the pool, its eyes burning. In the foreground six adventurers seen from
   behind stand as black silhouettes against the glare: a fighter with a
   crested helm and kite shield, a robed mage with a glowing staff, a dwarf
   with an axe. The camera pushes in slowly, low and wide. Painted
   epic-fantasy realism in the manner of a late-1980s fantasy book cover,
   35mm anamorphic lens, soft film grain, flame-orange and gold against deep
   blue-violet shadows, heroes only as silhouettes, cinematic.
2. *(Kling)* Over the shoulder of an armored fighter, seen from behind in
   silhouette, as he raises a kite shield into a roaring wave of dragon
   fire. Sparks stream past the shield's edge. Beside him a hooded mage,
   also in silhouette, thrusts a staff forward and a bolt of blue-white
   light strikes the bronze dragon's chest. Handheld, intense, painted
   epic-fantasy realism, 35mm anamorphic, film grain, flame-orange and gold
   against blue-violet shadow, cinematic.
3. *(Kling)* Slow motion: a wounded bronze dragon collapses into a glowing
   pool, sending up a great wave. A shrieking spirit made of fire tears free
   of its body, twists in the air and bursts into steam. The pool's light
   gutters and dies, leaving the stone chamber dim. Painted epic-fantasy
   realism, 35mm anamorphic lens, film grain, orange fire fading to cool
   blue darkness, cinematic.
4. *(Veo 3)* Dawn over a walled medieval harbor city on a northern sea, its
   walls half ruined and half rebuilt with fresh timber scaffolding. Six
   adventurers walk toward the open gate, seen only from behind as dark
   silhouettes against the rising sun. Townsfolk in simple wool clothes run
   out of the gate toward them, waving. The camera tracks behind the party.
   Painted epic-fantasy realism, 35mm anamorphic, film grain, warm gold
   sunrise against blue shadow, cinematic.
5. *(Veo 3)* A packed town square in front of a stone city hall at sunrise.
   A joyful crowd cheers, throwing flower petals, and banners wave from the
   windows. Robed elderly councillors on the steps hold out an open chest of
   glittering crown jewels. In the foreground six adventurers seen from
   behind as silhouettes climb the steps. The camera cranes slowly up and
   back over them. Painted epic-fantasy realism, 35mm anamorphic, soft film
   grain, golden backlight, celebratory, cinematic.

**Negative:** shared line, plus: green dragon, modern buildings, snow.

### Party

Fighter, cleric, magic-user, thief, elf ranger and dwarf, as in the shared
list, in plain early-adventure gear: dented steel, wool cloaks, worn leather.
Rim-lit by fire in the lair and by the sun in the city.

### Music (Suno)

> Original instrumental fantasy orchestral cue, 22 seconds. Opens dark and
> urgent with low brass, pounding timpani and tremolo strings for the fight
> with a fire dragon. At about 10 seconds it breaks into a bright, heroic
> brass fanfare, joined by church bells and a full string theme, for a
> city's welcome at sunrise. Ends on a warm, held major chord. No vocals.

---

## Curse of the Azure Bonds — `CURSE`

### Finale facts

- **Foe:** Tyranthraxus again, the champion of the New Alliance. He fights
  in the chapel of an ancient temple of Bane in the ruins of Myth Drannor,
  with high priests and margoyles at his side, and casts a lightning bolt
  every round. The party destroys the Pool of Radiance with the Gauntlet of
  Moander, he dies the true death, and the last azure sigil vanishes from
  their forearms. (*Cluebook.pdf*, introduction and Temple map, "Final
  Battle".) In the game itself his body here is a giant's.
- **Welcome:** the docs give no celebration. This film uses Tilverton, the
  town where the party's story begins. That is our choice, not the
  original's. (*Cluebook.pdf*, Tilverton.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Wide, from behind the party | A ruined elven temple overgrown with vines, now a black chapel. A towering giant with burning eyes hurls lightning, and winged stone creatures swoop down. |
| 2 | 4.5 s | Tight, behind a raised fist | A hero in silhouette drives a grotesque, slime-dark gauntlet into a glowing pool. Light erupts. |
| 3 | 4.5 s | Close on a forearm, side-on, the face out of frame | Five glowing blue sigils on the forearm fade one by one. Behind it, the giant crumbles into ash. |
| 4 | 4.5 s | Behind the party, walking | A forest road opens on a small walled town of timber houses. Townsfolk run to meet the heroes. |
| 5 | 5 s | Slow push from behind the party | The market square is full: bells ringing, children on shoulders, petals, raised tankards. Low afternoon sun. |

### Prompts

1. *(Kling)* Inside a vast ruined elven temple overgrown with vines and
   moss, turned into a dark chapel with black banners. A towering giant
   with glowing eyes hurls a bolt of lightning, and winged stone gargoyle
   creatures swoop from the rafters. In the foreground six adventurers seen
   from behind as black silhouettes brace behind shields. Wide shot. Painted
   epic-fantasy realism in the manner of a late-1980s fantasy book cover,
   35mm anamorphic, film grain, electric blue-white lightning against deep
   green-black shadow, cinematic.
2. *(Kling)* A tight shot from behind a hero's shoulder, the hero only a
   dark silhouette. A grotesque, dark, slime-coated gauntlet slams down into
   a glowing pool set in the temple floor. A blinding column of white light
   erupts and cracks spread across the stone. Slow motion, painted
   epic-fantasy realism, 35mm anamorphic, film grain, white light against
   green-black shadow, cinematic.
3. *(Runway Gen-4, reference: party still)* Close-up of a warrior's bare
   forearm, side view, the face out of frame. Five glowing azure runes on
   the skin fade away one by one, like embers cooling. Out of focus behind
   it, a towering giant crumbles into drifting ash. Painted epic-fantasy
   realism, 35mm anamorphic, shallow depth of field, film grain, azure glow
   fading to warm light, cinematic.
4. *(Veo 3)* A forest road opens onto a small walled town of timber-framed
   houses in late afternoon. Six adventurers walk toward the gate, seen
   only from behind as silhouettes against the low sun. Townsfolk run out
   along the road to greet them, waving. Tracking shot. Painted epic-fantasy
   realism, 35mm anamorphic, film grain, warm amber light against blue
   shadow, cinematic.
5. *(Veo 3)* A crowded market square in a timber-framed town. Bells ring,
   children ride on their parents' shoulders, flower petals fly, and people
   raise tankards and cheer. In the foreground six adventurers seen from
   behind as dark silhouettes walk into the crowd. A slow push-in. Painted
   epic-fantasy realism, 35mm anamorphic, film grain, golden afternoon
   backlight, joyful, cinematic.

**Negative:** shared line, plus: visible faces on the forearm shot,
tattoos other than the glowing runes.

### Party

The shared six, travel-worn and in darker leathers and cloaks. In shot 3,
blue sigils glow on the forearm. Rim-lit by lightning in the temple and by
amber sun in the town.

### Music (Suno)

> Original instrumental fantasy orchestral cue, 22 seconds. Begins ominous
> and mysterious with a low choir pad (no words), dark strings and rolling
> thunder-like timpani. A great cymbal swell marks a release at about 9
> seconds; the music lifts into a soaring, freed melody on solo horn, then
> full orchestra with bright bells and a festive folk drum for a small
> town's celebration. Ends warm and resolved. No vocals.

---

## Secret of the Silver Blades — `SECRET`

### Finale facts

- **Foe:** the Dreadlord, the lich that the wizard Eldamar became. He
  wakes in his Sanctum in the Castle of the Twins, frozen inside a glacier,
  with ten storm giants and six Banite priests guarding him. Once he falls,
  the party shatters the gem that holds Eldamar's soul. Eldamar and his
  brother Oswulf rise to Tyr together, the glacier vanishes, and the threat
  to New Verdigris is over. (*Cluebook.pdf*, synopsis and "Dreadlord's
  Sanctum".)
- **Welcome:** New Verdigris, a mining boom town whose people called the
  heroes here through the Well of Knowledge: miners and townsfolk.
  (*Cluebook.pdf*, history and New Verdigris.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Wide, low, from behind the party | A throne hall carved from blue glacier ice. A crowned lich in black armor rises in cold green fire, with giants looming behind him. |
| 2 | 4.5 s | Following the fighter from behind | The fighter charges, and a silver blade flashes. The lich's cloak flickers as he blinks to another place, then he is struck down. |
| 3 | 4.5 s | Medium shot, slow motion | A mace shatters a glowing gem on a pedestal. Two figures made of light rise together through the ice ceiling. |
| 4 | 4.5 s | The party in silhouette on a ridge, from behind | Outside at dawn, the glacier collapses into mist and reveals a castle. Far below lies a mining town. |
| 5 | 5 s | Behind the party, walking | The town's muddy main street is lined with miners raising picks and lanterns and townsfolk cheering. Smoke rises from the chimneys. |

### Prompts

1. *(Veo 3)* A throne hall carved from translucent blue glacier ice. A
   gaunt crowned undead sorcerer in black plate armor rises from a frozen
   throne, wreathed in cold green fire, and huge armored giants loom behind
   him. In the foreground six adventurers seen from behind as black
   silhouettes. Low wide shot. Painted epic-fantasy realism in the manner of
   a late-1980s fantasy book cover, 35mm anamorphic, film grain, icy blue
   and sickly green against deep shadow, gold rim light on the heroes,
   cinematic.
2. *(Kling)* An armored fighter seen from behind in silhouette charges
   across an ice floor, and a gleaming silver sword flashes in an arc. The
   undead sorcerer's cloak shimmers as he blinks a step aside, then the
   blade strikes home and he falls in a burst of green sparks. Dynamic
   following shot, painted epic-fantasy realism, 35mm anamorphic, film
   grain, icy blue and green, cinematic.
3. *(Kling)* Slow motion: a flanged mace shatters a glowing violet gem on a
   stone pedestal. The shards scatter like stars. Two tall figures made
   entirely of soft white light rise side by side through a cracking ice
   ceiling toward warm sunlight. Painted epic-fantasy realism, 35mm
   anamorphic, film grain, violet and white light, serene, cinematic.
4. *(Veo 3)* Dawn on a mountain ridge. Six adventurers stand in a line,
   seen only from behind as dark silhouettes. Before them a vast glacier
   melts and collapses into rolling mist, uncovering an old stone castle on
   the mountainside. Far below in the valley lies a small mining town with
   smoking chimneys. Slow push-in. Painted epic-fantasy realism, 35mm
   anamorphic, film grain, pink-gold dawn against cold blue, cinematic.
5. *(Veo 3)* The muddy main street of a frontier mining town with wooden
   buildings and mine-cart rails. Miners in soot-stained clothes raise picks
   and lanterns, and townsfolk cheer and wave caps. Six adventurers seen
   from behind as silhouettes walk up the middle of the street. Tracking
   shot from behind. Painted epic-fantasy realism, 35mm anamorphic, film
   grain, warm lantern and morning light, joyful, cinematic.

**Negative:** shared line, plus: skeleton face close-ups, summer greenery.

### Party

The shared six in fur-lined cloaks and hoods over their armor, with frost on
their shoulders. The fighter's sword is bright silver. Rim-lit icy blue in
the hall and pink-gold at dawn.

### Music (Suno)

> Original instrumental fantasy orchestral cue, 22 seconds. Opens cold and
> eerie: glassy celesta, high icy strings, low male choir hum (no words)
> and slow heavy drums. At about 9 seconds a shimmering harp glissando
> marks release, then a warm, uplifting theme on French horns and strings,
> with a rustic fiddle and hand drums for a mining town's cheer. Ends
> bright and resolved. No vocals.

---

## Pools of Darkness — `DARKNESS`

### Finale facts

- **Foe:** Gothmenes, a balor and Bane's chief ally. He waits in his palace
  in Dark Phlan, Bane's evil copy of Phlan in the Dark Dimension, holding
  Elminster in his whip. Minions rise from the Pool of Death to fight the
  party. Once he is beaten, Bane's crystal crumbles to sand, the stolen
  cities return, and the party wakes healed on the peaceful streets of
  Phlan. (*Cluebook.pdf*, story synopsis and "The Palace of Gothmenes",
  Finale.)
- **Ending:** the Realms are whole again, and no one but the heroes
  remembers what happened: Phlan goes about a peaceful morning, unaware it
  was ever taken. No crowd: the film ends on the heroes alone knowing what
  they saved. (*Cluebook.pdf*.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Wide, from behind the party | A black palace under a dead violet sky. A huge winged demon of fire and smoke cracks a flaming whip over a black pool. Creatures crawl out of the pool. |
| 2 | 4.5 s | Behind the party, charging | An old wizard pulls free of the whip and throws light at the demon as the heroes charge. |
| 3 | 4.5 s | Medium shot, slow motion | A dark crystal cracks and pours away as black sand. The demon roars and dissolves into ash. The violet sky tears open to sunlight. |
| 4 | 4.5 s | Behind the party | White light fades to reveal a harbor city street at dawn, whole again, with gulls overhead. |
| 5 | 5 s | Slow crane up from behind the party | An ordinary morning: townsfolk open shutters and market stalls, unaware. The heroes stand on the harbor wall watching the sun rise over the water, the only ones who remember. |

### Prompts

1. *(Kling)* A colossal black palace hall under a dead violet sky. A huge
   winged demon made of fire and smoke cracks a whip of flame over a dark,
   oily pool, and twisted creatures crawl out of the pool. In the foreground
   six adventurers seen from behind as black silhouettes raise weapons.
   Wide shot. Painted epic-fantasy realism in the manner of a late-1980s
   fantasy book cover, 35mm anamorphic, film grain, infernal orange against
   violet-black, gold rim light, cinematic.
2. *(Kling)* An old white-bearded wizard in grey robes wrenches free of a
   burning whip and hurls a blaze of white light at the fire demon. Six
   adventurers seen from behind as silhouettes charge past him toward the
   demon. Dynamic tracking shot from behind, painted epic-fantasy realism,
   35mm anamorphic, film grain, orange fire and white light against violet
   darkness, cinematic.
3. *(Kling)* Slow motion: a large dark crystal on an altar cracks and pours
   away as black sand. The fire demon roars and comes apart into drifting
   ash. Above, the violet sky tears open and golden sunlight floods in.
   Painted epic-fantasy realism, 35mm anamorphic, film grain, violet giving
   way to gold, cinematic.
4. *(Veo 3)* Brilliant white light fades to reveal a cobbled street in a
   walled harbor city at dawn, peaceful and whole, with gulls wheeling
   overhead and ships' masts beyond the rooftops. Six adventurers stand in
   the street, seen only from behind as silhouettes against the sunrise.
   Painted epic-fantasy realism, 35mm anamorphic, film grain, soft gold
   dawn, cinematic.
5. *(Veo 3)* An ordinary peaceful morning in a walled harbor city:
   townsfolk open shutters and set out market stalls, fishermen ready their
   boats, no one looking up. Six adventurers stand on the harbor wall, seen
   only from behind as silhouettes, watching the sun rise over the sea. The
   camera cranes slowly up and back until the city and the bright sea fill
   the frame. Painted epic-fantasy realism, 35mm anamorphic, film grain,
   golden morning backlight, quiet and bittersweet, cinematic.

**Negative:** shared line, plus: horned devil cartoon, red skin close-up of
the heroes.

### Party

The shared six as veterans at the height of their power: ornate plate,
long cloaks, staves and blades faintly glowing with enchantment. Rim-lit by
demon fire and then by the dawn.

### Music (Suno)

> Original instrumental dark-fantasy orchestral cue, 22 seconds. Starts
> menacing and massive: low brass clusters, pounding war drums, dissonant
> choir pad (no words). At about 10 seconds a sudden silence, a single
> bell, then strings and a solo horn rise into a radiant but quiet theme,
> with harp and soft glockenspiel for a peaceful harbor morning. Ends on a
> warm, held major chord, more peace than triumph. No vocals.

---

## Gateway to the Savage Frontier — `GATEWAY`

### Finale facts

- **Foe:** Vaalgamon, a Zhentarim general. He and his forces wait in
  Ascore, an ancient port city now buried by the Great Desert and haunted
  by its own mummies and skeletons. The party gets past him and sets the
  four statuettes on the altar in Ascore's great plaza, by its pyramids.
  The statuettes, the pyramids and the desert together destroy the
  Zhentarim army, and the party barely escapes. (*Cluebook.pdf*, story
  synopsis and Ascore.)
- **Welcome:** the party returns in triumph to Yartar, the crossroads town
  known as the hub of the Savage Frontier and the place where the story
  begins. (*Cluebook.pdf*, synopsis and Yartar.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Wide, from behind the party | A dead city half buried in sand, with pyramids beyond. A black-cloaked general and his soldiers hold the plaza, and wrapped mummies shamble out of the ruins. |
| 2 | 4.5 s | Following the thief from behind | The thief sprints past the general and sets four small statuettes on a stone altar. |
| 3 | 4.5 s | Very wide | The pyramids flare with light. A towering wall of sandstorm sweeps over the enemy army and swallows it. |
| 4 | 4.5 s | Behind the party, riding | Out of the dust at dusk. A river town's walls and lanterns lie ahead. |
| 5 | 5 s | Slow push from behind the party | A crowded crossroads market at dusk: lanterns, cheering traders and townsfolk, music, raised cups. |

### Prompts

1. *(Veo 3)* A dead ancient city half buried in desert sand, with weathered
   pyramids beyond under a blazing sky. A black-cloaked armored general and
   his soldiers hold a great stone plaza, while linen-wrapped mummies
   shamble out of the ruins. In the foreground six adventurers seen from
   behind as dark silhouettes. Wide shot. Painted epic-fantasy realism in
   the manner of a late-1980s fantasy book cover, 35mm anamorphic, film
   grain, ochre and burnt orange against deep blue shadow, cinematic.
2. *(Kling)* A small hooded thief seen from behind in silhouette sprints
   across a sandy plaza, dodging a black-cloaked general's sword, and sets
   four small carved statuettes onto a stone altar in a row. Fast following
   shot, painted epic-fantasy realism, 35mm anamorphic, film grain, harsh
   desert sun, ochre and blue shadow, cinematic.
3. *(Veo 3)* Very wide shot of a desert: ancient pyramids flare with golden
   light, and a towering wall of sandstorm rises and sweeps across an army
   of dark-armored soldiers, swallowing them whole. Painted epic-fantasy
   realism, 35mm anamorphic, film grain, gold and ochre against storm-dark
   brown, epic, cinematic.
4. *(Veo 3)* At dusk six riders come out of a cloud of drifting desert
   dust, seen only from behind as silhouettes against the setting sun.
   Ahead lie the timber walls and lantern-lit gate of a river town on green
   plains. Tracking shot from behind. Painted epic-fantasy realism, 35mm
   anamorphic, film grain, amber dusk and blue shadow, cinematic.
5. *(Veo 3)* A crowded crossroads market town at dusk, strung with lanterns.
   Traders, river folk and townsfolk cheer, raise cups and wave scarves,
   and musicians play. Six adventurers seen from behind as silhouettes walk
   into the square. Slow push-in from behind. Painted epic-fantasy realism,
   35mm anamorphic, film grain, warm lantern glow, joyful, cinematic.

**Negative:** shared line, plus: Egyptian gods, camels in the battle,
modern desert vehicles.

### Party

The shared six with desert scarves and sun-bleached cloaks over their
armor, sand on their boots. The thief is the lightest and fastest outline.
Rim-lit by hard sun in Ascore and by amber dusk on the road.

### Music (Suno)

> Original instrumental fantasy adventure cue, 22 seconds. Opens tense and
> driving: frame drums, low strings ostinato, a reedy wind instrument over
> a desert drone. At about 9 seconds a roaring orchestral swell like a
> storm, then a joyful heroic theme with brass, strings, hand claps and a
> lively frontier fiddle for a market town's welcome. Ends resolved and
> bright. No vocals.

---

## Treasures of the Savage Frontier — `TREASURE`

### Finale facts

- **Foe:** Freezefire, a legendary dragon that lies mummified in its lair
  at the top of the Ice Peak with a magic gem still in its claw. Black-robed
  spellcasters attack the party there. When the party prises the gem loose,
  the dragon comes back to life, and dwarves fight beside the party against
  it. (*Cluebook.pdf*, Ice Peak, Stage 4: Freezefire's Lair.)
- **Welcome:** the dwarves carry the party down the mountain to Icewolf, a
  village of the Ice Hunters. There the heroes are healed and named the
  Heroes of the North, and the celebration begins. (*Cluebook.pdf*,
  Stage 4.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Wide, from behind the party | An ice cavern. A shrivelled white dragon lies dead with a glowing gem in its claw. Black-robed spellcasters chant around it. |
| 2 | 4.5 s | Behind the party, handheld | The dragon wakes and breathes frost. Dwarves with axes charge beside the heroes. |
| 3 | 4.5 s | Medium shot, slow motion | The dragon crashes down, ice shatters, and the gem's light settles. |
| 4 | 4.5 s | Behind the procession | The dwarves carry the heroes down a snowy mountainside on raised shields, by torchlight. |
| 5 | 5 s | Slow push past bonfires, the heroes from behind | A northern village at dusk: bonfires, horns blowing, fur-clad villagers and dwarves cheering. |

### Prompts

1. *(Veo 3)* A vast cavern of blue ice. A huge white dragon lies dead and
   shrivelled like a mummy, a glowing pale-blue gem still clutched in one
   claw. Black-robed spellcasters stand around it chanting, their hands
   glowing. In the foreground six adventurers seen from behind as dark
   silhouettes. Wide shot. Painted epic-fantasy realism in the manner of a
   late-1980s fantasy book cover, 35mm anamorphic, film grain, icy blue and
   white against black, gold rim light on the heroes, cinematic.
2. *(Kling)* The shrivelled white dragon snaps back to life, rears up and
   breathes a blast of frost. Bearded dwarves with axes and round shields
   charge alongside six adventurers seen from behind as silhouettes.
   Handheld, intense, painted epic-fantasy realism, 35mm anamorphic, film
   grain, icy blue and white, cinematic.
3. *(Kling)* Slow motion: the white dragon crashes onto the cavern floor,
   and pillars of ice shatter into glittering shards. A pale-blue gem rolls
   free and its light slowly calms. Painted epic-fantasy realism, 35mm
   anamorphic, film grain, icy blue with warm torchlight creeping in,
   cinematic.
4. *(Veo 3)* At night on a snowy mountainside, a procession of cheering
   dwarves with torches carries six adventurers down the slope on raised
   round shields. The adventurers are seen only from behind as dark
   silhouettes. Tracking shot from behind. Painted epic-fantasy realism,
   35mm anamorphic, film grain, orange torchlight against deep blue snow,
   cinematic.
5. *(Veo 3)* A northern village of log longhouses at dusk in the snow.
   Bonfires blaze, villagers in furs blow long horns and cheer, and dwarves
   raise tankards. Six adventurers seen from behind as silhouettes stand
   before the largest bonfire. A slow push-in past the flames. Painted
   epic-fantasy realism, 35mm anamorphic, film grain, firelight gold against
   blue twilight, joyful, cinematic.

**Negative:** shared line, plus: green or red dragon, summer, tropical
plants.

### Party

The shared six in heavy furs and snow-crusted cloaks, with frost on their
helms and hoods. The dwarf hero stands out among the dwarf allies by his
horned helm. Rim-lit icy blue in the lair and by torch and bonfire after.

### Music (Suno)

> Original instrumental Nordic-flavoured fantasy cue, 22 seconds. Begins
> cold and fierce: deep frame drums, low droning strings, a solo
> hardanger-style fiddle, a distant war horn. At about 9 seconds a huge
> drum hit and swell into a proud, warm anthem with brass, strings,
> stomping drums and folk fiddle for a village feast around bonfires. Ends
> on a ringing horn and held chord. No vocals.

---

## Unlimited Adventures — `UA`

### Finale facts

- Unlimited Adventures is a construction kit, so it has no fixed foe,
  place or town. Every module made with it shares this one film. It must fit
  any adventure: an unnamed dark power in a ruined hall, then a generic
  walled town. Keep the foe a vague, towering shadow so it never
  contradicts a module's own villain. (*UA/Manual.pdf*, the kit's purpose.)

### Storyboard (≈21 s)

| # | Length | Camera | Action |
|---|---|---|---|
| 1 | 4.5 s | Wide, from behind the party | A ruined, torchlit stone hall. A towering horned shape made of shadow, its form never clear, rises with ember eyes. |
| 2 | 4.5 s | Behind the party, charging | Blades and a spell of light strike the shadow. |
| 3 | 4.5 s | Medium shot, slow motion | The shadow breaks apart into smoke and embers, and dawn light pours through the broken roof. |
| 4 | 4.5 s | Behind the party, walking | A road at sunrise leads to a walled town whose gates swing open. |
| 5 | 5 s | Crane up from behind the party | The town square full of cheering people, banners, petals and bells. |

### Prompts

1. *(Veo 3)* A ruined stone hall lit by guttering torches. A towering horned
   shape made of living shadow rises, its form never clear, with two ember
   eyes. In the foreground six adventurers seen from behind as black
   silhouettes ready their weapons. Wide shot. Painted epic-fantasy realism
   in the manner of a late-1980s fantasy book cover, 35mm anamorphic, film
   grain, torch-gold against deep blue-violet shadow, cinematic.
2. *(Kling)* Six adventurers seen from behind as silhouettes charge a
   towering shadow creature. Swords flash, and a mage's staff hurls a beam
   of white light into its chest. Dynamic tracking shot from behind,
   painted epic-fantasy realism, 35mm anamorphic, film grain, torch-gold and
   white light against blue-violet shadow, cinematic.
3. *(Kling)* Slow motion: the towering shadow creature bursts apart into
   swirling smoke and glowing embers, and golden dawn light pours through
   the hall's broken roof onto the six silhouetted adventurers. Painted
   epic-fantasy realism, 35mm anamorphic, film grain, shadow giving way to
   gold, cinematic.
4. *(Veo 3)* A country road at sunrise leading to a walled medieval town.
   The town's great wooden gates swing open and people stream out. Six
   adventurers walk toward the gate, seen only from behind as silhouettes
   against the sun. Tracking shot from behind. Painted epic-fantasy realism,
   35mm anamorphic, film grain, warm gold sunrise and blue shadow,
   cinematic.
5. *(Veo 3)* A crowded medieval town square: people cheering, banners
   waving from every window, flower petals in the air, bells ringing. Six
   adventurers seen from behind as silhouettes walk into the square. The
   camera cranes up and back. Painted epic-fantasy realism, 35mm
   anamorphic, film grain, golden morning backlight, joyful, cinematic.

**Negative:** shared line, plus: a clearly recognisable monster species,
specific heraldry or coats of arms.

### Party

The shared six exactly as listed, in neutral classic adventuring gear with
no game-specific dress, so any module's party can see itself in them.

### Music (Suno)

> Original instrumental classic fantasy orchestral cue, 22 seconds. Opens
> dark and suspenseful with low strings, timpani rolls and a brooding horn
> motif. At about 10 seconds it turns into a bright heroic fanfare with
> full brass, soaring strings, snare and bells for a town's welcome. Ends
> on a confident, held major chord. No vocals.
