#include "stdafx.h"
#include "Prototype.h"

void Prototype::DrawTerrain(const World::Chunk& chunk, ActorPosition position)
{
    const std::string terrainKey = "terrain:" + std::to_string(chunk.key.first) + ":" +
                                   std::to_string(chunk.key.second) + ":" +
                                   std::to_string(world.seed);
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
                Point p = Screen(wx + offsetX, wy + offsetY);

                auto hash = World::Hash(tx, ty, 42);
                float shade = float(hash % 15) * .002f;
                Color color =
                    chunk.biome == 0
                        ? Color(.095f + shade, .125f + shade, .13f + shade)
                        : (chunk.biome == 1 ? Color(.12f + shade, .145f + shade, .145f + shade)
                                            : Color(.15f + shade, .14f + shade, .14f + shade));
                if (World::Village(wx, wy))
                    color = Color(.11f + shade, .13f + shade, .13f + shade);
                bool road = World::Road(tx, ty);
                if (road)
                    color = Color(.23f + shade, .23f + shade, .20f + shade);
                Ground(wx + offsetX, wy + offsetY, 64, 64, color);
                if (road)
                    Ground(wx + offsetX + 12 + double(hash % 17),
                           wy + offsetY + 15,
                           16,
                           10,
                           Color(.32f, .31f, .26f, .5f));
                else
                    r.Line(p, {p.x + 3, p.y - 4}, 1, Color(.24f, .27f, .24f, .4f));
            }
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(terrainKey, terrainOrigin);
}

void Prototype::DrawObject(const Object& o)
{
    const Point bounds = Screen(o.x + o.w * .5, o.y + o.d * .5);
    if (bounds.x < -300 || bounds.x > width + 300 || bounds.y < -250 || bounds.y > height + 350)
        return;
    const std::string meshKey = "object:" + std::to_string(o.kind) + ":" + std::to_string(o.w) +
                                ":" + std::to_string(o.d) + ":" + std::to_string(o.h);
    const Point origin = Screen(o.x, o.y);
    if (r.BeginCachedMesh(meshKey, origin))
    {
        if (o.kind == 0)
            House(o);
        else if (o.kind == 1)
            Tree(o);
        else
            Box(o.x,
                o.y,
                o.w,
                o.d,
                o.h,
                Color(.33f, .36f, .35f),
                Color(.23f, .27f, .28f),
                Color(.17f, .21f, .23f));
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(meshKey, origin);
}

void Prototype::SynchronizeScene()
{
    scene.BeginSync();
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

    auto bind = [this](const std::string& id,
                       const std::string& parent,
                       WorldPoint position,
                       ActorLayer layer,
                       std::function<void(const Actor&)> draw) -> FunctionActor&
    {
        auto& actor = scene.Ensure(id, parent);
        actor.SetWorldPosition({position.x, position.y, 0});
        actor.layer = layer;
        actor.draw = std::move(draw);
        return actor;
    };

    auto object = [&](const std::string& id, const std::string& parent, const Object& data)
    {
        auto& actor = bind(id,
                           parent,
                           {data.x, data.y},
                           ActorLayer::World,
                           [this, data](const Actor& node)
                           {
                               auto placed = data;
                               const auto position = node.WorldPosition();
                               placed.x = position.x;
                               placed.y = position.y;
                               DrawObject(placed);
                           });
        actor.depthOffset = data.w + data.d;
    };

    for (size_t i = 0; i < villageObjects.size(); ++i)
        object("village/object/" + std::to_string(i), "village", villageObjects[i]);

    for (const auto& entry : world.chunks)
    {
        const auto& chunk = entry.second;
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
        auto& ground = scene.Ensure(id + "/terrain", id);
        ground.layer = ActorLayer::Terrain;
        ground.draw = [this, terrain](const Actor& node)
        {
            DrawTerrain(terrain, node.WorldPosition());
        };
        for (size_t i = 0; i < chunk.objects.size(); ++i)
            object(id + "/object/" + std::to_string(i), id, chunk.objects[i]);
    }

    bind("village/square",
         "village",
         {-165, -145},
         ActorLayer::Terrain,
         [this](const Actor& node)
         {
             const auto p = node.WorldPosition();
             Ground(p.x, p.y, 330, 290, Color(.24f, .24f, .21f));
         })
        .depthOffset = 1.e12;

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
         ActorLayer::World,
         [this](const Actor& node)
         {
             const auto p = node.WorldPosition();
             Person({p.x, p.y}, Color(.44f, .49f, .52f), true);
         });
    bind("characters/child",
         "characters",
         child,
         ActorLayer::World,
         [this](const Actor& node)
         {
             const auto p = node.WorldPosition();
             Person({p.x, p.y}, Color(.48f, .36f, .29f), false, true);
         });
    bind("characters/keeper",
         "characters",
         keeper,
         ActorLayer::World,
         [this](const Actor& node)
         {
             const auto p = node.WorldPosition();
             Person({p.x, p.y}, Color(.33f, .37f, .34f), false);
         });
    if (captured)
        bind("characters/companion",
             "characters",
             spirit,
             ActorLayer::World,
             [this](const Actor& node)
             {
                 const auto p = node.WorldPosition();
                 Wisp(capturedSpirit, {p.x, p.y});
             });

    for (const auto& enemy : levelOne.enemies)
    {
        const std::string id = enemy.boss > 0 ? "boss/" + std::to_string(enemy.boss)
                                              : "enemy/" + std::to_string(enemy.camp.first) + "/" +
                                                    std::to_string(enemy.camp.second) + "/" +
                                                    std::to_string(enemy.slot);
        // Capture the index-independent rendering state; vector growth/removal
        // must never leave the graph holding a dangling gameplay reference.
        LevelOne::Enemy visual;
        visual.position = enemy.position;
        visual.type = enemy.type;
        visual.health = enemy.health;
        visual.maximum = enemy.maximum;
        visual.flash = enemy.flash;
        visual.animation = enemy.animation;
        visual.boss = enemy.boss;
        visual.alerted = enemy.alerted;
        visual.returning = enemy.returning;
        visual.weakened = enemy.weakened;
        bind(id,
             "enemies",
             enemy.position,
             ActorLayer::World,
             [this, visual](const Actor& node) mutable
             {
                 const auto p = node.WorldPosition();
                 visual.position = {p.x, p.y};
                 DrawEnemy(visual);
             });
    }

    for (auto& item : levelOne.loot)
    {
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

    auto& ui = bind("interface/hud",
                    "interface",
                    {0, 0},
                    ActorLayer::Interface,
                    [this](const Actor&)
                    {
                        r.Rect(0, 0, float(width), 10, Color(.02f, .03f, .04f));
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
    scene.EndSync();
    sceneReady = true;
}

void Prototype::Update(float dt, bool sprint)
{
    requestedSprint = sprint;
    if (!sceneReady)
        SynchronizeScene();
    scene.Update(dt);
}

void Prototype::Draw()
{
    // Input can load a save or change UI between simulation ticks.
    // Reconcile before rendering, retaining nodes with the same identity.
    SynchronizeScene();
    r.Begin(Color(.035f, .052f, .069f));
    scene.Draw(false);
    r.FinishScene();
    scene.Draw(true);
    r.Flush();
}
