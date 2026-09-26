#include "campaign_fixture.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "spell_components.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace opengold;using namespace opengold::rules;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
const auto root=std::filesystem::path(OPENGOLD_SOURCE_DIR);
auto module(){return srd5::load(root/"data/rules/srd-5.2.1/combat.rules");}
std::string read(const std::filesystem::path& p){std::ifstream in(p);check(bool(in),"Frozen fixture exists");return {std::istreambuf_iterator<char>(in),{}};}
void write(const std::filesystem::path& p,const std::string& bytes){std::ofstream out(p);out<<bytes;check(bool(out),"Fixture written");}
Character hero(std::string klass){
    CharacterDraft d;d.race="human";d.gender="female";d.character_class=klass;d.background="sage";d.alignment="neutral_good";d.name="Component tester";d.rolled=true;
    for(auto& r:d.rolls)r={{6,5,4,1},3};Character c(*srd5::character_rules(),d,{});auto rules=module();VitalState vital{c.sheet().hit_points,false,{}};
    for(unsigned level=2;level<=3;++level){auto choice=rules->default_advancement(c.sheet());
        choice.spells=klass=="cleric"?std::vector<std::string>{"cure_wounds","healing_word"}:std::vector<std::string>{"magic_missile"};
        if(level==3){choice.spells.push_back("blindness");if(klass=="wizard")choice.spells.push_back("scorching_ray");}
        check(c.advance(*rules,vital,choice),"Level-three caster prepared through real advancement");
    }return c;
}
CombatantView unit(const CombatSession& c,EntityId id=1){for(const auto& a:c.snapshot().combatants)if(a.id==id)return a;throw std::runtime_error("Missing actor");}
bool has(const CombatSession& c,std::string_view verb){for(const auto& a:c.legal_commands())if(a.verb==verb)return true;return false;}
Command command(const CombatSession& c,std::string_view verb){for(const auto& a:c.legal_commands())if(a.verb==verb)return a;throw std::runtime_error("Missing command: "+std::string(verb));}
auto battle(const RulesModule& rules,const Character& h,const std::vector<std::string>& gear,unsigned hands=0){
    const auto p=rules.character_profile(h.sheet(),gear,{hands});
    auto c=rules.create({{8,8,std::vector<std::uint8_t>(64)},{{1,"campaign-character","Caster",0,{1,1},p.data,VitalState{h.sheet().hit_points-10,false,{}}},{99,"vanguard","Enemy",1,{3,1}}}},2);
    if(c->snapshot().actor!=1)check(c->submit(command(*c,"end")),"Reach the caster turn after armor initiative penalty");
    check(c->snapshot().actor==1,"Caster has its turn");return c;
}
void definitions(){
    namespace detail=opengold::srd5::detail;
    // Components now live on the spell table rather than a parallel array, so
    // every supported spell has them by construction. Asserting a row count
    // here would make adding a spell an edit in this file too.
    for(const auto& spell:detail::spell_table)
        check(detail::spell_components(spell.id)!=nullptr,"Every supported spell has explicit component definitions");
    for(const auto* id:{"chill_touch","ray_of_frost","fire_bolt","poison_spray","sacred_flame","cure_wounds","magic_missile","scorching_ray"}){const auto* s=detail::spell_components(id);check(s&&s->verbal&&s->somatic,"Source spells require Verbal and Somatic components");}
    for(const auto* id:{"healing_word","blindness"}){const auto* s=detail::spell_components(id);check(s&&s->verbal&&!s->somatic,"Source spells require only Verbal components");}
    check(detail::spell_components("cure_wounds_2")==detail::spell_components("cure_wounds")&&detail::spell_components("healing_word_2")==detail::spell_components("healing_word"),"Higher slot forms keep base components");
    check(!detail::spell_components("invented")&&!detail::spell_components("melee"),"Unknown spells and non-spell actions have no inferred components");
}
unsigned slots(const CombatantView& actor,unsigned level){std::istringstream in(actor.persistent.resources);std::string magic;unsigned winds{},first{},second{};in>>magic>>winds>>first>>second;check(bool(in)&&magic=="SRD2","Level-three caster uses stored level-one/two slots");return level==1?first:second;}
void expectations(){auto rules=module();
    // Independent spell-entry oracle: SRD pp.113,122,131,139,146,160.
    for(const auto& klass:{"cleric","wizard"}){
        const auto h=hero(klass);const bool wizard=std::string_view(klass)=="wizard";
        const std::vector<std::string> somatic=wizard?std::vector<std::string>{"fire_bolt","magic_missile","magic_missile_2","scorching_ray"}:std::vector<std::string>{"cure_wounds","cure_wounds_2"};
        for(const auto& gear:std::vector<std::vector<std::string>>{{},{"shield"},{"mace"},{"wand"},{"greatsword"},{"longbow"},{"quarterstaff"},{"mace","shield"},{"wand","shield"},{"quarterstaff","shield"}}){
            const bool blocked=gear.size()==2;
            const auto p=rules->character_profile(h.sheet(),gear);
            check((p.spell_modifiers.find("Somatic components are unavailable")!=std::string::npos)==blocked,"Existing Modifiers text explains equipment blocking");
            auto c=battle(*rules,h,gear);const auto initial=c->save();
            for(const auto& verb:somatic){
                check(has(*c,verb)==!blocked,"Somatic availability must follow actual occupied hands");
                if(blocked){Command forged{c->snapshot().revision,1,wizard?99u:1u,verb};check(!c->submit(forged)&&c->save()==initial,"Unavailable cast changes no slots, action, RNG, HP, grip, shield or time");}
                else {auto cast=rules->restore(initial);const auto before=unit(*cast);check(cast->submit(command(*cast,verb)),"Eligible Somatic spell resolves normally");
                    check(!unit(*cast).action&&unit(*cast).bonus_action==before.bonus_action&&unit(*cast).equipment==before.equipment&&unit(*cast).armor_class==before.armor_class,"Casting spends Action without changing grip or shield AC");
                    const unsigned spent=verb=="fire_bolt"?0:verb.ends_with("_2")||verb=="scorching_ray"?2:1;
                    for(unsigned level:{1u,2u})check(slots(unit(*cast),level)==slots(before,level)-(spent==level?1:0),"Somatic casting spends exactly its chosen slot; cantrip spends none");
                    check(rules->restore(cast->save())->save()==cast->save(),"Resolved cast round trips exactly");}
            }
            check(has(*c,"blindness"),"Verbal-only Blindness remains available with occupied hands");
            if(!wizard)for(const auto& verb:{"healing_word","healing_word_2"}){
                auto cast=rules->restore(initial);const auto before=unit(*cast);check(cast->submit(command(*cast,verb)),"Verbal-only Healing Word casts with occupied hands");
                check(unit(*cast).action&&!unit(*cast).bonus_action&&unit(*cast).hit_points>before.hit_points&&unit(*cast).equipment==before.equipment,"Healing Word spends Bonus Action and heals while retaining the attack grip");
                const unsigned spent=std::string_view(verb).ends_with("_2")?2:1;for(unsigned level:{1u,2u})check(slots(unit(*cast),level)==slots(before,level)-(spent==level?1:0),"Healing Word spends exactly the chosen slot with occupied hands");
            }
            auto blind=rules->restore(initial);check(blind->submit(command(*blind,"blindness"))&&!unit(*blind).action,"Verbal-only level-two spell spends its Action");
        }
        for(unsigned hands:{1u,2u}){auto c=battle(*rules,h,{"quarterstaff"},hands);check(has(*c,somatic.front()),"Versatile attack grip allows a hand for casting");const auto before=unit(*c);check(c->submit(command(*c,somatic.front()))&&unit(*c).equipment==before.equipment,"Casting retains selected Versatile damage grip");}
        auto armored=battle(*rules,h,{"plate"});check(!has(*armored,"blindness")&&!has(*armored,somatic.front())&&!has(*armored,"healing_word"),"Untrained armor still prohibits both Verbal-only and Somatic casting");
    }
}
CampaignParty party(bool npc=false){CampaignParty p(module());auto h=hero("cleric");h.inventory().add("mace","Mace");h.inventory().add("shield","Shield");const auto id=npc?p.recruit("fixture:cleric",std::move(h)):p.add_pc(std::move(h));p.equip(id,1);p.equip(id,2);
    auto state=p.checkpoint();state.roster[0].vitals.hit_points-=10;state.time_minutes=123;state.subminute_milliseconds=456;state.random_state=789;state.roster[0].wealth[3]=37;p.restore(std::move(state));return p;}
auto campaign_battle(const RulesModule& rules,const CampaignParty& p){auto actors=p.participants();actors[0].cell={1,1};actors.push_back({99,"vanguard","Enemy",1,{3,1}});return rules.create({{8,8,std::vector<std::uint8_t>(64)},actors},2);}
void campaign(){auto rules=module();for(bool npc:{false,true}){auto p=party(npc);const auto bytes=encode_campaign(p,nullptr,"components");CampaignParty copy(module());copy.restore(decode_campaign(bytes,*srd5::character_rules(),*rules,"components",nullptr).party);
    check(encode_campaign(copy,nullptr,"components")==bytes,"Equipment, wounds, advancement, resources and clocks survive campaign reload");
    auto blocked=campaign_battle(*rules,copy);check(!has(*blocked,"cure_wounds")&&has(*blocked,"healing_word"),"Normal equipped party encounter uses hand restrictions");
    const auto before=copy.member(1);copy.unequip(1,1);auto free=campaign_battle(*rules,copy);check(has(*free,"cure_wounds")&&copy.member(1).vitals==before.vitals&&copy.profile(1).armor_class==p.profile(1).armor_class,"Unequipping weapon releases a hand without removing shield AC or healing");
    copy.equip(1,1);copy.unequip(1,2);free=campaign_battle(*rules,copy);check(has(*free,"cure_wounds")&&copy.profile(1).armor_class==p.profile(1).armor_class-2,"Unequipping shield releases a hand and removes only shield AC");
}}
std::string upgrade(std::string bytes){const auto at=bytes.find("0.6.20");check(at!=bytes.npos,"Actual prior writer version");bytes.replace(at,6,module()->identity().version);return bytes;}
void legacy(){auto rules=module();const auto base=root/"tests/fixtures";
    const auto bytes=read(base/"campaign-v10-components.ogs");CampaignParty p(module());p.restore(decode_campaign(bytes,*srd5::character_rules(),*rules,"components",nullptr).party);
    const auto saved=encode_campaign(p,nullptr,"components");const auto body=[](const std::string& s){return s.substr(s.find('\n',s.find('\n')+1)+1);};
    check(body(saved)==test::with_background_training_grants(test::with_legacy_cantrip_choices(upgrade(body(bytes)))),"Campaign migration changes only module identity, absent cantrip choices and owed background grants");
    auto c=rules->restore(read(base/"combat-v13-components.save"));check(c->save()==upgrade(read(base/"combat-v13-components.save")),"Prior checkpoint keeps every actor, recipe, resource, queue, RNG and clock");
    check(!has(*c,"cure_wounds")&&has(*c,"healing_word"),"Old equipment acquires corrected spell eligibility on resume");
    check(c->submit(command(*c,"healing_word")),"Prior legal verbal cast accepted");
    check(c->save()==rules->restore(read(base/"combat-v13-components-continued.save"))->save(),"Unrestricted spell exactly matches actual prior-writer continuation");
}
void ui_fixtures(){const auto path=std::filesystem::path(OPENGOLD_BINARY_DIR)/"component-fixtures";std::filesystem::create_directories(path);auto rules=module();
    for(const auto& klass:{"cleric","wizard"})for(bool shield:{false,true}){const auto h=hero(klass);std::vector<std::string> gear{"quarterstaff"};if(shield)gear.push_back("shield");
        write(path/(std::string(klass)+(shield?"-blocked.save":"-free.save")),battle(*rules,h,gear,shield?1:2)->save());}
}
void freeze(){auto rules=module();check(rules->identity().version=="0.6.20","Freeze must use actual prior writer");auto p=party();const auto base=root/"tests/fixtures";write(base/"campaign-v10-components.ogs",encode_campaign(p,nullptr,"components"));auto c=campaign_battle(*rules,p);write(base/"combat-v13-components.save",c->save());check(has(*c,"cure_wounds"),"Prior writer demonstrates missing restriction");check(c->submit(command(*c,"healing_word")),"Prior writer continuation");write(base/"combat-v13-components-continued.save",c->save());}
}
int main(int argc,char**){try{if(argc==2){freeze();return 0;}definitions();expectations();campaign();legacy();ui_fixtures();std::cout<<"Spell component tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
