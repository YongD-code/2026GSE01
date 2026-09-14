#pragma once
#include "World.h"
#include "SpriteSheet.h"
#include <algorithm>
#include <random>
#include <string>
#include <vector>

// Session-only training progression. World collision is supplied by the host.
class LevelOne
{
  public:
    struct Enemy
    {
        WorldPoint position;
        int type;
        float health, maximum, flash = 0;
        SpriteAnimation animation;
    };

    struct Projectile
    {
        WorldPoint position, velocity;
        float remaining, damage;
        bool companion;
    };

    enum class LootKind
    {
        Experience,
        Weapon,
        Healing,
        Stone
    };

    struct Loot
    {
        WorldPoint position;
        LootKind kind;
        float age = 0;
        bool attracted = false;
    };

    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<Loot> loot;
    int level = 1, experience = 0, kills = 0, weapon = 0, stones = 0, spiritRank = 0;
    int failedUpgrades = 0;
    float health = 100, invulnerability = 0, cooldown = 0, companionCooldown = 0;
    float castTime = 0, spawnTime = 1;
    bool automatic = true, complete = false;
    Facing castFacing = Facing::South;
    std::string notice;
    std::mt19937 random{std::random_device{}()};

    int RequiredExperience() const
    {
        return 24 + (level - 1) * 12;
    }

    float MaximumHealth() const
    {
        return 100.f + (level - 1) * 12.f;
    }

    float Damage() const
    {
        return 18.f + (level - 1) * 3.f + weapon * 5.f;
    }

    float Interval() const
    {
        return (std::max)(.22f, .85f - (level - 1) * .035f);
    }

    float Range() const
    {
        return 300.f + (std::min)(level - 1, 10) * 6.f;
    }

    float MagnetRange() const
    {
        return 120.f + (std::min)(level - 1, 10) * 8.f;
    }

    bool Dead() const
    {
        return health <= 0;
    }

    float Roll()
    {
        return std::uniform_real_distribution<float>(0.f, 1.f)(random);
    }

    static double Distance(WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    template <class Collision> static bool ClearLine(WorldPoint a, WorldPoint b, Collision blocked)
    {
        const int steps = (std::max)(1, static_cast<int>(std::ceil(Distance(a, b) / 6)));
        for (int i = 1; i <= steps; ++i)
        {
            const double t = double(i) / steps;
            if (blocked({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}))
                return false;
        }
        return true;
    }

    template <class Collision>
    static void Move(WorldPoint& position, WorldPoint delta, Collision blocked)
    {
        const int steps =
            (std::max)(1, static_cast<int>(std::ceil(std::hypot(delta.x, delta.y) / 5)));
        for (int i = 0; i < steps; ++i)
        {
            WorldPoint next = {position.x + delta.x / steps, position.y};
            if (!blocked(next))
                position = next;
            next = {position.x, position.y + delta.y / steps};
            if (!blocked(next))
                position = next;
        }
    }

    void Upgrade(bool captured)
    {
        if (Dead())
            return;
        if (!captured)
        {
            notice = "주령석은 보관됩니다. 먼저 E로 주령을 포획하세요.";
            return;
        }
        if (spiritRank >= 10)
        {
            notice = "주령이 최대 강화에 도달했습니다.";
            return;
        }
        if (stones <= 0)
        {
            notice = "강화에 필요한 주령석이 없습니다.";
            return;
        }
        --stones;
        if (failedUpgrades >= 3 || Roll() < .7f)
        {
            ++spiritRank;
            failedUpgrades = 0;
            notice = "주령 강화 성공! 동행 주술의 피해량이 증가했습니다.";
        }
        else
        {
            ++failedUpgrades;
            notice = "강화 실패. 등급은 유지됩니다. 3회 연속 실패 후 다음 강화는 확정 성공입니다.";
        }
    }

    void Collect(LootKind kind)
    {
        switch (kind)
        {
        case LootKind::Experience:
            if (level >= 20)
                break;
            experience += 12;
            while (level < 20 && experience >= RequiredExperience())
            {
                experience -= RequiredExperience();
                ++level;
                health = (std::min)(MaximumHealth(), health + 30.f);
                notice = "레벨 상승! 공격력·최대 체력·흡수 범위 증가, 발사 대기시간 감소.";
            }
            if (level == 20)
                experience = 0;
            break;
        case LootKind::Weapon:
            if (weapon < 20)
                ++weapon;
            notice = "강화 파편 흡수! 무기 피해량이 증가했습니다.";
            break;
        case LootKind::Healing:
            health = (std::min)(MaximumHealth(), health + MaximumHealth() * .3f);
            notice = "생명의 불씨를 흡수해 최대 체력의 30%를 회복했습니다.";
            break;
        case LootKind::Stone:
            ++stones;
            notice = "주령석 획득! G로 동행 주령을 강화하세요. 성공 확률 70%.";
            break;
        }
    }

    template <class Collision> bool Shoot(WorldPoint origin, bool companion, Collision blocked)
    {
        Enemy* target = nullptr;
        double nearest = Range();
        for (Enemy& enemy : enemies)
        {
            const double distance = Distance(origin, enemy.position);
            if (enemy.health > 0 && distance <= nearest &&
                ClearLine(origin, enemy.position, blocked))
            {
                nearest = distance;
                target = &enemy;
            }
        }
        if (!target)
            return false;
        const double dx = target->position.x - origin.x, dy = target->position.y - origin.y;
        const double length = (std::max)(.001, std::hypot(dx, dy));
        projectiles.push_back({origin,
                               {dx / length * 440, dy / length * 440},
                               Range(),
                               companion ? 10.f + spiritRank * 6.f : Damage(),
                               companion});
        if (!companion)
        {
            SpriteAnimation aim;
            aim.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
            castFacing = aim.facing;
            castTime = .18f;
        }
        return true;
    }

    template <class Collision>
    void Update(float dt,
                WorldPoint player,
                WorldPoint companion,
                bool captured,
                bool controlling,
                bool fireHeld,
                Collision blocked)
    {
        if (Dead())
            return;
        invulnerability = (std::max)(0.f, invulnerability - dt);
        castTime = (std::max)(0.f, castTime - dt);
        cooldown = (std::max)(0.f, cooldown - dt);
        companionCooldown = (std::max)(0.f, companionCooldown - dt);
        spawnTime -= dt;
        // Keep the starting hearth safe; spawn only with an unobstructed route to the player.
        if (spawnTime <= 0 && enemies.size() < 18)
        {
            spawnTime = complete ? 2.f : 2.8f;
            for (int attempt = 0; attempt < 32; ++attempt)
            {
                const double angle = Roll() * 6.283185307;
                const double radius = 250 + Roll() * 150;
                WorldPoint position = {player.x + std::cos(angle) * radius,
                                       player.y + std::sin(angle) * radius};
                if (Distance(position, {0, 0}) < 240 || blocked(position) ||
                    !ClearLine(position, player, blocked))
                    continue;
                bool occupied = false;
                for (const Enemy& enemy : enemies)
                    if (Distance(position, enemy.position) < 50)
                        occupied = true;
                if (occupied)
                    continue;
                const int type = static_cast<int>(random() % 3);
                const float maximum = 38.f + type * 8.f + (complete ? 20.f : 0.f);
                enemies.push_back({position, type, maximum, maximum});
                break;
            }
        }
        for (Enemy& enemy : enemies)
        {
            enemy.flash = (std::max)(0.f, enemy.flash - dt);
            const WorldPoint previous = enemy.position;
            const double distance = Distance(player, enemy.position);
            if (distance > 24 && Distance(player, {0, 0}) > 210)
            {
                const double speed = 42 + enemy.type * 5;
                Move(enemy.position,
                     {(player.x - enemy.position.x) / distance * speed * dt,
                      (player.y - enemy.position.y) / distance * speed * dt},
                     blocked);
            }
            const double dx = enemy.position.x - previous.x, dy = enemy.position.y - previous.y;
            enemy.animation.Update((dx - dy) * .85, (dx + dy) * .43, dt, false);
            if (Distance(player, enemy.position) < 28 && invulnerability <= 0 &&
                Distance(player, {0, 0}) > 210)
            {
                health = (std::max)(0.f, health - (10.f + enemy.type * 2));
                invulnerability = .9f;
                if (Dead())
                {
                    notice = "불씨가 꺼졌습니다. R로 레벨 1을 다시 시작하세요.";
                    projectiles.clear();
                    return;
                }
            }
        }
        if (!controlling && cooldown <= 0 && (automatic || fireHeld) &&
            Shoot(player, false, blocked))
            cooldown = Interval();
        if (captured && companionCooldown <= 0 && Shoot(companion, true, blocked))
            companionCooldown = (std::max)(.45f, 1.5f - spiritRank * .08f);
        for (Projectile& shot : projectiles)
        {
            float remainingTime = dt;
            while (remainingTime > 0 && shot.remaining > 0)
            {
                const float step =
                    (std::min)((std::min)(remainingTime, .008f), shot.remaining / 440.f);
                if (step <= 0)
                    break;
                remainingTime -= step;
                shot.remaining = (std::max)(0.f, shot.remaining - 440.f * step);
                shot.position.x += shot.velocity.x * step;
                shot.position.y += shot.velocity.y * step;
                if (blocked(shot.position))
                {
                    shot.remaining = 0;
                    break;
                }
                for (Enemy& enemy : enemies)
                {
                    if (enemy.health <= 0 || Distance(shot.position, enemy.position) > 22)
                        continue;
                    enemy.health -= shot.damage;
                    enemy.flash = .15f;
                    shot.remaining = 0;
                    if (enemy.health <= 0)
                    {
                        ++kills;
                        loot.push_back({enemy.position, LootKind::Experience});
                        if (kills == 1 || Roll() < .35f)
                            loot.push_back({enemy.position, LootKind::Weapon});
                        if (Roll() < .25f)
                            loot.push_back({enemy.position, LootKind::Healing});
                        if (Roll() < .12f)
                            loot.push_back({enemy.position, LootKind::Stone});
                    }
                    break;
                }
            }
        }
        projectiles.erase(std::remove_if(projectiles.begin(),
                                         projectiles.end(),
                                         [](const Projectile& p)
                                         {
                                             return p.remaining <= 0;
                                         }),
                          projectiles.end());
        enemies.erase(std::remove_if(enemies.begin(),
                                     enemies.end(),
                                     [&](const Enemy& e)
                                     {
                                         return e.health <= 0 || Distance(e.position, player) > 950;
                                     }),
                      enemies.end());
        const WorldPoint collector = controlling ? companion : player;
        for (Loot& item : loot)
        {
            item.age += dt;
            const double distance = Distance(item.position, collector);
            if (distance < MagnetRange())
                item.attracted = true;
            // Pickups are ethereal: attraction passes through scenery to avoid unreachable rewards.
            if (item.attracted && distance > 0)
            {
                const double t = (std::min)(1.0, (180 + item.age * 30) * dt / distance);
                item.position.x += (collector.x - item.position.x) * t;
                item.position.y += (collector.y - item.position.y) * t;
            }
            if (Distance(item.position, collector) < 18)
            {
                Collect(item.kind);
                item.age = 1000;
            }
        }
        loot.erase(std::remove_if(loot.begin(),
                                  loot.end(),
                                  [](const Loot& item)
                                  {
                                      return item.age >= 90;
                                  }),
                   loot.end());
        if (!complete && level >= 3 && kills >= 12 && weapon >= 1)
        {
            complete = true;
            health = MaximumHealth();
            notice = "레벨 1 완료! 파밍의 불씨를 익혔습니다. 체력 회복 후 자유 파밍을 계속할 수 "
                     "있습니다.";
        }
    }
};
