#include "opengold/creature_catalog.h"

namespace opengold::por
{

EffectDefinition describe_effect(std::uint8_t code)
{
    // Pool-specific identifiers from GBC's effects-per-game format research.
    // Deliberately not the superficially similar FRUA/Curse enumeration.
    EffectDefinition e;
    e.code = code;
    const auto set = [&](std::string_view name, EffectKind kind, std::string_view subject = {})
    {
        e.name = name;
        e.kind = kind;
        e.subject = subject;
    };
    using enum EffectKind;
    switch (code)
    {
    case 0x00:
        set("None", none);
        break;
    case 0x01:
        set("Bless", spell);
        break;
    case 0x02:
        set("Curse", spell);
        break;
    case 0x03:
        set("Sword versus undead", other, "undead");
        break;
    case 0x04:
        set("Studying manual of bodily health", other);
        break;
    case 0x05:
        set("Detect magic", spell);
        break;
    case 0x06:
        set("Flame tongue", other);
        break;
    case 0x07:
        set("Training with manual of bodily health", other);
        break;
    case 0x08:
        set("Protection from evil", spell, "evil");
        break;
    case 0x09:
        set("Protection from good", spell, "good");
        break;
    case 0x0a:
        set("Resist cold", resistance, "cold");
        break;
    case 0x0b:
        set("Charmed", condition);
        break;
    case 0x0c:
        set("Enlarge", spell);
        break;
    case 0x0d:
        set("Reduce", spell);
        break;
    case 0x0e:
        set("Friends", spell);
        break;
    case 0x0f:
        set("Slow poison", spell, "poison");
        break;
    case 0x10:
        set("Read magic", spell);
        break;
    case 0x11:
        set("Shield", spell);
        break;
    case 0x12:
        set("Gnome attack bonus", racial_bonus);
        break;
    case 0x13:
        set("Find traps", spell);
        break;
    case 0x14:
        set("Resist fire", resistance, "fire");
        break;
    case 0x15:
        set("Silenced", condition);
        break;
    case 0x16:
        set("Slow poison expiring", condition);
        break;
    case 0x17:
        set("Spiritual hammer", spell);
        break;
    case 0x18:
        set("See invisible", spell);
        break;
    case 0x19:
    case 0x47:
        set("Invisible", spell);
        break;
    case 0x1a:
        set("Dwarf attack bonus", racial_bonus);
        break;
    case 0x1b:
        set("Feather fall", spell);
        break;
    case 0x1c:
        set("Mirror image", spell);
        break;
    case 0x1d:
        set("Enfeebled", condition);
        break;
    case 0x1e:
        set("Nauseated", condition);
        break;
    case 0x1f:
        set("Helpless", condition);
        break;
    case 0x20:
        set("Animate dead", spell);
        break;
    case 0x21:
        set("Blind", condition);
        break;
    case 0x22:
        set("Diseased", condition);
        break;
    case 0x23:
    case 0x31:
        set("Prayer", spell);
        break;
    case 0x24:
        set("Accursed", condition);
        break;
    case 0x25:
        set("Blink", spell);
        break;
    case 0x26:
        set("Strength", spell);
        break;
    case 0x27:
        set("Haste", spell);
        break;
    case 0x28:
        set("In stinking cloud", condition);
        break;
    case 0x29:
        set("Protection from normal missiles", immunity, "normal missiles");
        break;
    case 0x2a:
        set("Slow", spell);
        break;
    case 0x2b:
        set("Disease affecting strength", condition);
        break;
    case 0x2c:
        set("Disease affecting hit points", condition);
        break;
    case 0x2d:
        set("Protection from evil, 10 feet", spell, "evil");
        break;
    case 0x2e:
        set("Protection from good, 10 feet", spell, "good");
        break;
    case 0x2f:
        set("Dwarf defense against giants", racial_bonus, "giants");
        break;
    case 0x30:
        set("Gnome defense against large monsters", racial_bonus);
        break;
    case 0x32:
        set("Mummy disease", condition);
        break;
    case 0x33:
        set("Charmed snake", condition);
        break;
    case 0x34:
        set("Held", condition);
        break;
    case 0x35:
        set("Asleep", condition);
        break;
    case 0x36:
        set("Repulsed", condition);
        break;
    case 0x37:
        set("Poisoned", condition);
        break;
    case 0x38:
        set("Ring invisibility", spell);
        break;
    case 0x3a:
        set("Paralyzed", condition);
        break;
    case 0x3b:
    case 0x3e:
        set("Regeneration", regeneration);
        break;
    case 0x3d:
        set("Ring fire resistance", resistance, "fire");
        break;
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x46:
        set("Poison attack", special_attack, "poison");
        e.target_save_adjustment = code == 0x41 ? 4 : code == 0x42 ? 2 : code == 0x46 ? -2 : 0;
        break;
    case 0x43:
    case 0x44:
    case 0x45:
        set(code == 0x44 ? "Paralysis attack (elves immune)" : "Paralysis attack", special_attack,
            "paralysis");
        e.target_save_adjustment = code == 0x45 ? -2 : 0;
        break;
    case 0x48:
        set("Camouflage", other);
        break;
    case 0x49:
        set("Rear claw rake", special_attack);
        break;
    case 0x4c:
        set("Blood drain", special_attack);
        break;
    case 0x4d:
        set("Bite and hold", special_attack);
        break;
    case 0x4f:
        set("Fire touch", special_attack, "fire");
        break;
    case 0x50:
        set("Ankheg acid melee attack", special_attack, "acid");
        break;
    case 0x51:
        set("Dragon fear aura", special_attack, "fear");
        break;
    case 0x52:
        set("Mummy fear aura", special_attack, "fear");
        break;
    case 0x53:
        set("Petrifying gaze", special_attack, "petrification");
        break;
    case 0x54:
        set("Charming gaze", special_attack, "charm");
        break;
    case 0x55:
    case 0x56:
        set("Level drain", special_attack);
        e.levels_drained = code == 0x55 ? 1 : 2;
        break;
    case 0x57:
        set("Disease attack", special_attack, "disease");
        break;
    case 0x58:
        set("Lightning breath", special_attack, "electricity");
        break;
    case 0x59:
        set("Displacement", other);
        break;
    case 0x5a:
        set("Halfling poison save bonus", racial_bonus, "poison");
        break;
    case 0x5b:
        set("Electricity immunity", immunity, "electricity");
        break;
    case 0x5d:
        set("Half fire damage", resistance, "fire");
        e.incoming_damage_multiplier = 0.5;
        break;
    case 0x5e:
        set("Half blunt/piercing damage", resistance, "blunt/piercing weapons");
        e.incoming_damage_multiplier = 0.5;
        break;
    case 0x5f:
        set("Fights at 0 through -6 HP", other);
        break;
    case 0x60:
        set("Requires silver or magic weapons", immunity, "non-silver, non-magical weapons");
        break;
    case 0x61:
        set("Dwarf saving throw bonus", racial_bonus);
        break;
    case 0x62:
        set("Regeneration", regeneration);
        e.regeneration = Regeneration{3, std::nullopt};
        break;
    case 0x63:
        set("Fights while unconscious", other);
        break;
    case 0x64:
        set("Troll fire/acid vulnerability", vulnerability, "fire/acid");
        break;
    case 0x65:
        set("Troll regeneration", regeneration);
        e.regeneration = Regeneration{3, DamageDice{3, 6, 0}};
        break;
    case 0x67:
    case 0x77:
        set("Requires magic weapons", immunity, "non-magical weapons");
        break;
    case 0x68:
        set("Thri-kreen missile evasion", resistance, "missiles");
        break;
    case 0x6a:
        set("Magic resistance", resistance, "magic");
        e.resistance_percent = 100;
        break;
    case 0x6b:
        set("Sleep/charm resistance", resistance, "sleep/charm");
        e.resistance_percent = 90;
        break;
    case 0x6c:
        set("Sleep/charm immunity", immunity, "sleep/charm");
        break;
    case 0x6d:
        set("Hold person/wand paralysis immunity", immunity, "hold person/wand paralysis");
        break;
    case 0x6e:
        set("Cold immunity", immunity, "cold");
        break;
    case 0x6f:
        set("Paralysis/poison immunity", immunity, "paralysis/poison");
        break;
    case 0x70:
        set("Fire immunity", immunity, "fire");
        break;
    case 0x71:
        set("Efreeti fire resistance", resistance, "fire");
        break;
    case 0x72:
        set("Half electricity damage", resistance, "electricity");
        e.incoming_damage_multiplier = 0.5;
        break;
    case 0x73:
        set("Half piercing/slashing damage", resistance, "piercing/slashing weapons");
        e.incoming_damage_multiplier = 0.5;
        break;
    case 0x74:
        set("Half magic weapon damage", resistance, "magical weapons");
        e.incoming_damage_multiplier = 0.5;
        break;
    case 0x75:
        set("Holy water vulnerability", vulnerability, "holy water");
        break;
    case 0x76:
        set("Half cold damage", resistance, "cold");
        e.incoming_damage_multiplier = 0.5;
        break;
    case 0x78:
        set("Boulder evasion", resistance, "boulders");
        break;
    case 0x79:
        set("Ankheg acid squirt", special_attack, "acid");
        break;
    case 0x7a:
        set("Fire vulnerability", vulnerability, "fire");
        break;
    case 0x7b:
        set("Wraith weapon defenses", resistance, "ordinary weapons; silver/magic exceptions");
        break;
    case 0x7c:
        set("Sleep/charm resistance", resistance, "sleep/charm");
        e.resistance_percent = 30;
        break;
    case 0x7d:
        set("Sleep/charm/paralysis/poison immunity", immunity, "sleep/charm/paralysis/poison");
        break;
    case 0x7e:
        set("Gaze immunity", immunity, "gaze");
        break;
    case 0x7f:
        set("Reflectable gaze", vulnerability, "reflected gaze");
        break;
    default:
        break; // Unknown remains unknown, with its original numeric code.
    }
    return e;
}
} // namespace opengold::por
