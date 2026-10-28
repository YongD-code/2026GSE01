#include "stdafx.h"
#include "Prototype.h"
#include "FrameProfiler.h"

void Prototype::DrawTerrain(const World::Chunk& chunk, ActorPosition position)
{
    const Point center = Screen(position.x + 256, position.y + 256);
    // Projected bounds of the entire 512-unit diamond, including edge strokes.
    if (center.x + 438 < 0 || center.x - 438 > width || center.y + 224 < 0 ||
        center.y - 224 > height)
    {
        return;
    }
    const int variant =
        static_cast<int>(World::Hash(chunk.key.first, chunk.key.second, world.seed) % 4);
    // Share equal road/village tile masks; authored roads no longer repeat every chunk.
    const std::string terrainKey =
        "terrain:shared-v5:" + std::to_string(chunk.biome) + ":" + std::to_string(chunk.roadMask) +
        ":" + std::to_string(chunk.villageMask) + ":" + std::to_string(variant);
    const double offsetX = position.x - double(chunk.key.first) * World::ChunkSize;
    const double offsetY = position.y - double(chunk.key.second) * World::ChunkSize;
    const Point terrainOrigin = Screen(position.x, position.y);
    if (r.BeginCachedMesh(terrainKey, terrainOrigin))
    {
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
            {
                World::Coordinate tx = chunk.key.first * 8 + x, ty = chunk.key.second * 8 + y;
                double wx = double(tx) * 64, wy = double(ty) * 64;
                auto hash = World::Hash(x, y, 42 + variant);
                float shade = float(hash % 15) * .002f;
                Color color =
                    chunk.biome == 0
                        ? Color(.095f + shade, .125f + shade, .13f + shade)
                        : (chunk.biome == 1 ? Color(.12f + shade, .145f + shade, .145f + shade)
                                            : Color(.15f + shade, .14f + shade, .14f + shade));
                if ((chunk.villageMask >> (y * 8 + x)) & 1)
                    color = Color(.11f + shade, .13f + shade, .13f + shade);
                bool road = ((chunk.roadMask >> (y * 8 + x)) & 1) != 0;
                if (road)
                    color = Color(.23f + shade, .23f + shade, .20f + shade);
                Ground(wx + offsetX, wy + offsetY, 64, 64, color);
                const double tileX = wx + offsetX;
                const double tileY = wy + offsetY;
                if (road)
                {
                    // Inset flagstones leave dark joints without altering walkability.
                    for (int row = 0; row < 2; ++row)
                    {
                        for (int col = 0; col < 2; ++col)
                        {
                            const double sx = tileX + 3 + col * 30;
                            const double sy = tileY + 3 + row * 30;
                            const float tint = float((hash >> (row * 3 + col)) % 5) * .012f;
                            r.Quad(Screen(sx + 2, sy),
                                   Screen(sx + 27, sy + 1),
                                   Screen(sx + 28, sy + 27),
                                   Screen(sx, sy + 28),
                                   Color(.29f + tint, .31f + tint, .29f + tint));
                            r.Line(Screen(sx + 2, sy),
                                   Screen(sx + 27, sy + 1),
                                   1,
                                   Color(.41f, .43f, .38f));
                        }
                    }
                    if (hash % 4 == 0)
                    {
                        Ground(tileX + 21, tileY + 8, 2, 10, Color(.15f, .23f, .14f));
                    }
                }
                else
                {
                    // Flat moss, fallen needles and gravel are part of the terrain mesh.
                    for (int detail = 0; detail < 3; ++detail)
                    {
                        const auto bits = World::Hash(x, y, 83 + detail + variant * 7);
                        const double sx = tileX + 5 + double(bits % 47);
                        const double sy = tileY + 5 + double((bits >> 9) % 47);
                        if (detail < 2)
                        {
                            r.Quad(Screen(sx, sy + 3),
                                   Screen(sx + 6, sy),
                                   Screen(sx + 11, sy + 5),
                                   Screen(sx + 3, sy + 9),
                                   Color(.13f + shade, .21f + shade, .135f));
                        }
                        else
                        {
                            r.Triangle(Screen(sx, sy),
                                       Screen(sx + 4, sy + 1),
                                       Screen(sx + 2, sy + 4),
                                       Color(.28f, .30f, .26f));
                            r.Line(Screen(sx + 3, sy + 7),
                                   Screen(sx + 9, sy + 10),
                                   1,
                                   Color(.27f, .23f, .14f));
                        }
                    }
                }
            }
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(terrainKey, terrainOrigin);
}

void Prototype::DrawObject(const Object& o)
{
    const Point bounds = Screen(o.x + o.w * .5, o.y + o.d * .5);
    const double radiusX = (o.w + o.d) * .425 + 65;
    const double radiusY = (o.w + o.d) * .215 + 18;
    if (bounds.x + radiusX < 0 || bounds.x - radiusX > width || bounds.y + radiusY < 0 ||
        bounds.y - radiusY - o.h - 75 > height)
        return;
    Object visual = o;
    const int variant = o.kind == 0
                            ? 0
                            : static_cast<int>(World::Hash(static_cast<World::Coordinate>(o.x),
                                                           static_cast<World::Coordinate>(o.y),
                                                           774) %
                                               3);
    float scale = 1.f;
    if (o.kind == 1)
    {
        visual.w = 18;
        visual.d = 18;
        visual.h = 90;
        scale = static_cast<float>(o.h / visual.h);
    }
    const float importedWidth = o.kind == 1 ? 48.f : static_cast<float>(visual.w);
    const float importedDepth = o.kind == 1 ? 48.f : static_cast<float>(visual.d);
    const Point importedOrigin = o.kind == 1 ? Screen(o.x + (o.w - importedWidth * scale) * .5,
                                                      o.y + (o.d - importedDepth * scale) * .5)
                                             : Screen(o.x, o.y);
    if (models.DrawScenery(r,
                           o.kind,
                           variant,
                           importedOrigin,
                           importedWidth,
                           importedDepth,
                           static_cast<float>(visual.h),
                           scale))
    {
        return;
    }
    const std::string meshKey = "object:shared-v5:" + std::to_string(visual.kind) + ":" +
                                std::to_string(visual.w) + ":" + std::to_string(visual.d) + ":" +
                                std::to_string(visual.h) + ":" + std::to_string(variant);
    const Point origin = Screen(o.x, o.y);
    if (r.BeginCachedMesh(meshKey, origin))
    {
        if (o.kind == 0)
            House(o);
        else if (o.kind == 1)
            Tree(visual, variant);
        else
            Stone(o, variant);
        r.EndCachedMesh();
    }
    const Point drawOrigin = o.kind == 1 ? Screen(o.x + (o.w - visual.w * scale) * .5,
                                                  o.y + (o.d - visual.d * scale) * .5)
                                         : origin;
    r.DrawCachedMesh(meshKey, drawOrigin, scale);
}

void Prototype::SynchronizeScene()
{
    FrameProfiler::Scope timing(FrameProfiler::Stage::SceneSync);
    scene.BeginSync();
    if (titleScreen || openingPlaying)
    {
        auto& presentation = scene.Ensure("presentation");
        presentation.layer = ActorLayer::Interface;
        presentation.draw = [this](const Actor&)
        {
            if (openingPlaying)
                DrawOpening();
            else
                DrawFront();
        };
        presentation.pointer = [this](int x, int y)
        {
            if (openingPlaying)
            {
                if (OpeningSkipBounds().Contains(x, y))
                    SkipOpening();
                return true;
            }
            return FrontClick(x, y);
        };
        scene.EndSync();
        sceneReady = true;
        return;
    }
    auto inView = [this](WorldPoint at, double horizontal, double above, double below)
    {
        const Point p = Screen(at.x, at.y);
        return p.x + horizontal >= -96 && p.x - horizontal <= width + 96 && p.y + below >= -96 &&
               p.y - above <= height + 96;
    };
    auto objectInView = [&](const Object& data)
    {
        return inView({data.x + data.w * .5, data.y + data.d * .5},
                      (data.w + data.d) * .425 + 65,
                      (data.w + data.d) * .215 + 18 + data.h + 75,
                      (data.w + data.d) * .215 + 18);
    };
    auto& root = scene.Ensure("world");
    root.update = [this](Actor&, float dt)
    {
        UpdateSimulation(dt, requestedSprint);
    };
    scene.Ensure("village", "world");
    scene.Ensure("chunks", "world");
    scene.Ensure("characters", "world");
    scene.Ensure("enemies", "world");
    scene.Ensure("loot", "world");
    scene.Ensure("projectiles", "world");
    scene.Ensure("effects", "world");
    scene.Ensure("interface");
    BindCemeteryRoom();

    auto bind = [this](const std::string& id,
                       const std::string& parent,
                       WorldPoint position,
                       ActorLayer layer,
                       std::function<void(const Actor&)> draw) -> FunctionActor&
    {
        auto& actor = scene.Ensure(id, parent);
        actor.SetWorldPosition({position.x, position.y, 0});
        actor.layer = layer;
        actor.footprint = {
            0, 0, 0, 0, layer == ActorLayer::World || layer == ActorLayer::Character};
        actor.depthOffset = 0;
        actor.draw = std::move(draw);
        return actor;
    };

    auto object = [&](const std::string& id, const std::string& parent, const Object& data)
    {
        if (!objectInView(data))
            return;
        const auto& buildings = StoryLayout::Buildings();
        const bool storyFacade = data.kind == 0 && std::any_of(buildings.begin(),
                                                               buildings.end(),
                                                               [&data](const Object& building)
                                                               {
                                                                   return building.kind == 0 &&
                                                                          building.x == data.x &&
                                                                          building.y == data.y;
                                                               });
        auto& actor = bind(id,
                           parent,
                           {data.x, data.y},
                           ActorLayer::World,
                           [this, data, storyFacade](const Actor& node)
                           {
                               auto placed = data;
                               const auto position = node.WorldPosition();
                               placed.x = position.x;
                               placed.y = position.y;
                               DrawObject(placed);
                               if (storyFacade)
                               {
                                   StoryFacade(placed);
                               }
                           });
        actor.footprint = {0, 0, data.w, data.d, true};
        if (data.kind == 1)
        {
            // The foliage is elevated; the trunk determines front/back occlusion.
            actor.footprint = {data.w * .5, data.d * .5, 0, 0, true};
        }
    };

    auto modelBind = [this](const std::string& id,
                            const std::string& parent,
                            WorldPoint position,
                            std::function<void(const Actor&)> draw) -> ModelActor&
    {
        auto& actor = scene.Ensure<ModelActor>(id, parent);
        actor.SetWorldPosition({position.x, position.y, 0});
        actor.layer = ActorLayer::Character;
        actor.footprint = {0, 0, 0, 0, true};
        actor.depthOffset = 0;
        actor.draw = std::move(draw);
        return actor;
    };

    for (size_t i = 0; i < villageObjects.size(); ++i)
        object("village/object/" + std::to_string(i), "village", villageObjects[i]);

    for (const auto& entry : world.chunks)
    {
        const auto& chunk = entry.second;
        // Remote NPC collision chunks stay loaded without creating remote render actors.
        if (!inView({double(chunk.key.first) * World::ChunkSize + 256,
                     double(chunk.key.second) * World::ChunkSize + 256},
                    520,
                    500,
                    300))
            continue;
        const std::string id =
            "chunk/" + std::to_string(chunk.key.first) + "/" + std::to_string(chunk.key.second);
        auto& parent = scene.Ensure(id, "chunks");
        parent.SetWorldPosition({double(chunk.key.first) * World::ChunkSize,
                                 double(chunk.key.second) * World::ChunkSize,
                                 0});
        // Store only terrain metadata, not a second copy of the object array.
        World::Chunk terrain;
        terrain.key = chunk.key;
        terrain.biome = chunk.biome;
        terrain.roadMask = chunk.roadMask;
        terrain.villageMask = chunk.villageMask;
        auto& ground = scene.Ensure(id + "/terrain", id);
        ground.layer = ActorLayer::Terrain;
        ground.draw = [this, terrain](const Actor& node)
        {
            DrawTerrain(terrain, node.WorldPosition());
        };
        for (size_t i = 0; i < chunk.objects.size(); ++i)
            object(id + "/object/" + std::to_string(i), id, chunk.objects[i]);
    }

    for (int village = 0; village < World::VillageCount; ++village)
    {
        const auto hub = World::Settlements()[village];
        if (!inView(hub, 520, 500, 300))
        {
            continue;
        }
        bind("village/square/" + std::to_string(village),
             "village",
             {hub.x - 165, hub.y - 145},
             ActorLayer::Terrain,
             [this](const Actor& node)
             {
                 const auto p = node.WorldPosition();
                 const Point origin = Screen(p.x, p.y);
                 const std::string key = "village:square:environment-v3";
                 if (r.BeginCachedMesh(key, origin))
                 {
                     Ground(p.x, p.y, 330, 290, Color(.17f, .20f, .18f));
                     for (int row = 0; row < 10; ++row)
                     {
                         for (int col = 0; col < 11; ++col)
                         {
                             const double sx = p.x + col * 30 + 1;
                             const double sy = p.y + row * 29 + 1;
                             const float tone = float((row * 7 + col * 3) % 5) * .012f;
                             Ground(sx, sy, 28, 27, Color(.29f + tone, .31f + tone, .28f + tone));
                             r.Line(
                                 Screen(sx, sy), Screen(sx + 28, sy), 1, Color(.39f, .41f, .36f));
                         }
                     }
                     r.EndCachedMesh();
                 }
                 r.DrawCachedMesh(key, origin);
             })
            .depthOffset = 1.e12;

        const auto office = Population::PostOffice(village);
        const std::string post = "village/post/" + std::to_string(village);
        bind(post,
             "village",
             office,
             ActorLayer::World,
             [this](const Actor& node)
             {
                 const auto at = node.WorldPosition();
                 Box(at.x + 28,
                     at.y,
                     20,
                     16,
                     44,
                     Color(.35f, .14f, .10f),
                     Color(.22f, .09f, .07f),
                     Color(.16f, .08f, .06f));
                 r.Line(Screen(at.x + 32, at.y + 16, 32),
                        Screen(at.x + 44, at.y + 16, 32),
                        2,
                        Color(.04f, .04f, .05f));
             })
            .footprint = {28, 0, 20, 16, true};
        bind(post + "/label",
             post,
             office,
             ActorLayer::WorldLabel,
             [this, village](const Actor& node)
             {
                 const auto at = node.WorldPosition();
                 if (Distance(player, {at.x, at.y}) < 300)
                 {
                     DrawWorldLabel({at.x, at.y},
                                    62,
                                    std::string(World::VillageName(village)) + " · 편지함",
                                    Color(.7f, .85f, 1.f));
                 }
             });
    }
    for (const auto& house : VillageLayout::Houses())
    {
        if (Distance(player, house.entrance) > 115 || !objectInView(house.building))
        {
            continue;
        }
        const auto* parentHouse = &house;
        bind("village/home-label/" + std::to_string(house.id),
             "village/object/" +
                 std::to_string(VillageLayout::AuthoredBuildings().size() + house.id),
             house.entrance,
             ActorLayer::WorldLabel,
             [this, parentHouse](const Actor& node)
             {
                 const auto at = node.WorldPosition();
                 DrawWorldLabel(
                     {at.x, at.y}, 38, HomeOccupant(*parentHouse), Color(.85f, .8f, .66f));
             });
    }

    bind("village/hearth",
         "village",
         fire,
         ActorLayer::World,
         [this](const Actor& node)
         {
             const auto p = node.WorldPosition();
             Hearth({p.x, p.y});
         });
    bind("characters/player",
         "characters",
         player,
         ActorLayer::Character,
         [this](const Actor& node)
         {
             const auto p = node.WorldPosition();
             DrawPlayer({p.x, p.y});
         });
    scene.Ensure("story", "world");
    for (auto site : StoryLayout::Sites())
    {
        site.position = StoryPosition(site);
        if (!StoryVisible(site) || !inView(site.position, 180, 220, 100))
        {
            continue;
        }
        if (site.kind != StoryLayout::Kind::Evidence &&
            (site.id != "merchant" || !merchantModel.Ready()))
        {
            bind("story-setting/" + site.id,
                 "story",
                 site.position,
                 ActorLayer::World,
                 [this, site](const Actor& node)
                 {
                     const auto at = node.WorldPosition();
                     const Point p = Screen(at.x, at.y);
                     if (p.x >= -140 && p.x <= width + 140 && p.y >= -80 && p.y <= height + 160)
                     {
                         StoryProp(site, {at.x, at.y});
                     }
                 });
        }
        const std::string bodyId = "story/" + site.id;
        if (site.kind != StoryLayout::Kind::Evidence && !StoryPersonVisible(site, site.position))
        {
            continue;
        }
        if (site.kind == StoryLayout::Kind::Evidence)
        {
            bind(bodyId,
                 "story",
                 site.position,
                 ActorLayer::World,
                 [this, site](const Actor& node)
                 {
                     const auto pos = node.WorldPosition();
                     StoryProp(site, {pos.x, pos.y});
                 });
        }
        else if (auto* model = StoryCharacterModel(site.id))
        {
            auto& actor = scene.Ensure<StationaryNpcActor>(bodyId, "story");
            actor.SetWorldPosition({site.position.x, site.position.y, 0});
            actor.layer = ActorLayer::Character;
            actor.footprint = {-30, -22, 60, 68, true};
            actor.update = [this, site](Actor& node, float dt)
            {
                UpdateStationaryNpcActor(static_cast<StationaryNpcActor&>(node), dt, site.id);
            };
            actor.draw = [this, site, model](const Actor& node)
            {
                const auto& person = static_cast<const StationaryNpcActor&>(node);
                const auto at = node.WorldPosition();
                const Point feet = Screen(at.x, at.y);
                r.Ellipse(feet.x, feet.y + 1, 15, 5, Color(0, 0, 0, .35f));
                model->Draw(
                    r, feet, Color(1, 1, 1), person.modelState, StoryLayout::PersonHeight(site));
            };
            // Draw may follow a sync before the first simulation tick.
            if (actor.modelState.owner != model)
            {
                UpdateStationaryNpcActor(actor, 0, site.id);
            }
        }
        else
        {
            modelBind(bodyId,
                      "story",
                      site.position,
                      [this, site](const Actor& node)
                      {
                          const auto pos = node.WorldPosition();
                          StoryPerson(site,
                                      {pos.x, pos.y},
                                      &static_cast<const ModelActor&>(node).modelState);
                      });
        }
        bind("story-label/" + site.id,
             bodyId,
             site.position,
             ActorLayer::WorldLabel,
             [this, site](const Actor& node)
             {
                 const auto pos = node.WorldPosition();
                 const WorldPoint at = {pos.x, pos.y};
                 const bool evidence = site.kind == StoryLayout::Kind::Evidence;
                 if ((!evidence && StoryPersonVisible(site, at)) ||
                     (evidence && StoryVisible(site) && Distance(player, at) < 130))
                 {
                     DrawWorldLabel(at,
                                    evidence ? 40.f : StoryLayout::PersonHeight(site) + 12.f,
                                    site.name,
                                    Color(.95f, .9f, .72f));
                 }
             });
    }
    scene.Ensure("residents", "world");
    for (size_t i = 0; i < population.people.size(); ++i)
    {
        const auto& person = population.people[i];
        if (!ResidentVisible(person))
            continue;
        const std::string id = "residents/" + std::to_string(person.id);
        modelBind(id,
                  "residents",
                  person.position,
                  [this, i](const Actor& node)
                  {
                      DrawResident(population.people[i],
                                   &static_cast<const ModelActor&>(node).modelState);
                  });
        if (ResidentNameVisible(person))
            bind(id + "/label",
                 id,
                 person.position,
                 ActorLayer::WorldLabel,
                 [this, i](const Actor& node)
                 {
                     const auto& npc = population.people[i];
                     if (ResidentNameVisible(npc))
                     {
                         const auto at = node.WorldPosition();
                         DrawWorldLabel(
                             {at.x, at.y}, 79, Population::Name(npc.id), Color(.75f, .94f, .83f));
                     }
                 });
    }
    if (captured)
        modelBind(
            "characters/companion",
            "characters",
            spirit,
            [this](const Actor& node)
            {
                const auto p = node.WorldPosition();
                Wisp(capturedSpirit, {p.x, p.y}, &static_cast<const ModelActor&>(node).modelState);
            });

    for (const auto& enemy : levelOne.enemies)
    {
        if (!inView(enemy.position, 160, 240, 100))
            continue;
        const std::string id = enemy.boss > 0 ? "boss/" + std::to_string(enemy.boss)
                                              : "enemy/" + std::to_string(enemy.camp.first) + "/" +
                                                    std::to_string(enemy.camp.second) + "/" +
                                                    std::to_string(enemy.slot);
        // Capture the index-independent rendering state; vector growth/removal
        // must never leave the graph holding a dangling gameplay reference.
        EnemyVisual visual;
        visual.position = enemy.position;
        visual.type = enemy.type;
        visual.health = enemy.health;
        visual.maximum = enemy.maximum;
        visual.flash = enemy.flash;
        visual.hitDirection = enemy.hitDirection;
        visual.hitStrength = enemy.hitStrength;
        visual.castPose = enemy.phase == 1   ? .7f
                          : enemy.phase == 2 ? 1.f
                          : enemy.phase == 3 ? .25f
                                             : 0.f;
        if (enemy.boss == EaterBoss::Encounter)
        {
            const auto& attack = enemy.eater;
            if (enemy.phase == 1)
            {
                visual.bossCastProgress =
                    .05f + .4f * EaterBoss::Progress(attack.plan.windup, enemy.phaseTime);
            }
            else if (enemy.phase == 2 && attack.pattern != EaterBoss::Pattern::Rush)
            {
                visual.bossCastProgress =
                    .45f + .45f * EaterBoss::Progress(attack.plan.active, enemy.phaseTime);
            }
            else if (enemy.phase == 4)
            {
                visual.bossCastProgress =
                    .15f + .7f * EaterBoss::Progress(EaterBoss::TransitionTime, enemy.phaseTime);
            }
            else if (enemy.phase == 3 && attack.hasAttack &&
                     attack.plan.recovery - enemy.phaseTime < .2f)
            {
                visual.bossCastProgress =
                    .9f + .1f * (attack.plan.recovery - enemy.phaseTime) / .2f;
            }
            else
            {
                visual.castPose = 0;
            }
            if (visual.bossCastProgress >= 0)
            {
                visual.castPose = visual.bossCastProgress;
            }
            if (attack.pattern == EaterBoss::Pattern::Sweep &&
                (enemy.phase == 2 || enemy.phase == 3))
            {
                visual.action = CharacterModel::Motion::Attack;
            }
        }
        visual.animation = enemy.animation;
        visual.boss = enemy.boss;
        visual.alerted = enemy.alerted;
        visual.weakened = enemy.weakened;
        visual.remnantTime = enemy.remnantTime;
        visual.running = enemy.running;
        modelBind(id,
                  "enemies",
                  enemy.position,
                  [this, visual](const Actor& node) mutable
                  {
                      const auto p = node.WorldPosition();
                      visual.position = {p.x, p.y};
                      DrawEnemy(visual, &static_cast<const ModelActor&>(node).modelState);
                  });
        if (enemy.boss > 0)
        {
            const auto& encounter = StoryEncounter::Get(enemy.boss);
            std::string name = encounter.name;
            if (enemy.boss == EaterBoss::Encounter && enemy.alerted && !enemy.weakened)
            {
                if (enemy.eater.phaseTwo)
                {
                    name += " · 격앙";
                }
                if (enemy.phase == 1 || enemy.phase == 2)
                {
                    name += " · ";
                    name += EaterBoss::Name(enemy.eater.pattern);
                }
                else if (enemy.phase == 3)
                {
                    name += " · 반격 기회";
                }
                else if (enemy.phase == 4)
                {
                    name += " · 봉인 붕괴";
                }
            }
            const float elevation = enemy.weakened ? 78.f : encounter.size + 27;
            bind(id + "/label",
                 "enemies",
                 enemy.position,
                 ActorLayer::WorldLabel,
                 [this, name, elevation](const Actor& node)
                 {
                     const auto at = node.WorldPosition();
                     DrawWorldLabel({at.x, at.y}, elevation, name, Color(.94f, .82f, .62f));
                 });
        }
    }

    for (auto& item : levelOne.loot)
    {
        if (!inView(item.position, 80, 80, 40))
            continue;
        if (!item.actorId)
            item.actorId = nextSceneId++;
        const auto kind = item.kind;
        bind("loot/" + std::to_string(item.actorId),
             "loot",
             item.position,
             ActorLayer::Projectile,
             [this, kind](const Actor& node)
             {
                 const auto pos = node.WorldPosition();
                 const Point ground = Screen(pos.x, pos.y);
                 const Point p = Screen(pos.x, pos.y, 3 + std::sin(time * 3) * 1.5);
                 const int index = static_cast<int>(kind);
                 const char* names[] = {"경험치 조각", "무기 강화석", "회복약", "주령석", "봉인권"};
                 const float sizes[] = {30, 44, 44, 40, 48};
                 r.Ellipse(ground.x, ground.y + 2, 15, 5, Color(0, 0, 0, .35f));
                 if (lootSheets[index].Ready())
                     lootSheets[index].Draw(
                         r, p, sizes[index], SpriteAnimation(), Color(1, 1, 1), width, height, 0.f);
                 else
                     r.Text(p.x - 30, p.y - 10, names[index], Color(.9f, .87f, .75f));
             });
    }
    for (auto& shot : levelOne.projectiles)
    {
        if (!inView(shot.position, 80, 100, 40))
            continue;
        if (!shot.actorId)
            shot.actorId = nextSceneId++;
        const auto visual = shot;
        bind("projectile/" + std::to_string(shot.actorId),
             "projectiles",
             shot.position,
             ActorLayer::Projectile,
             [this, visual](const Actor& node)
             {
                 const auto pos = node.WorldPosition();
                 const Point p = Screen(pos.x, pos.y, 32);
                 const Point tail =
                     Screen(pos.x - visual.velocity.x * .03, pos.y - visual.velocity.y * .03, 32);
                 const Color color = visual.hostile     ? Color(3.f, .35f, .15f)
                                     : visual.companion ? Color(1.8f, .5f, 2.f)
                                                        : Color(.4f, 1.5f, 3.f);
                 r.Line(tail, p, 4, color);
                 r.Ellipse(p.x, p.y, 5, 5, color);
             });
    }

    bind("effects/warnings",
         "effects",
         {0, 0},
         ActorLayer::Warning,
         [this](const Actor&)
         {
             DrawCombatWarnings();
         });
    for (int i = 0; i < 9; ++i)
        bind("effects/fog/" + std::to_string(i),
             "effects",
             {0, 0},
             ActorLayer::Atmosphere,
             [this, i](const Actor&)
             {
                 const float x = std::fmod(i * 197.f + time * 9, width + 350.f) - 175;
                 r.Ellipse(x, height * .25f + i * 67, 210, 20, Color(.38f, .48f, .51f, .025f));
             });
    for (int i = 0; i < 40; ++i)
        bind("effects/ash/" + std::to_string(i),
             "effects",
             {0, 0},
             ActorLayer::Atmosphere,
             [this, i](const Actor&)
             {
                 const float x = std::fmod(i * 131.f + time * 13, width + 10.f);
                 const float y = std::fmod(i * 73.f + time * 8, height + 10.f);
                 r.Rect(x, y, 2, 2, Color(.67f, .65f, .55f, .2f));
             });

    bind("effects/spirit-realm",
         "effects",
         {0, 0},
         ActorLayer::SkillBackdrop,
         [this](const Actor&)
         {
             DrawSpiritRealm();
         });
    bind("effects/realm-shell",
         "effects",
         {0, 0},
         ActorLayer::Atmosphere,
         [this](const Actor&)
         {
             DrawSpiritRealmShell();
         });
    bind("effects/hit-feedback",
         "effects",
         {0, 0},
         ActorLayer::Projectile,
         [this](const Actor&)
         {
             for (const auto& hit : levelOne.hitFeedback)
             {
                 const Point p = Screen(hit.position.x, hit.position.y, 28);
                 const float t = hit.age / .24f, alpha = 1 - t;
                 const float angle = float(std::atan2((hit.direction.x + hit.direction.y) * .43,
                                                      (hit.direction.x - hit.direction.y) * .85));
                 const float size = hit.heavy ? 26.f : 14.f;
                 r.EnergyParticle(p,
                                  size * (1 + t),
                                  size * .35f,
                                  angle,
                                  Color(.65f, .9f, 1.f, alpha * .75f),
                                  hit.age,
                                  false);
                 for (int i = 0; i < (hit.heavy ? 7 : 4); ++i)
                 {
                     const float a = angle + (i - 3) * .53f;
                     const float travel = size * (1 + t * 2);
                     const Point end = {p.x + std::cos(a) * travel,
                                        p.y + std::sin(a) * travel * .65f};
                     r.Line({end.x - std::cos(a) * 9 * alpha, end.y - std::sin(a) * 6 * alpha},
                            end,
                            1.5f,
                            Color(.8f, 1.f, 1.f, alpha));
                 }
             }
         });
    for (const auto& effect : skillEffects)
    {
        if (IsStagQ(effect))
        {
            const double length = (std::max)(1., Distance(effect.origin, effect.target));
            const WorldPoint aim = {(effect.target.x - effect.origin.x) / length,
                                    (effect.target.y - effect.origin.y) / length};
            for (int branch = 0; branch < 3; ++branch)
            {
                if (effect.branchEnds[branch] <= .1f ||
                    StagSkill::Rise(effect.age, effect.branchEnds[branch]) <= 0)
                {
                    continue;
                }
                const auto at =
                    StagSkill::Point(effect.origin, aim, branch, effect.branchEnds[branch]);
                bind("effects/stag-bloom/" + std::to_string(effect.id) + "/" +
                         std::to_string(branch),
                     "effects",
                     at,
                     ActorLayer::World,
                     [this, effect, branch](const Actor&)
                     {
                         DrawStagBloom(effect, branch);
                     });
            }
            bind("effects/stag-ground/" + std::to_string(effect.id),
                 "effects",
                 effect.origin,
                 ActorLayer::Warning,
                 [this, effect](const Actor&)
                 {
                     DrawStagQ(effect, true);
                 });
        }
        if (effect.set == 0 && effect.slot == 3)
        {
            bind("effects/domain-back/" + std::to_string(effect.id),
                 "effects",
                 effect.origin,
                 ActorLayer::Warning,
                 [this, effect](const Actor&)
                 {
                     DrawDomain(effect, true);
                 });
        }
        bind("effects/skill/" + std::to_string(effect.id),
             "effects",
             SkillAreaCenter(effect),
             ActorLayer::Projectile,
             [this, effect](const Actor&)
             {
                 DrawSkillEffect(effect);
             });
    }
    if (hasMoveDestination)
    {
        bind("effects/move-target",
             "effects",
             moveDestination,
             ActorLayer::Warning,
             [this](const Actor& node)
             {
                 const auto at = node.WorldPosition();
                 const Point p = Screen(at.x, at.y);
                 r.Line({p.x - 10, p.y}, {p.x, p.y - 5}, 2, Color(.3f, 1, .6f));
                 r.Line({p.x, p.y - 5}, {p.x + 10, p.y}, 2, Color(.3f, 1, .6f));
                 r.Line({p.x + 10, p.y}, {p.x, p.y + 5}, 2, Color(.3f, 1, .6f));
                 r.Line({p.x, p.y + 5}, {p.x - 10, p.y}, 2, Color(.3f, 1, .6f));
             });
    }
    auto& ui = bind("interface/hud",
                    "interface",
                    {0, 0},
                    ActorLayer::Interface,
                    [this](const Actor&)
                    {
                        r.Rect(0, 0, float(width), 10, Color(.02f, .03f, .04f));
                        DrawDomainPortrait();
                        HUD();
                    });
    ui.pointer = [this](int x, int y)
    {
        if (titleScreen || slotMode != SlotMode::Closed)
            return FrontClick(x, y);
        if (!pauseMenu && !journal)
            return GameUIClick(x, y);
        MenuClick(x, y);
        return true;
    };
    if (openingFade > 0)
    {
        auto& fade = bind("interface/opening-fade",
                          "interface",
                          {0, 0},
                          ActorLayer::Interface,
                          [this](const Actor&)
                          {
                              r.Rect(0,
                                     0,
                                     float(width),
                                     float(height),
                                     Color(0, 0, 0, (std::min)(1.f, openingFade / .8f)));
                          });
        fade.depthOffset = 1.e14;
    }
    scene.EndSync();
    sceneReady = true;
}

void Prototype::Update(float dt, bool sprint)
{
    requestedSprint = sprint;
    const bool presentationOnly = titleScreen || openingPlaying;
    UpdatePresentation(dt);
    if (presentationOnly)
    {
        // A completed opening enters the world on the next frame without advancing combat
        // during the final transition. Monitoring keeps its normal one-second cadence.
        if (!sceneReady)
            SynchronizeScene();
        FrameProfiler::Scope timing(FrameProfiler::Stage::Monitoring);
        PublishMonitoring();
        return;
    }
    if (!sceneReady)
        SynchronizeScene();
    scene.Update(dt);
    FrameProfiler::Scope timing(FrameProfiler::Stage::Monitoring);
    PublishMonitoring();
}

void Prototype::Draw()
{
    // Input can load a save or change UI between simulation ticks.
    // Reconcile before rendering, retaining nodes with the same identity.
    SynchronizeScene();
    r.Begin(Color(.035f, .052f, .069f), !titleScreen && !openingPlaying);
    occupiedLabels.clear();
    {
        FrameProfiler::Scope timing(FrameProfiler::Stage::WorldDraw);
        scene.Draw(false);
        r.FinishScene();
    }
    {
        FrameProfiler::Scope timing(FrameProfiler::Stage::InterfaceDraw);
        scene.Draw(true);
        r.Flush();
    }
}
