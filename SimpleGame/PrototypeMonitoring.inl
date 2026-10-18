// Included inside Prototype. Read-only telemetry; no second world simulation.
Monitoring::Publisher monitorPublisher;
std::chrono::steady_clock::time_point nextMonitorUpdate;
unsigned long long monitorSequence = 0;
const std::string monitorSession =
    std::to_string(GetCurrentProcessId()) + "-" + std::to_string(Monitoring::Timestamp());

void PublishMonitoring()
{
    const auto& settings = monitorPublisher.Settings();
    if (!settings.enabled)
    {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now < nextMonitorUpdate)
    {
        return;
    }
    nextMonitorUpdate = now + std::chrono::seconds(1);
    const bool inWorld = !titleScreen && !openingPlaying;
    const char* mode = openingPlaying                                           ? "opening"
                       : titleScreen                                            ? "title"
                       : levelOne.Dead()                                        ? "dead"
                       : (pauseMenu || journal || slotMode != SlotMode::Closed) ? "paused"
                       : (dialoguePending || chapter.choicePending)             ? "dialogue"
                                                                                : "playing";
    const char* regionIds[] = {"ash_village",
                               "outskirts",
                               "ruins",
                               "chapel",
                               "wilderness",
                               "west_market",
                               "south_logging",
                               "north_fields",
                               "east_hunters"};
    const char* regionNames[] = {"잿빛 마을",
                                 "마을 외곽·묘역",
                                 "폐허·감금 시설",
                                 "예배당",
                                 "야외 탐험 구역",
                                 "서쪽 장터",
                                 "남쪽 벌목촌",
                                 "북쪽 경작지",
                                 "동쪽 사냥 거점"};
    auto regionOf = [](WorldPoint p)
    {
        for (int i = 1; i < int(Population::Hubs().size()); ++i)
            if (Distance(p, Population::Hubs()[i]) < 460)
                return i + 4;
        if (World::Village(p.x, p.y))
        {
            return 0;
        }
        if (p.x >= 512 && p.x <= 3500 && p.y >= -450 && p.y <= 400)
        {
            return p.x < 1500 ? 1 : p.x < 2800 ? 2 : 3;
        }
        return 4;
    };
    auto position = [](WorldPoint p)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
        {
            return std::string("null");
        }
        std::ostringstream value;
        value.imbue(std::locale::classic());
        value << "{\"x\":" << p.x << ",\"y\":" << p.y << '}';
        return value.str();
    };
    auto number = [](float value)
    {
        return std::isfinite(value) ? std::to_string(value) : std::string("null");
    };
    int activeNpc = 0, alive = 0, alerted = 0, weak = 0;
    int regionNpc[9] = {}, regionMonsters[9] = {}, regionAlert[9] = {};
    int npcWorking = 0, npcHunting = 0, npcRecovering = 0, npcGold = 0, npcKills = 0;
    const auto* quest = chapter.Current();
    const auto* nearby = inWorld ? NearbyStory() : nullptr;
    const auto* nearbyResident =
        inWorld && !nearby && ControlsActive() ? NearbyResident() : nullptr;
    std::ostringstream npcs, monsters;
    npcs.imbue(std::locale::classic());
    monsters.imbue(std::locale::classic());
    for (int index = 0; index < StoryLayout::NpcCount; ++index)
    {
        const StoryLayout::Site* active = nullptr;
        const StoryLayout::Site* first = nullptr;
        for (const auto& site : StoryLayout::Sites())
        {
            if (site.kind == StoryLayout::Kind::Evidence || site.spriteIndex != index)
            {
                continue;
            }
            if (!first)
            {
                first = &site;
            }
            if (inWorld && StoryLayout::Visible(site, chapter))
            {
                active = &site;
                break;
            }
        }
        if (index)
        {
            npcs << ',';
        }
        const bool objective =
            active && quest &&
            (quest->trigger == "talk:" + active->id || quest->trigger == "choice:" + active->id);
        const bool speaking = active && dialoguePending && speaker == active->name;
        const char* status = !inWorld                                                ? "waiting"
                             : speaking                                              ? "talking"
                             : active                                                ? "present"
                             : (index == 0 && chapter.step >= chapter.Index("P001")) ? "missing"
                                                                                     : "inactive";
        npcs << "{\"id\":" << Monitoring::Quote(StoryLayout::NpcSprites()[index].id)
             << ",\"name\":" << Monitoring::Quote(first ? first->name : "미등록 NPC")
             << ",\"status\":" << Monitoring::Quote(status)
             << ",\"active\":" << (active ? "true" : "false")
             << ",\"health\":null,\"quest_target\":" << (objective ? "true" : "false")
             << ",\"interactable\":"
             << (active && active == nearby && !controlling && ControlsActive() ? "true" : "false")
             << ",\"site_id\":" << (active ? Monitoring::Quote(active->id) : "null")
             << ",\"position\":" << (active ? position(active->position) : "null") << ",\"region\":"
             << (active ? Monitoring::Quote(regionIds[regionOf(active->position)]) : "null") << '}';
        if (active)
        {
            ++activeNpc;
            ++regionNpc[regionOf(active->position)];
        }
    }
    for (const auto& npc : population.people)
    {
        npcs << ',';
        const int region = regionOf(npc.position);
        const bool speaking = inWorld && dialoguePending && talkingResident == npc.id;
        if (inWorld)
        {
            ++activeNpc;
            ++regionNpc[region];
            npcWorking += !speaking &&
                          (npc.action == Population::Working || npc.action == Population::Trading);
            npcHunting += !speaking &&
                          (npc.action == Population::Hunting || npc.action == Population::Fighting);
            npcRecovering += !speaking && npc.action == Population::Recovering;
            npcGold += npc.gold;
            npcKills += npc.kills;
        }
        npcs << "{\"id\":" << Monitoring::Quote("resident-" + std::to_string(npc.id))
             << ",\"name\":" << Monitoring::Quote(Population::Name(npc.id))
             << ",\"kind\":\"autonomous\",\"role\":"
             << Monitoring::Quote(Population::RoleId(npc.role))
             << ",\"role_name\":" << Monitoring::Quote(Population::RoleName(npc.role))
             << ",\"home_region\":"
             << Monitoring::Quote(npc.district == 0 ? regionIds[0] : regionIds[npc.district + 4])
             << ",\"status\":"
             << Monitoring::Quote(!inWorld   ? "waiting"
                                  : speaking ? "talking"
                                             : Population::ActionId(npc.action))
             << ",\"active\":" << (inWorld ? "true" : "false")
             << ",\"health\":" << number(npc.health)
             << ",\"maximum\":100,\"stamina\":" << number(npc.stamina) << ",\"gold\":" << npc.gold
             << ",\"earned\":" << npc.earned << ",\"kills\":" << npc.kills
             << ",\"goods\":" << npc.goods
             << ",\"quest_target\":" << (ResidentQuestTarget(npc.id) ? "true" : "false")
             << ",\"interactable\":" << (nearbyResident == &npc ? "true" : "false")
             << ",\"destination\":" << position(npc.destination)
             << ",\"position\":" << (inWorld ? position(npc.position) : "null")
             << ",\"region\":" << (inWorld ? Monitoring::Quote(regionIds[region]) : "null") << '}';
    }
    if (inWorld)
    {
        for (const auto& enemy : levelOne.enemies)
        {
            if (enemy.health <= 0)
            {
                continue;
            }
            if (alive++)
            {
                monsters << ',';
            }
            const int region = regionOf(enemy.position);
            ++regionMonsters[region];
            const bool fighting = enemy.alerted && !enemy.returning && !enemy.weakened;
            alerted += fighting ? 1 : 0;
            regionAlert[region] += fighting ? 1 : 0;
            weak += enemy.weakened ? 1 : 0;
            const std::string id = enemy.boss ? "boss-" + std::to_string(enemy.boss)
                                              : "camp-" + std::to_string(enemy.camp.first) + "-" +
                                                    std::to_string(enemy.camp.second) + "-" +
                                                    std::to_string(enemy.slot);
            monsters << "{\"id\":" << Monitoring::Quote(id) << ",\"type\":" << enemy.type
                     << ",\"boss\":" << enemy.boss << ",\"species_name\":"
                     << Monitoring::Quote(enemy.boss ? StoryEncounter::Get(enemy.boss).name
                                                     : SpiritName(enemy.type))
                     << ",\"health\":" << number(enemy.weakened ? 0.f : enemy.health)
                     << ",\"maximum\":" << number(enemy.maximum) << ",\"status\":"
                     << Monitoring::Quote(enemy.weakened    ? "remnant"
                                          : enemy.returning ? "returning"
                                          : fighting        ? "combat"
                                                            : "idle")
                     << ",\"position\":" << position(enemy.position)
                     << ",\"region\":" << Monitoring::Quote(regionIds[region]) << '}';
        }
    }
    std::ostringstream json;
    json.imbue(std::locale::classic());
    json << "{\"schema_version\":1,\"type\":\"world_snapshot\",\"game_id\":"
         << Monitoring::Quote(settings.gameId)
         << ",\"game_name\":" << Monitoring::Quote(settings.gameName)
         << ",\"source_id\":" << Monitoring::Quote(settings.sourceId)
         << ",\"session\":" << Monitoring::Quote(monitorSession)
         << ",\"sequence\":" << ++monitorSequence << ",\"timestamp_ms\":" << Monitoring::Timestamp()
         << ",\"mode\":" << Monitoring::Quote(mode)
         << ",\"scope\":\"loaded_entities\",\"player\":{\"position\":" << position(player)
         << ",\"health\":" << number(levelOne.health)
         << ",\"controlling_spirit\":" << (controlling ? "true" : "false") << "},\"quest\":";
    if (inWorld && quest)
    {
        json << "{\"id\":" << Monitoring::Quote(quest->id)
             << ",\"title\":" << Monitoring::Quote(quest->title)
             << ",\"objective\":" << Monitoring::Quote(quest->objective) << '}';
    }
    else
    {
        json << "null";
    }
    json << ",\"counts\":{\"npc_registered\":" << StoryLayout::NpcCount + population.people.size()
         << ",\"npc_active\":" << activeNpc
         << ",\"npc_autonomous\":" << (inWorld ? population.people.size() : 0)
         << ",\"npc_working\":" << npcWorking << ",\"npc_hunting\":" << npcHunting
         << ",\"npc_recovering\":" << npcRecovering << ",\"npc_gold\":" << npcGold
         << ",\"npc_kills\":" << npcKills << ",\"monsters_alive\":" << alive
         << ",\"monsters_combat\":" << alerted << ",\"monsters_weakened\":" << weak
         << "},\"regions\":[";
    for (int i = 0; i < 9; ++i)
    {
        if (i)
        {
            json << ',';
        }
        json << "{\"id\":" << Monitoring::Quote(regionIds[i])
             << ",\"name\":" << Monitoring::Quote(regionNames[i])
             << ",\"kind\":" << Monitoring::Quote(i == 0 || i >= 5 ? "village" : "area")
             << ",\"status\":"
             << Monitoring::Quote(!inWorld            ? "waiting"
                                  : regionAlert[i]    ? "combat"
                                  : regionMonsters[i] ? "monsters_present"
                                                      : "no_observed_threat")
             << ",\"npc_active\":" << regionNpc[i] << ",\"monsters_alive\":" << regionMonsters[i]
             << ",\"monsters_combat\":" << regionAlert[i]
             << ",\"coverage\":\"loaded_only\",\"npc_coverage\":\"complete\"}";
    }
    json << "],\"npcs\":[" << npcs.str() << "],\"monsters\":[" << monsters.str() << "]}";
    monitorPublisher.Submit(json.str());
}
