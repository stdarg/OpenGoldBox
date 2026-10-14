# Reactions to damage

Hellish Rebuke (Warlocks and Infernal Tieflings), Stone's Endurance and
Storm's Thunder (Goliaths) answer damage. The game asks with the React and
Decline buttons; play waits for the answer.

## Who dealt the damage

The SRD's Hellish Rebuke and Storm's Thunder answer damage "from a creature".
The game takes that creature to be the one acting when the damage lands: the
creature whose turn it is, or the one making an Opportunity Attack. So:

- An attack, a spell (Magic Missile's darts, Burning Hands, a save for half),
  Spiritual Weapon, Flaming Sphere and Heat Metal are answered: their caster
  acts.
- Damage a creature takes at the start or end of its own turn (Searing
  Smite's burn, Moonbeam, Acid Arrow) or while moving (Spike Growth) has no
  creature to answer, so neither Reaction is offered.
- Warding Bond's shared damage is answered as damage from the attacker.

Stone's Endurance needs no source: any damage may be lessened, except damage
from a turn that ends outside a command (a creature fled or fell), which
cannot stop play to ask.

## When it is asked

- After an attack: right after its damage and riders (Hex, Divine Favor, a
  Goliath's Fire's Burn), before the next attack of the same command.
- Any other damage: once the command is done, so an area spell finishes before
  anyone answers it. A creature damaged several times in one command is asked
  once.
- Stone's Endurance: before each damage is applied; asked once per command.

Verification: `opengold_warlock_tests` (Magic Missile asks for Hellish Rebuke
once) and `opengold_species_tests` (Stone's Endurance against a dart).
