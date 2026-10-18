#pragma once
#include "World.h"
#include <algorithm>
#include <map>
#include <queue>
#include <vector>
#include <cmath>
#include <functional>
#include <chrono>

namespace ClickNavigation
{
// Callers schedule bounded searches; partial routes can be continued in a later frame.
template <class Collision>
std::vector<WorldPoint> Find(WorldPoint start,
                             WorldPoint goal,
                             Collision blocked,
                             int maximumNodes = 4096,
                             double milliseconds = 0,
                             bool allowPartial = false,
                             WorldPoint* resolvedGoal = nullptr,
                             bool* complete = nullptr)
{
    const auto began = std::chrono::steady_clock::now();
    if (resolvedGoal)
    {
        *resolvedGoal = goal;
    }
    if (complete)
    {
        *complete = false;
    }
    bool exhausted = false;
    auto expired = [&]()
    {
        if (!exhausted && milliseconds > 0)
        {
            const double elapsed =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                    .count();
            exhausted = elapsed >= milliseconds;
        }
        return exhausted;
    };
    auto clear = [&](WorldPoint a, WorldPoint b)
    {
        const int count = (std::max)(1, int(std::ceil(std::hypot(b.x - a.x, b.y - a.y) / 4)));
        for (int i = 1; i <= count; ++i)
        {
            if ((i == 1 || i % 16 == 0) && expired())
            {
                return false;
            }
            const double t = double(i) / count;
            if (blocked({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}))
            {
                return false;
            }
        }
        return true;
    };
    if (blocked(goal))
    {
        bool found = false;
        for (int radius = 16; radius <= 192 && !found; radius += 16)
        {
            for (int i = 0; i < 24; ++i)
            {
                if (expired())
                {
                    return {};
                }
                const double angle = i * 6.283185307 / 24;
                WorldPoint candidate = {goal.x + std::cos(angle) * radius,
                                        goal.y + std::sin(angle) * radius};
                if (!blocked(candidate))
                {
                    goal = candidate;
                    found = true;
                    break;
                }
            }
        }
        if (!found)
        {
            return {};
        }
    }
    if (resolvedGoal)
    {
        *resolvedGoal = goal;
    }
    if (clear(start, goal))
    {
        if (complete)
        {
            *complete = true;
        }
        return {goal};
    }
    using Key = std::pair<int, int>;

    struct Node
    {
        double cost = 1.e30;
        Key parent = {0, 0};
        bool closed = false;
    };

    using Entry = std::pair<double, Key>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    std::map<Key, Node> nodes;
    auto point = [&](Key k)
    {
        return WorldPoint{start.x + k.first * 24, start.y + k.second * 24};
    };
    auto distance = [](WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    };
    nodes[{0, 0}].cost = 0;
    open.push({distance(start, goal), {0, 0}});
    Key last = {0, 0};
    bool reached = false;
    double closest = distance(start, goal);
    for (int visited = 0; !open.empty() && visited < maximumNodes && !expired();)
    {
        Key current = open.top().second;
        open.pop();
        Node& node = nodes[current];
        if (node.closed)
        {
            continue;
        }
        node.closed = true;
        ++visited;
        WorldPoint at = point(current);
        const double remaining = distance(at, goal);
        if (remaining < closest)
        {
            closest = remaining;
            last = current;
        }
        if (remaining < 36 && clear(at, goal))
        {
            last = current;
            reached = true;
            break;
        }
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (x == 0 && y == 0)
                {
                    continue;
                }
                Key next = {current.first + x, current.second + y};
                WorldPoint target = point(next);
                if (!clear(at, target))
                {
                    continue;
                }
                double cost = node.cost + distance(at, target);
                Node& other = nodes[next];
                if (other.closed || cost >= other.cost)
                {
                    continue;
                }
                other.cost = cost;
                other.parent = current;
                open.push({cost + distance(target, goal), next});
            }
        }
    }
    if (!reached && (!allowPartial || closest > distance(start, goal) - 24))
    {
        return {};
    }
    if (complete)
    {
        *complete = reached;
    }
    std::vector<WorldPoint> reverse;
    if (reached)
    {
        reverse.push_back(goal);
    }
    while (last != Key{0, 0})
    {
        reverse.push_back(point(last));
        last = nodes[last].parent;
    }
    std::reverse(reverse.begin(), reverse.end());
    std::vector<WorldPoint> route;
    WorldPoint anchor = start;
    for (size_t i = 0; i < reverse.size();)
    {
        size_t furthest = i;
        // Keep already checked grid edges when the budget is spent on searching.
        while (furthest + 1 < reverse.size() && !expired() && clear(anchor, reverse[furthest + 1]))
        {
            ++furthest;
        }
        route.push_back(reverse[furthest]);
        anchor = reverse[furthest];
        i = furthest + 1;
    }
    return route;
}
} // namespace ClickNavigation
