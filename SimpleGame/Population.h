#pragma once
#include "World.h"
#include "SpriteSheet.h"
#include "ClickNavigation.h"
#include "VillageLayout.h"
#include "NpcAppearance.h"
#include "SpiritCatalog.h"
#include <array>
#include <string>
#include <vector>

// Persistent people; SceneGraph owns their presentation. Decisions are staggered and paths bounded.
class Population
{
  public:
    enum Role
    {
        Farmer,
        Merchant,
        Artisan,
        Hunter
    };

    enum Action
    {
        Walking,
        Working,
        Trading,
        Hunting,
        Fighting,
        Returning,
        Resting,
        Recovering,
        Fleeing,
        Delivering,
        MovingHome
    };

    struct News
    {
        unsigned serial = 0;
        int origin = -1;
        double created = 0;
        float safety = .8f, jobs = .5f, wages = .5f;
    };

    struct Letter
    {
        unsigned id = 0;
        int carrier = -1, from = 0, to = 0;
        News news;
        bool delivered = false;
    };

    struct Society
    {
        double clock = 0;
        unsigned nextNews = 1, nextLetter = 1, delivered = 0, migrations = 0;
        std::array<std::array<News, World::VillageCount>, World::VillageCount> boards = {};
        std::vector<Letter> letters;
        float dispatchWait = 6, reportWait = 0, contactWait = 0;
    };

    struct Person
    {
        int id = 0, role = Farmer, district = 0, gold = 20, earned = 0, kills = 0, goods = 0;
        WorldPoint position = {0, 0}, destination = {0, 0}, home = {0, 0}, workplace = {0, 0};
        float health = 100, stamina = 100, timer = 0, decision = 0, attackWait = 0,
              invulnerability = 0;
        float pathWait = 0, simulationTime = 0;
        WorldPoint visualVelocity = {0, 0}, separation = {0, 0};
        float stopTime = 0;
        bool locomotionInitialized = false;
        int action = Walking, nextAction = Working, target = -1;
        unsigned cycle = 0;
        std::vector<WorldPoint> route;
        size_t waypoint = 0;
        SpriteAnimation animation;
        NpcAppearance::Definition appearance;
        int houseId = -1, reservedHouse = -1, migrationVillage = -1, mailId = -1;
        float ambition = .5f, caution = .5f, sociability = .5f, migrationWait = 90;
        unsigned migrations = 0;
        bool questPinned = false;
        std::array<News, World::VillageCount> knowledge = {};
    };

    std::vector<Person> people;
    std::uint64_t seed = 0;
    size_t planningCursor = 0;
    Society society;

    static const std::array<WorldPoint, World::VillageCount>& Hubs()
    {
        return World::Settlements();
    }

    static const char* DistrictName(int district)
    {
        return World::VillageName(district);
    }

    static const char* RoleId(int role)
    {
        static const char* names[] = {"farmer", "merchant", "artisan", "hunter"};
        return names[role];
    }

    static const char* RoleName(int role)
    {
        static const char* names[] = {"채집꾼", "행상인", "장인", "주령 사냥꾼"};
        return names[role];
    }

    static const char* ActionId(int action)
    {
        static const char* names[] = {"walking",
                                      "working",
                                      "trading",
                                      "hunting",
                                      "fighting",
                                      "returning",
                                      "resting",
                                      "recovering",
                                      "fleeing",
                                      "delivering",
                                      "moving_home"};
        return names[action];
    }

    static const char* ActionName(int action)
    {
        static const char* names[] = {"이동 중",
                                      "일하는 중",
                                      "거래 중",
                                      "주령 수색",
                                      "사냥 중",
                                      "귀환 중",
                                      "휴식 중",
                                      "회복 중",
                                      "대피 중",
                                      "편지 운반",
                                      "이사 중"};
        return names[action];
    }

    static std::string Name(int id)
    {
        static const char* family[] = {
            "아사노",   "미즈노",   "하야시",   "모리",     "시마다",   "나카무라", "타카하시",
            "코바야시", "이노우에", "마츠다",   "후지타",   "오가와",   "미우라",   "이시다",
            "후쿠다",   "오카다",   "아오키",   "사카모토", "하시모토", "요시다",   "야마구치",
            "나카지마", "모리타",   "스기야마", "사이토",   "이시카와"};
        static const char* given[] = {"하루", "유이", "소타", "미오", "렌", "아키", "나오", "린"};
        return std::string(family[(id / 8) % 26]) + " " + given[id % 8];
    }

    static WorldPoint PostOffice(int village)
    {
        const auto at = Hubs()[village];
        return {at.x, at.y + 96};
    }

    static bool TownInterior(WorldPoint p)
    {
        return World::Settlement(p.x, p.y);
    }

    static WorldPoint CampAnchor(int district, int id, bool workplace = false)
    {
        const auto hub = Hubs()[district];
        if (workplace)
        {
            return {hub.x - 350 + (id % 5) * 160., hub.y + 250 + (id % 3) * 35.};
        }
        return PostOffice(district);
    }

    static WorldPoint RoutingGoal(WorldPoint start, WorldPoint destination)
    {
        return VillageLayout::RoutingGoal(start, destination);
    }

    template <class Collision> static WorldPoint OpenPoint(WorldPoint desired, Collision blocked)
    {
        if (!blocked(desired))
        {
            return desired;
        }
        for (int ring = 1; ring <= 20; ++ring)
        {
            for (int i = 0; i < 16; ++i)
            {
                const double angle = i * 6.283185307179586 / 16;
                WorldPoint p = {desired.x + std::cos(angle) * ring * 20,
                                desired.y + std::sin(angle) * ring * 20};
                if (!blocked(p))
                {
                    return p;
                }
            }
        }
        return desired;
    }

    void Travel(Person& p, WorldPoint goal, int arrival, int action = Walking)
    {
        const bool changed = p.action != action || p.nextAction != arrival ||
                             std::hypot(goal.x - p.destination.x, goal.y - p.destination.y) > 72;
        p.destination = goal;
        p.nextAction = arrival;
        p.action = action;
        if (changed)
        {
            p.route.clear();
            p.waypoint = 0;
        }
    }

#include "VillageSociety.inl"

    static void UpdateLocomotion(Person& p, WorldPoint previous, float dt)
    {
        const double dx = p.position.x - previous.x, dy = p.position.y - previous.y;
        const double distance = std::hypot(dx, dy);
        const bool first = !p.locomotionInitialized;
        if (first)
        {
            p.animation.phase = std::fmod(p.id * .731f, 8.f);
            p.locomotionInitialized = true;
        }
        if (distance > 64)
        {
            p.visualVelocity = p.separation = {0, 0};
            p.animation.moving = false;
            p.stopTime = .1f;
            return;
        }
        const double response = 1 - std::exp(-dt * 18);
        const double inverseTime = dt > .0001f ? 1. / dt : 0;
        p.visualVelocity.x += ((dx - dy) * .85 * inverseTime - p.visualVelocity.x) * response;
        p.visualVelocity.y += ((dx + dy) * .43 * inverseTime - p.visualVelocity.y) * response;
        if (distance > .03)
        {
            p.stopTime = 0;
            p.animation.moving = true;
            const double stride = p.role == Hunter ? 50. : p.role == Merchant ? 48. : 44.;
            p.animation.phase = std::fmod(p.animation.phase + float(distance * 8 / stride), 8.f);
            const double quarter = .7853981633974483;
            // Face the displacement actually taken this step. A filtered velocity can
            // still point backwards just after a turn or when leaving a blocked route.
            const double angle = std::atan2(-(dx - dy) * .85, (dx + dy) * .43);
            const double difference =
                std::remainder(angle - int(p.animation.facing) * quarter, 6.283185307179586);
            // Small steering/separation corrections must not flip the portrait every step.
            if (first || std::abs(difference) > quarter * .5 + .12)
            {
                const int direction = (int(std::floor(angle / quarter + .5)) + 8) % 8;
                p.animation.facing = static_cast<Facing>(direction);
            }
        }
        else
        {
            p.stopTime += dt;
            if (p.stopTime >= .09f)
                p.animation.moving = false;
        }
    }

    template <class Level, class Collision>
    void Update(float dt,
                WorldPoint player,
                Level& level,
                Collision worldBlocked,
                double fullRateRadius = 1400)
    {
        if (people.empty() || dt <= 0)
            return;
        auto blocked = [&](WorldPoint p)
        {
            return worldBlocked(p);
        };
        UpdateSociety(dt, level, worldBlocked);
        int plans = 2;
        std::map<World::Key, std::vector<size_t>> neighbors;
        for (size_t i = 0; i < people.size(); ++i)
        {
            const auto at = people[i].position;
            neighbors[{World::Coordinate(std::floor(at.x / 64)),
                       World::Coordinate(std::floor(at.y / 64))}]
                .push_back(i);
        }
        // Rotating traversal prevents a blocked early agent from taking every pathfinding slot.
        for (size_t n = 0; n < people.size(); ++n)
        {
            auto& p = people[(planningCursor + n) % people.size()];
            p.invulnerability = (std::max)(0.f, p.invulnerability - dt);
            // Keep visible movement continuous; distant jobs and hunts still advance in time.
            const double playerDistance = Level::Distance(p.position, player);
            p.simulationTime += dt;
            if (playerDistance > fullRateRadius && p.simulationTime < .75f && p.health > 0)
            {
                continue;
            }
            const float stepDt = p.simulationTime;
            p.simulationTime = 0;
            const WorldPoint previous = p.position;
            p.attackWait = (std::max)(0.f, p.attackWait - stepDt);
            p.pathWait = (std::max)(0.f, p.pathWait - stepDt);
            p.decision -= stepDt;
            p.timer -= stepDt;
            const bool resting = p.action == Resting || p.action == Recovering;
            const bool journey = p.action == Delivering || p.action == MovingHome;
            p.stamina = (std::max)(0.f,
                                   (std::min)(100.f,
                                              p.stamina + stepDt * (resting   ? 7.f
                                                                    : journey ? -.16f
                                                                              : -.65f)));
            if (p.health <= 0)
            {
                // Defeated residents recuperate at home rather than disappearing permanently.
                p.health = 1;
                p.position = p.home;
                p.action = Recovering;
                p.timer = 20;
                p.gold = (std::max)(0, p.gold - 8);
                p.route.clear();
                p.target = -1;
            }
            else if (resting)
                p.health = (std::min)(100.f, p.health + stepDt * 6.f);
            if (p.decision <= 0)
            {
                p.decision = playerDistance > fullRateRadius ? 2.f : .4f;
                int threat = -1, prey = -1;
                double danger = 160, best = 390;
                for (size_t i = 0; i < level.enemies.size(); ++i)
                {
                    const auto& enemy = level.enemies[i];
                    if (enemy.boss != 0 || enemy.weakened || enemy.returning || enemy.health <= 0 ||
                        TownInterior(enemy.position) || enemy.type == SpiritCatalog::Stag)
                        continue;
                    const double d = Level::Distance(p.position, enemy.position);
                    if (d < danger)
                    {
                        danger = d;
                        threat = int(i);
                    }
                    // Never take quest bosses, remnants or a fight close to the player.
                    if (p.role == Hunter && d < best &&
                        Level::Distance(player, enemy.position) > 380 &&
                        (!enemy.alerted || enemy.npcTarget >= 0) &&
                        Level::ClearLine(p.position, enemy.position, blocked))
                    {
                        best = d;
                        prey = int(i);
                    }
                }
                p.target = -1;
                if (p.health < 30 || p.stamina < 15 || (p.role != Hunter && threat >= 0))
                {
                    if (p.action != Fleeing && !resting)
                        Travel(p, p.home, Resting, Fleeing);
                }
                else if (prey >= 0 && !resting && p.mailId < 0 && p.migrationVillage < 0)
                {
                    auto& enemy = level.enemies[prey];
                    p.target = prey;
                    if (best < 145 && Level::ClearLine(p.position, enemy.position, blocked))
                    {
                        p.action = Fighting;
                        p.destination = enemy.position;
                        p.route.clear();
                        if (p.attackWait <= 0)
                        {
                            const bool fatal = enemy.health <= 17;
                            level.DamageEnemy(enemy, 17, p.position, false, false, true);
                            p.attackWait = 1.1f;
                            p.stamina = (std::max)(0.f, p.stamina - 2);
                            if (fatal)
                            {
                                ++p.kills;
                                ++p.goods;
                                p.gold += 12;
                                p.earned += 12;
                                p.action = Hunting;
                                p.timer = 2;
                            }
                        }
                    }
                    else
                        Travel(p, enemy.position, Hunting);
                }
                else if (p.action == Fighting)
                {
                    p.action = Hunting;
                    p.timer = 0;
                }
                if (p.timer <= 0 &&
                    (p.action == Resting || p.action == Recovering || p.action == Working ||
                     p.action == Trading || p.action == Hunting))
                {
                    if ((p.action == Resting || p.action == Recovering) &&
                        (p.health < 85 || p.stamina < 80))
                        p.timer = 3;
                    else if (p.action == Working)
                    {
                        const int wage = int((p.role == Artisan ? 9 : 6) * WageScale(p.district));
                        p.gold += wage;
                        p.earned += wage;
                        ++p.goods;
                        Travel(p,
                               p.cycle % 2 ? PostOffice(p.district) : p.home,
                               p.cycle % 2 ? Trading : Resting,
                               Returning);
                    }
                    else if (p.action == Trading)
                    {
                        const int revenue = 8 + p.goods * 3;
                        p.gold += revenue;
                        p.earned += revenue;
                        p.goods = 0;
                        Travel(p, p.home, Resting, Returning);
                    }
                    else if (p.role == Merchant)
                    {
                        const int destination = VillageLayout::NextVillage(
                            p.district, (p.district + 1 + int(p.cycle % 5)) % World::VillageCount);
                        if (p.goods == 0)
                        {
                            p.goods = (std::min)(2 + int(p.cycle % 3), p.gold / 2);
                            p.gold -= p.goods * 2;
                        }
                        Travel(p, OpenPoint(CampAnchor(destination, p.id, true), blocked), Trading);
                    }
                    else if (p.role == Hunter)
                    {
                        const double angle = (p.id * 2.39996 + ++p.cycle * 1.7);
                        const auto hub = Hubs()[p.district];
                        Travel(
                            p,
                            OpenPoint({hub.x + std::cos(angle) * (World::VillageHalfSize + 260.),
                                       hub.y + std::sin(angle) * (World::VillageHalfSize + 260.)},
                                      blocked),
                            Hunting);
                    }
                    else
                        Travel(p, p.workplace, Working);
                    ++p.cycle;
                }
            }
            if (p.action == Walking || p.action == Returning || p.action == Fleeing ||
                p.action == Delivering || p.action == MovingHome)
            {
                if (Level::Distance(p.position, p.destination) < 14)
                {
                    FinishTravel(p, blocked);
                    p.action = p.nextAction;
                    p.timer = p.action == Resting ? 10.f + p.id % 7 : 5.f + p.id % 6;
                    p.route.clear();
                }
                else
                {
                    const bool finishingSegment =
                        !p.route.empty() && p.waypoint + 1 >= p.route.size() &&
                        Level::Distance(p.position, p.route.back()) < 45 &&
                        Level::Distance(p.route.back(), p.destination) > 18;
                    if ((p.waypoint >= p.route.size() || finishingSegment) && p.pathWait <= 0 &&
                        plans > 0)
                    {
                        const auto goal = RoutingGoal(p.position, p.destination);
                        const double d = Level::Distance(p.position, goal);
                        if (d < .01)
                        {
                            p.pathWait = .5f;
                            p.animation.moving = false;
                            p.stopTime = .1f;
                            continue;
                        }
                        const double t = (std::min)(1., 260. / d);
                        const WorldPoint step = {p.position.x + (goal.x - p.position.x) * t,
                                                 p.position.y + (goal.y - p.position.y) * t};
                        auto route =
                            ClickNavigation::Find(p.position, step, blocked, 192, 1.5, true);
                        p.pathWait = .3f;
                        --plans;
                        if (!route.empty())
                        {
                            p.route = std::move(route);
                            p.waypoint = 0;
                        }
                        else if (p.waypoint >= p.route.size())
                        {
                            p.pathWait = 1.f + p.id % 4 * .15f;
                            p.destination = OpenPoint(p.destination, blocked);
                        }
                    }
                    if (p.waypoint < p.route.size())
                    {
                        WorldPoint separation = {0, 0};
                        const World::Key cell = {World::Coordinate(std::floor(p.position.x / 64)),
                                                 World::Coordinate(std::floor(p.position.y / 64))};
                        for (int y = -1; y <= 1; ++y)
                        {
                            for (int x = -1; x <= 1; ++x)
                            {
                                const auto found =
                                    neighbors.find({cell.first + x, cell.second + y});
                                if (found == neighbors.end())
                                {
                                    continue;
                                }
                                for (const auto index : found->second)
                                {
                                    const auto& other = people[index];
                                    const double distance =
                                        Level::Distance(p.position, other.position);
                                    if (other.id != p.id && distance > .01 && distance < 24)
                                    {
                                        separation.x += (p.position.x - other.position.x) /
                                                        distance * (24 - distance);
                                        separation.y += (p.position.y - other.position.y) /
                                                        distance * (24 - distance);
                                    }
                                }
                            }
                        }
                        const double spread = std::hypot(separation.x, separation.y);
                        if (spread > 14)
                        {
                            separation.x *= 14 / spread;
                            separation.y *= 14 / spread;
                        }
                        const double ease = 1 - std::exp(-stepDt * 10);
                        p.separation.x += (separation.x - p.separation.x) * ease;
                        p.separation.y += (separation.y - p.separation.y) * ease;
                        const double speed = p.action == Fleeing ? 92.
                                             : p.role == Hunter  ? 78.
                                                                 : 53.;
                        double remaining = stepDt * speed;
                        // Spend the rest of this step on the next waypoint instead of pausing
                        // for a frame at every short path segment.
                        for (int hop = 0;
                             hop < 8 && p.waypoint < p.route.size() && remaining > .001;
                             ++hop)
                        {
                            const auto target = p.route[p.waypoint];
                            const double d = Level::Distance(p.position, target);
                            if (d < .01)
                            {
                                ++p.waypoint;
                                continue;
                            }
                            const double travel = (std::min)(d, remaining);
                            const WorldPoint before = p.position;
                            const double portion = travel / speed;
                            Level::Move(
                                p.position,
                                {(target.x - p.position.x) / d * travel + p.separation.x * portion,
                                 (target.y - p.position.y) / d * travel + p.separation.y * portion},
                                blocked);
                            remaining -= travel;
                            if (Level::Distance(before, p.position) < .001)
                            {
                                p.route.clear();
                                p.pathWait = (std::max)(p.pathWait, .3f);
                                break;
                            }
                            if (Level::Distance(p.position, target) < 4)
                                ++p.waypoint;
                            else
                                break;
                        }
                    }
                }
            }
            UpdateLocomotion(p, previous, stepDt);
        }
        planningCursor = (planningCursor + 1) % people.size();
    }
};
