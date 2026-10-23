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
    constexpr int RegionCount = World::VillageCount + 4;
    std::array<const char*, RegionCount> regionIds, regionNames;
    for (int village = 0; village < World::VillageCount; ++village)
    {
        regionIds[village] = World::VillageId(village);
        regionNames[village] = World::VillageName(village);
    }
    const char* areaIds[] = {"outskirts", "ruins", "chapel", "wilderness"};
    const char* areaNames[] = {"마을 외곽·묘역", "폐허·감금 시설", "예배당", "야외 탐험 구역"};
    for (int area = 0; area < 4; ++area)
    {
        regionIds[World::VillageCount + area] = areaIds[area];
        regionNames[World::VillageCount + area] = areaNames[area];
    }
    auto regionOf = [](WorldPoint p)
    {
        const int village = World::VillageAt(p);
        if (village >= 0)
        {
            return village;
        }
        if (p.x >= 512 && p.x <= 3500 && p.y >= -450 && p.y <= 400)
        {
            return World::VillageCount + (p.x < 1500 ? 0 : p.x < 2800 ? 1 : 2);
        }
        return World::VillageCount + 3;
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
    auto partIndices = [](const NpcAppearance::Definition& appearance)
    {
        const auto& p = appearance.partIndices;
        return "{\"body\":" + std::to_string(p[0]) + ",\"hair\":" + std::to_string(p[1]) +
               ",\"outfit\":" + std::to_string(p[2]) + ",\"accessory\":" + std::to_string(p[3]) +
               "}";
    };
    int activeNpc = 0, alive = 0, alerted = 0, weak = 0;
    int regionNpc[RegionCount] = {}, regionMonsters[RegionCount] = {},
        regionAlert[RegionCount] = {};
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
             << ",\"part_indices\":"
             << partIndices(models.npcDirect
                                ? models.catalog.Story(index, NpcAppearance::Story(index))
                                : NpcAppearance::Story(index))
             << ",\"status\":" << Monitoring::Quote(status)
             << ",\"active\":" << (active ? "true" : "false")
             << ",\"home_region\":" << Monitoring::Quote(World::VillageId(0))
             << ",\"home_id\":" << Monitoring::Quote("home-" + std::to_string(index))
             << ",\"home_address\":" << Monitoring::Quote(VillageLayout::Address(index))
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
             << ",\"part_indices\":" << partIndices(npc.appearance)
             << ",\"kind\":\"autonomous\",\"role\":"
             << Monitoring::Quote(Population::RoleId(npc.role))
             << ",\"role_name\":" << Monitoring::Quote(Population::RoleName(npc.role))
             << ",\"home_region\":" << Monitoring::Quote(World::VillageId(npc.district))
             << ",\"status\":"
             << Monitoring::Quote(!inWorld   ? "waiting"
                                  : speaking ? "talking"
                                             : Population::ActionId(npc.action))
             << ",\"active\":" << (inWorld ? "true" : "false")
             << ",\"health\":" << number(npc.health)
             << ",\"maximum\":100,\"stamina\":" << number(npc.stamina) << ",\"gold\":" << npc.gold
             << ",\"earned\":" << npc.earned << ",\"kills\":" << npc.kills
             << ",\"goods\":" << npc.goods
             << ",\"home_id\":" << Monitoring::Quote("home-" + std::to_string(npc.houseId))
             << ",\"home_address\":" << Monitoring::Quote(VillageLayout::Address(npc.houseId))
             << ",\"home_position\":" << position(npc.home)
             << ",\"personality\":" << Monitoring::Quote(Population::Personality(npc))
             << ",\"heard_news\":" << Monitoring::Quote(population.HeardNews(npc))
             << ",\"migrations\":" << npc.migrations << ",\"migrating_to\":"
             << (npc.migrationVillage >= 0
                     ? Monitoring::Quote(World::VillageId(npc.migrationVillage))
                     : "null")
             << ",\"mail_id\":"
             << (npc.mailId >= 0 ? Monitoring::Quote("letter-" + std::to_string(npc.mailId))
                                 : "null")
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
                     << ",\"disposition\":"
                     << Monitoring::Quote(SpiritCatalog::Get(enemy.type).disposition)
                     << ",\"gait\":"
                     << Monitoring::Quote(SpiritCatalog::Get(enemy.type).gait == 2   ? "quadruped"
                                          : SpiritCatalog::Get(enemy.type).gait == 1 ? "biped"
                                                                                     : "hover")
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
    for (int i = 0; i < RegionCount; ++i)
    {
        if (i)
        {
            json << ',';
        }
        json << "{\"id\":" << Monitoring::Quote(regionIds[i])
             << ",\"name\":" << Monitoring::Quote(regionNames[i])
             << ",\"kind\":" << Monitoring::Quote(i < World::VillageCount ? "village" : "area")
             << ",\"status\":"
             << Monitoring::Quote(!inWorld            ? "waiting"
                                  : regionAlert[i]    ? "combat"
                                  : regionMonsters[i] ? "monsters_present"
                                                      : "no_observed_threat")
             << ",\"npc_active\":" << regionNpc[i] << ",\"monsters_alive\":" << regionMonsters[i]
             << ",\"monsters_combat\":" << regionAlert[i]
             << ",\"coverage\":\"loaded_only\",\"npc_coverage\":\"complete\"";
        if (i < World::VillageCount)
        {
            int residents = i == 0 ? VillageLayout::StoryResidents : 0;
            int reserved = 0, vacant = 0, incoming = 0, known = 0;
            for (const auto& npc : population.people)
            {
                residents += npc.district == i;
                reserved += npc.migrationVillage == i;
            }
            for (const auto& house : VillageLayout::Houses())
            {
                vacant += house.village == i && population.HouseOwner(house.id) == -1;
            }
            for (const auto& report : population.society.boards[i])
            {
                known += report.serial > 0;
            }
            for (const auto& letter : population.society.letters)
            {
                incoming += !letter.delivered && letter.to == i;
            }
            json << ",\"residents\":" << residents
                 << ",\"houses\":" << VillageLayout::HomesPerVillage
                 << ",\"houses_occupied\":" << residents << ",\"houses_reserved\":" << reserved
                 << ",\"houses_vacant\":" << vacant << ",\"mail_in_transit\":" << incoming
                 << ",\"news_known\":" << known
                 << ",\"position\":" << position(World::Settlements()[i])
                 << ",\"post_office\":" << position(Population::PostOffice(i))
                 << ",\"half_size\":" << World::VillageHalfSize;
        }
        json << '}';
    }
    json << "],\"npcs\":[" << npcs.str() << "],\"monsters\":[" << monsters.str()
         << "],\"houses\":[";
    bool comma = false;
    for (const auto& house : VillageLayout::Houses())
    {
        if (comma)
        {
            json << ',';
        }
        comma = true;
        const int owner = population.HouseOwner(house.id);
        const bool reserved = owner >= 0 && population.people[owner].houseId != house.id;
        json << "{\"id\":" << Monitoring::Quote("home-" + std::to_string(house.id))
             << ",\"village\":" << Monitoring::Quote(World::VillageId(house.village))
             << ",\"address\":" << Monitoring::Quote(VillageLayout::Address(house.id))
             << ",\"position\":" << position(house.entrance) << ",\"owner\":";
        if (owner == -2)
        {
            json << Monitoring::Quote(StoryLayout::NpcSprites()[house.id].id);
        }
        else if (owner >= 0)
        {
            json << Monitoring::Quote("resident-" + std::to_string(owner));
        }
        else
        {
            json << "null";
        }
        json << ",\"status\":"
             << Monitoring::Quote(reserved      ? "reserved"
                                  : owner == -1 ? "vacant"
                                                : "occupied")
             << '}';
    }
    json << "],\"letters\":[";
    comma = false;
    for (const auto& letter : population.society.letters)
    {
        if (comma)
        {
            json << ',';
        }
        comma = true;
        json << "{\"id\":" << Monitoring::Quote("letter-" + std::to_string(letter.id))
             << ",\"from\":" << Monitoring::Quote(World::VillageId(letter.from))
             << ",\"to\":" << Monitoring::Quote(World::VillageId(letter.to))
             << ",\"carrier\":" << Monitoring::Quote("resident-" + std::to_string(letter.carrier))
             << ",\"delivered\":" << (letter.delivered ? "true" : "false")
             << ",\"summary\":" << Monitoring::Quote(Population::NewsText(letter.news)) << '}';
    }
    json << "],\"roads\":[";
    comma = false;
    for (const auto& link : World::Connections())
    {
        if (comma)
        {
            json << ',';
        }
        comma = true;
        json << "{\"from\":" << Monitoring::Quote(World::VillageId(link.first))
             << ",\"to\":" << Monitoring::Quote(World::VillageId(link.second))
             << ",\"travel_seconds\":" << World::RoadTravelSeconds << '}';
    }
    json << "],\"society\":{\"elapsed_seconds\":" << population.society.clock
         << ",\"letters_delivered\":" << population.society.delivered
         << ",\"migrations\":" << population.society.migrations << "}}";
    monitorPublisher.Submit(json.str());
}
