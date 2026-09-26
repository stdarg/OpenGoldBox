#include "spell_access.h"
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
namespace opengold::srd5::detail {
using namespace rules;
namespace {
constexpr std::string_view source="class:wizard:spellcasting";
// `wizard` marks a row the Wizard catalog may learn. It was an exclusion list
// of non-Wizard spell ids repeated at each use; a per-row property means a new
// spell describes itself in one place.
struct Spell {std::string_view id,label;unsigned level,mask;bool wizard{true};};
// Existing spell implementations only. This is not the complete Wizard list.
constexpr std::array spells{
    Spell{"chill_touch","Chill Touch",0,2048},Spell{"shocking_grasp","Shocking Grasp",0,1024},Spell{"eldritch_blast","Eldritch Blast",0,512,false},Spell{"ray_of_frost","Ray of Frost",0,256},Spell{"sacred_flame","Sacred Flame",0,128,false},Spell{"fire_bolt","Fire Bolt",0,1},Spell{"poison_spray","Poison Spray",0,64},Spell{"magic_missile","Magic Missile",1,4},
    Spell{"scorching_ray","Scorching Ray",2,16},Spell{"blindness","Blindness",2,32},
    Spell{"inflict_wounds","Inflict Wounds",1,0,false}};
void require(bool ok){if(!ok)throw std::runtime_error("Invalid spell grant, spellbook entry or preparation");}
const Spell& find(std::string_view id){
    for(const auto& spell:spells)if(spell.id==id)return spell;
    throw std::runtime_error("Unsupported spell knowledge");
}
FeatureGrant grant(std::string_view id,unsigned level,std::string_view origin=source){return {"spell:"+std::string(id),std::string(origin),level,{{"access",find(id).level?"spellbook":"cantrip"}}};}
}
bool is_spell_grant(const FeatureGrant& grant){return grant.id.starts_with("spell:");}
std::vector<FeatureGrant> without_spell_grants(std::span<const FeatureGrant> grants){
    std::vector<FeatureGrant> result;for(const auto& g:grants)if(!is_spell_grant(g))result.push_back(g);return result;
}
TrainingChoiceGroup starting_cantrip_options(std::string_view klass){
    if(klass=="warlock")return {"class:warlock:pact_magic","Warlock cantrips",2,
        {{"eldritch_blast","Eldritch Blast","Ranged spell attack: 1d10 Force damage, 120 feet; creature targets currently supported."},
         {"poison_spray","Poison Spray","Ranged spell attack: 1d12 Poison damage, 30 feet."},
         {"chill_touch","Chill Touch","Melee spell attack: 1d10 Necrotic damage, Touch; prevents healing until the end of your next turn."}}};
    if(klass=="cleric")return {"class:cleric:spellcasting","Cleric cantrips",3,
        {{"sacred_flame","Sacred Flame","Dexterity save: 1d8 Radiant damage, visible creature within 60 feet."}}};
    if(klass!="wizard"&&klass!="sorcerer")return {};
    return {klass=="sorcerer"?"class:sorcerer:spellcasting":std::string(source),klass=="sorcerer"?"Sorcerer cantrips":"Wizard cantrips",klass=="sorcerer"?4u:3u,
        {{"fire_bolt","Fire Bolt","Ranged spell attack: 1d10 Fire damage, 120 feet."},
         {"poison_spray","Poison Spray","Ranged spell attack: 1d12 Poison damage, 30 feet."},
         {"ray_of_frost","Ray of Frost","Ranged spell attack: 1d8 Cold damage, 60 feet; Speed reduced by 10 feet until your next turn."},
         {"shocking_grasp","Shocking Grasp","Melee spell attack: 1d8 Lightning damage, Touch; prevents Opportunity Attacks until the target’s next turn."},
         {"chill_touch","Chill Touch","Melee spell attack: 1d10 Necrotic damage, Touch; prevents healing until the end of your next turn."}}};
}
std::vector<FeatureGrant> starting_spell_grants(std::string_view klass,const std::optional<std::vector<std::string>>& cantrips){
    if(klass=="sorcerer"){
        std::vector<FeatureGrant> result;std::set<std::string> unique;
        const auto chosen=cantrips.value_or(std::vector<std::string>{});require(chosen.size()<=4);
        for(const auto& id:chosen){
            require((id=="fire_bolt"||id=="poison_spray"||id=="ray_of_frost"||id=="shocking_grasp"||id=="chill_touch")&&unique.insert(id).second);
            result.push_back(grant(id,1,"class:sorcerer:spellcasting"));
        }return result;
    }
    if(klass=="warlock"){
        require(!cantrips||cantrips->size()<=2);
        std::vector<FeatureGrant> result;std::set<std::string> unique;
        for(const auto& id:cantrips.value_or(std::vector<std::string>{})){
            require((id=="eldritch_blast"||id=="poison_spray"||id=="chill_touch")&&unique.insert(id).second);result.push_back(grant(id,1,"class:warlock:pact_magic"));
        }return result;
    }
    if(klass=="cleric"){
        std::vector<FeatureGrant> result;std::set<std::string> unique;
        for(const auto& id:cantrips.value_or(std::vector<std::string>{})){
            require(id=="sacred_flame"&&unique.insert(id).second);result.push_back(grant(id,1,"class:cleric:spellcasting"));
        }return result;
    }
    if(klass!="wizard"){require(!cantrips||cantrips->empty());return {};}
    const auto chosen=cantrips.value_or(std::vector<std::string>{"fire_bolt"});
    require(chosen.size()<=3);std::set<std::string> unique;std::vector<FeatureGrant> result;
    for(const auto& id:chosen){require((id=="fire_bolt"||id=="poison_spray"||id=="ray_of_frost"||id=="shocking_grasp"||id=="chill_touch")&&unique.insert(id).second);result.push_back(grant(id,1));}
    result.push_back(grant("magic_missile",1));return result;
}
SpellAccess spell_access(std::span<const FeatureGrant> grants,std::string_view klass,unsigned level,std::span<const std::string> prepared){
    SpellAccess result;
    if(klass=="Sorcerer"){
        // Starting cantrips only. Leveled spells, replacement and advancement remain #132.
        require(level==1&&prepared.empty());result.cantrip_choices=4;std::set<std::string> known;
        for(const auto& g:grants)if(is_spell_grant(g)){
            require(g.id=="spell:fire_bolt"||g.id=="spell:poison_spray"||g.id=="spell:ray_of_frost"||g.id=="spell:shocking_grasp"||g.id=="spell:chill_touch");
            const auto& spell=find(std::string_view(g.id).substr(6));
            require(g==grant(spell.id,1,"class:sorcerer:spellcasting")&&known.insert(g.id).second);
            result.cantrips.push_back({std::string(spell.id),std::string(spell.label),g.source_id,g.level});
        }require(result.cantrips.size()<=4);return result;
    }
    if(klass=="Warlock"){
        // Cantrip portion of Pact Magic only; slots and advancement remain separate.
        require(level==1&&prepared.empty());result.cantrip_choices=2;std::set<std::string> known;
        for(const auto& g:grants)if(is_spell_grant(g)){
            require(g.id=="spell:eldritch_blast"||g.id=="spell:poison_spray"||g.id=="spell:chill_touch");
            const auto& spell=find(std::string_view(g.id).substr(6));
            require(g==grant(spell.id,1,"class:warlock:pact_magic")&&known.insert(g.id).second);
            result.cantrips.push_back({std::string(spell.id),std::string(spell.label),g.source_id,g.level});
        }require(result.cantrips.size()<=2);return result;
    }
    if(klass=="Cleric"){
        require(level>=1&&level<=4);
        require(std::find(grants.begin(),grants.end(),FeatureGrant{"feature:spellcasting","class:cleric",1,{}})!=grants.end());
        result.cantrip_choices=level==4?4:3;std::set<std::string> known;
        for(const auto& g:grants)if(is_spell_grant(g)){
            require(g.id=="spell:sacred_flame"&&g==grant("sacred_flame",1,"class:cleric:spellcasting")&&known.insert(g.id).second);
            result.cantrips.push_back({"sacred_flame","Sacred Flame",g.source_id,g.level});
        }return result; // Leveled preparation and Divine Order remain their existing increments.
    }
    if(klass!="Wizard"){
        require(std::none_of(grants.begin(),grants.end(),is_spell_grant));
        return result; // Each other class's preparation policy has its own issue.
    }
    require(level>=1&&level<=4);
    require(std::find(grants.begin(),grants.end(),FeatureGrant{"feature:spellcasting","class:wizard",1,{}})!=grants.end());
    result.cantrip_choices=level==4?4:3;result.spellbook_choices=6+2*(level-1);result.prepared_choices=level+3;
    std::set<std::string> known;std::array<unsigned,5> books{},cantrips{};
    for(const auto& g:grants)if(is_spell_grant(g)){
        require(g.source_id==source&&g.level>=1&&g.level<=level&&known.insert(g.id).second);
        const auto& spell=find(std::string_view(g.id).substr(6));require(spell.wizard);
        auto expected=grant(spell.id,g.level);
        unsigned learned=g.level;
        if(const auto replacement=g.choices.find("learned_at");replacement!=g.choices.end()){
            require(spell.level==0);
            bool valid=false;for(unsigned n=g.level;n<=level;++n)if(replacement->second==std::to_string(n)){learned=n;valid=true;}
            require(valid);expected.choices.emplace("learned_at",replacement->second);
        }
        require(g==expected);
        require(spell.level==0||spell.level<=(g.level>=3?2u:1u));
        auto& list=spell.level?result.spellbook:result.cantrips;
        list.push_back({std::string(spell.id),std::string(spell.label),g.source_id,learned});
        if(spell.level)++books[g.level];else ++cantrips[g.level];
    }
    require(books[1]<=6&&cantrips[1]<=3&&cantrips[2]==0&&cantrips[3]==0&&cantrips[4]<=1);
    for(unsigned n=2;n<=level;++n)require(books[n]<=2);
    std::set<std::string> selected;
    for(const auto& id:prepared){
        require(selected.insert(id).second&&std::any_of(result.spellbook.begin(),result.spellbook.end(),[&](const auto& s){return s.id==id;}));
        result.prepared.push_back(id);
    }
    require(result.prepared.size()<=result.prepared_choices);return result;
}
void learn_advancement_spells(CharacterSheet& sheet,std::span<const std::string> selected){
    if(sheet.character_class!="Wizard")return;
    auto next=sheet.grants;
    for(const auto& id:selected){require(find(id).level!=0);
        if(std::none_of(next.begin(),next.end(),[&](const auto& g){return g.id=="spell:"+id;}))next.push_back(grant(id,sheet.level));}
    (void)spell_access(next,sheet.character_class,sheet.level,selected);sheet.grants=std::move(next);
}
SpellChoiceOptions spell_choice_options(const CharacterSheet& sheet,SpellChoiceContext context){
    SpellChoiceOptions result;if(sheet.character_class!="Wizard")return result;
    const auto access=spell_access(sheet.grants,sheet.character_class,sheet.level,sheet.prepared_spells);
    auto known=[&](std::string_view id){return std::any_of(sheet.grants.begin(),sheet.grants.end(),[&](const auto& g){return g.id=="spell:"+std::string(id);});};
    if(context!=SpellChoiceContext::long_rest)for(unsigned level=1;level<=unsigned(sheet.level);++level){
        if(context==SpellChoiceContext::advancement&&level!=unsigned(sheet.level))continue;
        for(bool cantrip:{true,false}){
            const unsigned capacity=cantrip?(level==1?3:level==4?1:0):(level==1?6:2);
            const unsigned used=std::count_if(sheet.grants.begin(),sheet.grants.end(),[&](const auto& g){return is_spell_grant(g)&&g.level==level&&(find(std::string_view(g.id).substr(6)).level==0)==cantrip;});
            if(capacity==used)continue;
            TrainingChoiceGroup group;group.id=std::string(cantrip?"cantrips:":"spellbook:")+std::to_string(level);
            group.label=cantrip?"Wizard cantrips":"Spellbook";group.count=capacity-used;group.acquired_level=level;
            for(const auto& spell:spells)if((spell.level==0)==cantrip&&spell.wizard&&
                spell.level<=(level>=3?2u:1u)&&!known(spell.id))group.options.push_back({std::string(spell.id),std::string(spell.label),{}});
            result.learning.push_back(std::move(group));
        }
    }
    result.prepared_count=access.prepared_choices;
    result.may_prepare=context!=SpellChoiceContext::pending;
    for(const auto& spell:access.spellbook)result.preparation.push_back({spell.id,spell.label,{}});
    if(context==SpellChoiceContext::advancement)result.locked_prepared=sheet.prepared_spells;
    result.may_replace=context==SpellChoiceContext::long_rest;
    if(result.may_replace){
        for(const auto& spell:access.cantrips)result.replaceable.push_back({spell.id,spell.label,{}});
        for(const auto& choice:starting_cantrip_options("wizard").options)if(!known(choice.id))result.replacements.push_back(choice);
    }
    return result;
}
void apply_spell_choices(CharacterSheet& sheet,const SpellChoices& choices,SpellChoiceContext context,bool complete){
    require(sheet.character_class=="Wizard");auto candidate=sheet;
    if(complete&&context==SpellChoiceContext::pending&&std::none_of(choices.learning.begin(),choices.learning.end(),[](const auto& entry){return !entry.second.empty();}))throw std::runtime_error("No supported missing spell choices.");
    const auto options=spell_choice_options(sheet,context);
    for(const auto& [id,values]:choices.learning){
        const auto group=std::find_if(options.learning.begin(),options.learning.end(),[&](const auto& g){return g.id==id;});
        require(group!=options.learning.end()&&values.size()<=group->count);
        for(const auto& value:values){require(std::any_of(group->options.begin(),group->options.end(),[&](const auto& o){return o.id==value;}));candidate.grants.push_back(grant(value,group->acquired_level));}
    }
    require(choices.replace_cantrip.empty()==choices.replacement.empty());
    if(!choices.replace_cantrip.empty()){
        require(options.may_replace&&std::any_of(options.replaceable.begin(),options.replaceable.end(),[&](const auto& o){return o.id==choices.replace_cantrip;})&&
            std::any_of(options.replacements.begin(),options.replacements.end(),[&](const auto& o){return o.id==choices.replacement;}));
        auto existing=std::find_if(candidate.grants.begin(),candidate.grants.end(),[&](const auto& g){return g.id=="spell:"+choices.replace_cantrip;});
        *existing=grant(choices.replacement,existing->level);existing->choices.emplace("learned_at",std::to_string(sheet.level));
    }
    if(choices.prepared){
        require(options.may_prepare);
        for(const auto& id:options.locked_prepared)require(std::find(choices.prepared->begin(),choices.prepared->end(),id)!=choices.prepared->end());
        candidate.prepared_spells=*choices.prepared;
    }
    const auto access=spell_access(candidate.grants,candidate.character_class,candidate.level,candidate.prepared_spells);
    if(complete){
        for(const auto& group:spell_choice_options(candidate,context).learning)if(!group.options.empty()&&group.count)throw std::runtime_error("Complete the available spell choices.");
        if(options.may_prepare)require(candidate.prepared_spells.size()==std::min<std::size_t>(access.prepared_choices,access.spellbook.size()));
    }
    sheet=std::move(candidate);
}
std::vector<std::string> known_cantrip_ids(const SpellAccess& access){
    std::vector<std::string> result;
    // find() still rejects an unsupported id, as the mask lookup used to.
    for(const auto& s:access.cantrips){(void)find(s.id);result.push_back(s.id);}
    return result;
}
std::vector<std::string> wizard_casting_ids(const SpellAccess& access){
    auto result=known_cantrip_ids(access);
    for(const auto& id:access.prepared){(void)find(id);result.push_back(id);}
    return result;
}
}
