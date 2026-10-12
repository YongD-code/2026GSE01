#include <windows.h>
#include <fstream>
#include <sstream>
#include <cassert>
#include "../SimpleGame/LevelOne.h"

int main()
{
    auto open = [](WorldPoint) { return false; };
    auto field = [](WorldPoint p, int kind)
    {
        return p.x < 1500 ? (kind == 0 ? 0.f : kind == 1 ? .1f : .2f) : 1.f;
    };
    LevelOne level;
    level.regionLoaded = true;
    level.activeRegion = {World::Index(1000), World::Index(1000)};
    LevelOne::Enemy normal;
    normal.position = normal.home = {1100, 1000};
    normal.type = 0;
    normal.boss = 6; // Quest encounter identifiers are not all actual bosses.
    normal.health = normal.maximum = 100;
    normal.alerted = true;
    normal.phase = 1;
    normal.phaseTime = .03f;
    normal.trail.push_back(normal.home);
    level.enemies.push_back(normal);
    auto boss = normal;
    boss.boss = 1;
    boss.position = boss.home = {1200, 1000};
    level.enemies.push_back(boss);
    level.projectiles.push_back({{1000, 1100}, {440, 0}, 440, 1, false});
    auto update = [&](auto scale)
    {
        level.Update(.05f, {1000, 1000}, {1000, 1000}, false, false,
                     false, 42, open, scale);
    };
    update(field);
    assert(level.enemies[0].phase == 1 && level.enemies[0].phaseTime == .03f);
    assert(std::abs(level.enemies[1].phaseTime - .025f) < .0001f);
    assert(std::abs(level.projectiles[0].position.x - 1004.4) < .001);
    assert(std::abs(level.projectiles[0].remaining - 435.6) < .001);
    update([](WorldPoint, int) { return 1.f; });
    assert(level.enemies[0].phase != 1);
    assert(std::abs(level.projectiles[0].position.x - 1026.4) < .001);

    LevelOne spawning;
    auto obstruction = [](WorldPoint p) { return p.x >= 95 && p.x <= 105 && std::abs(p.y) < 30; };
    assert(!spawning.ClearSpawn({200, 0}, {0, 0}, obstruction));
    assert(!spawning.ClearSpawn({80, 0}, {0, 0}, obstruction));
    assert(spawning.ClearSpawn({0, 100}, {0, 0}, obstruction));
    spawning.EnsureBoss(1, {100, 0}, {0, 0}, obstruction);
    assert(spawning.enemies.size() == 1);
    const auto at = spawning.enemies[0].position;
    assert(!obstruction(at));
    assert(LevelOne::ClearLine({0, 0}, at, obstruction));
    LevelOne sealed;
    sealed.EnsureBoss(1, {100, 0}, {0, 0}, [](WorldPoint) { return true; });
    assert(sealed.enemies.empty());
}
