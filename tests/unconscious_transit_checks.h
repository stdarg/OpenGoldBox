namespace unconscious_transit
{
Encounter corridor()
{
    Battlefield board{6, 3, std::vector<std::uint8_t>(18, 1)};
    for (int x = 0; x < 6; ++x)
        board.terrain[6 + x] = 0;
    return
    {
        board,
        {   {1, "vanguard", "Mover", 0, {0, 1}},
            {   2, "bandit", "Unconscious enemy", 1, {2, 1}, {},
                VitalState{0, false, creature_resources(0, 0, 0, true)}
            },
            {3, "bandit", "Guard", 1, {5, 1}}
        }};
}

void run()
{
    auto module = srd5::load(pack());
    for (bool reverse :
            {
                false, true
            })
        for (bool difficult :
                {
                    false, true
                })
            for (bool allied :
                    {
                        false, true
                    })
            {
                auto e = corridor();
                if (reverse)
                    for (auto &p : e.participants)
                        p.side = 1 - p.side;
                if (allied)
                    e.participants[1].side = e.participants[0].side;
                if (difficult)
                    e.battlefield.terrain[8] = 2;
                auto session = hero_first(*module, e);
                const auto before = unit(*session, 1);
                auto saved = session->save();
                check(!session->submit({session->snapshot().revision, 1, 0, "move", {2, 1}}) &&
                      session->save() == saved,
                      "Cannot voluntarily stop on an Unconscious creature");
                check(session->submit(command(*session, "move", {3, 1})),
                      "Both sides cross an Unconscious enemy");
                check(!session->snapshot().reaction_pending && unit(*session, 1).cell == Cell{3, 1},
                      "Incapacitated occupant never offers an opportunity attack");
                check(unit(*session, 1).movement_feet ==
                      before.movement_feet - (allied && !difficult ? 15 : 20),
                      "Unconscious enemy space is Difficult Terrain once; allies use ground cost");
                check(unit(*session, 1).action && unit(*session, 1).bonus_action,
                      "Transit spends no action budgets");
                check(module->restore(session->save())->save() == session->save(),
                      "Completed unconscious transit persists exactly");
                e.participants[1].state = VitalState{1};
                session = hero_first(*module, e);
                if (!allied)
                {
                    saved = session->save();
                    check(!session->submit({session->snapshot().revision, 1, 0, "move", {3, 1}}) &&
                          saved == session->save(),
                          "Awake enemy blocks route atomically");
                }
            }
    // Actual damage changes a block into traversable space without removing the creature.
    auto e = corridor();
    for (auto &participant : e.participants)
        participant.side = 1 - participant.side;
    e.participants[0].cell = {1, 1};
    e.participants[1].state = VitalState{1};
    bool downed = false;
    for (unsigned seed = 0; seed < 100 && !downed; ++seed)
    {
        auto session = module->create(e, seed);
        if (session->snapshot().actor != 1)
            continue;
        check(session->submit(command(*session, "melee")), "Attack blocker");
        if (unit(*session, 2).hit_points || unit(*session, 2).dead)
            continue;
        check(session->submit(command(*session, "move", {3, 1})) &&
              unit(*session, 1).cell == Cell{3, 1},
              "Actual knockout opens enemy transit");
        downed = true;
    }
    check(downed, "Exercise a live knockout rather than only a synthetic state");
    // Interrupt after the paid prefix while sharing the unconscious enemy's square.
    e = corridor();
    e.battlefield.terrain[0] = e.battlefield.terrain[1] = e.battlefield.terrain[2] = 0;
    e.participants[2].cell = {1, 0};
    e.participants.push_back({4, "healer", "Friendly healer", 0, {0, 0}});
    e.participants.push_back({5, "bandit", "Upper guard", 1, {2, 0}});
    for (const auto response :
            {"decline", "opportunity"
            })
    {
        auto session = hero_first(*module, e);
        check(session->submit(command(*session, "move", {4, 1})), "Start crossing");
        check(session->snapshot().reaction_pending && unit(*session, 1).cell == Cell{2, 1} &&
              unit(*session, 1).movement_feet == 15,
              "Reaction pauses on enemy square after paying its ten-foot cost");
        auto copy = module->restore(session->save());
        const auto reaction = command(*session, response);
        check(session->submit(reaction) && copy->submit(reaction) &&
              session->save() == copy->save(),
              "Suspended crossing resumes identically");
        while (session->snapshot().reaction_pending)
        {
            auto decline = command(*session, "decline");
            check(session->submit(decline) && copy->submit(decline) &&
                  session->save() == copy->save(),
                  "Later route reaction preserves continuation");
        }
        check(unit(*session, 1).cell == Cell{4, 1} && unit(*session, 1).movement_feet == 5,
              "Resume charges only remaining suffix");
    }
    e.participants[0].state = VitalState{1, false, creature_resources(1)};
    bool healed = false, died = false, natural_recovery = false;
    for (unsigned seed = 0; seed < 500 && !(healed && died && natural_recovery); ++seed)
    {
        auto session = module->create(e, seed);
        if (session->snapshot().actor != 1)
            continue;
        check(session->submit(command(*session, "move", {4, 1})), "Start low-HP crossing");
        check(session->submit(command(*session, "opportunity")), "Resolve interruption");
        if (unit(*session, 1).hit_points || unit(*session, 1).dead)
            continue;
        check(unit(*session, 1).cell == unit(*session, 2).cell &&
              !session->snapshot().reaction_pending,
              "Knockout preserves involuntary enemy overlap");
        auto waiting = module->restore(session->save());
        for (unsigned turns = 0;
                turns < 24 && !unit(*waiting, 1).dead && unit(*waiting, 1).hit_points == 0; ++turns)
        {
            check(waiting->submit(command(*waiting, "end")),
                  "Advance death saves during enemy overlap");
            check(module->restore(waiting->save())->save() == waiting->save(),
                  "Death/stability/recovery retains valid enemy-overlap continuation");
        }
        died |= unit(*waiting, 1).dead;
        natural_recovery |= unit(*waiting, 1).hit_points > 0;
        if (healed)
            continue;
        auto copy = module->restore(session->save());
        for (unsigned n = 0; session->snapshot().actor != 4; ++n)
        {
            check(n < 5, "Healer gets a turn");
            auto end = command(*session, "end");
            check(session->submit(end) && copy->submit(end),
                  "Advance exact copies to healing turn");
        }
        auto approach = command(*session, "move", {1, 1});
        check(session->submit(approach) && copy->submit(approach),
              "Healer approaches interrupted mover");
        auto heal = command(*session, "cure_wounds");
        heal.target = 1;
        check(session->submit(heal) && copy->submit(heal) && session->save() == copy->save(),
              "Healing preserves overlapping enemy position and RNG");
        check(unit(*session, 1).hit_points > 0 && unit(*session, 1).cell == unit(*session, 2).cell,
              "Recovery does not teleport the mover");
        while (session->snapshot().actor != 1)
            check(session->submit(command(*session, "end")), "Return to mover turn");
        copy = module->restore(session->save());
        auto move = command(*session, "move", {3, 1});
        check(session->submit(move) && copy->submit(move), "Exit involuntary overlap");
        if (session->snapshot().reaction_pending)
        {
            auto decline = command(*session, "decline");
            check(session->submit(decline) && copy->submit(decline), "Resolve exit reaction");
        }
        check(session->save() == copy->save() && unit(*session, 1).cell != unit(*session, 2).cell,
              "Recovered mover exits with exact continuation");
        check(module->restore(session->save())->save() == session->save(),
              "Cleared overlap marker validates");
        healed = true;
    }
    check(healed, "Actual healing and exit exercised");
    check(died, "Actual death during enemy overlap exercised");
    check(natural_recovery, "Actual natural recovery during enemy overlap exercised");
}

} // namespace unconscious_transit
