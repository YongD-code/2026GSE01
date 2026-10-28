#pragma once
#include "Actor.h"
#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <stdexcept>
#include <vector>

// Owns all actors; parent links never own memory. Structural changes happen
// between traversal calls, never from an Actor callback.
class SceneGraph
{
  public:
    size_t Size() const
    {
        return actors.size();
    }

    Actor* Find(const std::string& id) const
    {
        const auto found = actors.find(id);
        return found == actors.end() ? nullptr : found->second.get();
    }

    template <class T = FunctionActor>
    T& Ensure(const std::string& id, const std::string& parentId = "")
    {
        CheckMutation();
        orderDirty = true;
        Actor* parent = parentId.empty() ? nullptr : Find(parentId);
        if (!parentId.empty() && !parent)
            throw std::logic_error("SceneGraph parent does not exist");
        Actor* existing = Find(id);
        if (!existing)
        {
            std::unique_ptr<T> created(new T());
            created->id = id;
            created->sequence = nextSequence++;
            existing = created.get();
            actors.emplace(id, std::move(created));
        }
        T* typed = dynamic_cast<T*>(existing);
        if (!typed)
            throw std::logic_error("SceneGraph actor type mismatch");
        for (Actor* node = parent; node; node = node->parent)
        {
            if (node == existing)
                throw std::logic_error("SceneGraph cycle");
        }
        existing->parent = parent;
        existing->generation = generation;
        return *typed;
    }

    bool Reparent(const std::string& id, const std::string& parentId)
    {
        CheckMutation();
        Actor* actor = Find(id);
        Actor* parent = parentId.empty() ? nullptr : Find(parentId);
        if (!actor || (!parentId.empty() && !parent))
            return false;
        for (Actor* node = parent; node; node = node->parent)
        {
            if (node == actor)
                return false;
        }
        const ActorPosition position = actor->WorldPosition();
        actor->parent = parent;
        actor->SetWorldPosition(position);
        orderDirty = true;
        return true;
    }

    void Remove(const std::string& id)
    {
        CheckMutation();
        Actor* target = Find(id);
        if (!target)
            return;
        std::vector<std::string> removed;
        for (const auto& entry : actors)
        {
            for (const Actor* node = entry.second.get(); node; node = node->parent)
            {
                if (node == target)
                {
                    removed.push_back(entry.first);
                    break;
                }
            }
        }
        for (const auto& key : removed)
            actors.erase(key);
        orderDirty = true;
    }

    void BeginSync()
    {
        CheckMutation();
        ++generation;
        orderDirty = true;
    }

    void EndSync()
    {
        CheckMutation();
        std::vector<std::string> stale;
        for (const auto& entry : actors)
        {
            // Determine the entire removal set before erasing any parent pointers.
            for (const Actor* node = entry.second.get(); node; node = node->parent)
            {
                if (node->generation != generation)
                {
                    stale.push_back(entry.first);
                    break;
                }
            }
        }
        for (const auto& id : stale)
            actors.erase(id);
        orderDirty = true;
    }

    void Update(float dt)
    {
        Traversal guard(*this);
        orderDirty = true;
        std::vector<Actor*> ordered;
        ordered.reserve(actors.size());
        for (const auto& entry : actors)
        {
            ordered.push_back(entry.second.get());
        }
        // Ancestors update before descendants regardless of drawing layer.
        std::sort(ordered.begin(),
                  ordered.end(),
                  [](Actor* a, Actor* b)
                  {
                      const int aDepth = HierarchyDepth(a), bDepth = HierarchyDepth(b);
                      return aDepth == bDepth ? a->sequence < b->sequence : aDepth < bDepth;
                  });
        for (Actor* actor : ordered)
        {
            if (actor->IsEnabled())
                actor->Update(dt);
        }
        // Callbacks may change publicly accessible transforms, layers or depth offsets.
        orderDirty = true;
    }

    void Draw(bool interfacePass)
    {
        Traversal guard(*this);
        // Rebuild on the first pass; the following label/UI pass shares exactly this ordering.
        if (!interfacePass)
            orderDirty = true;
        for (Actor* actor : Ordered())
        {
            // Labels render after scene post-processing, before menus and HUD.
            if ((actor->layer >= ActorLayer::WorldLabel) == interfacePass && actor->IsVisible())
                actor->Draw();
        }
    }

    bool PointerDown(int x, int y)
    {
        Traversal guard(*this);
        orderDirty = true;
        const auto ordered = Ordered();
        orderDirty = true;
        for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
        {
            if ((*it)->IsVisible() && (*it)->PointerDown(x, y))
                return true;
        }
        return false;
    }

  private:
    std::map<std::string, std::unique_ptr<Actor>> actors;
    unsigned long long generation = 0, nextSequence = 0;
    bool traversing = false;
    bool orderDirty = true;
    std::vector<Actor*> drawOrder;

    struct Traversal
    {
        SceneGraph& graph;

        explicit Traversal(SceneGraph& owner) : graph(owner)
        {
            graph.CheckMutation();
            graph.traversing = true;
        }

        ~Traversal()
        {
            graph.traversing = false;
        }
    };

    void CheckMutation() const
    {
        if (traversing)
            throw std::logic_error("SceneGraph cannot mutate during traversal");
    }

    static int HierarchyDepth(const Actor* actor)
    {
        int depth = 0;
        for (; actor->Parent(); actor = actor->Parent())
            ++depth;
        return depth;
    }

    static ActorLayer RenderLayer(ActorLayer layer)
    {
        // Bodies and scenery share one world pass; Character is a semantic category.
        return layer == ActorLayer::Character ? ActorLayer::World : layer;
    }

    struct DrawEntry
    {
        Actor* actor;
        ActorLayer layer;
        double depth;
        double minX, minY, maxX, maxY;
        double minSide, maxSide;
    };

    static void OrderWorld(std::vector<DrawEntry>& entries)
    {
        size_t first = 0;
        while (first < entries.size() && entries[first].layer < ActorLayer::World)
        {
            ++first;
        }
        size_t end = first;
        while (end < entries.size() && entries[end].layer == ActorLayer::World)
        {
            ++end;
        }
        const size_t count = end - first;
        if (count < 2)
        {
            return;
        }

        // Side projection is x-y. Only horizontally overlapping ground projections
        // can occlude each other; sweep them before creating footprint dependencies.
        std::vector<size_t> candidates;
        candidates.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& entry = entries[first + i];
            if (entry.actor->footprint.enabled && entry.actor->IsVisible())
            {
                candidates.push_back(i);
            }
        }
        std::sort(candidates.begin(),
                  candidates.end(),
                  [&](size_t a, size_t b)
                  {
                      const double left = entries[first + a].minSide;
                      const double right = entries[first + b].minSide;
                      return left == right ? a < b : left < right;
                  });

        std::vector<std::vector<size_t>> after(count);
        std::vector<size_t> incoming(count, 0);
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            const size_t aIndex = candidates[i];
            const auto& a = entries[first + aIndex];
            for (size_t j = i + 1; j < candidates.size(); ++j)
            {
                const size_t bIndex = candidates[j];
                const auto& b = entries[first + bIndex];
                if (b.minSide > a.maxSide)
                {
                    break;
                }
                const bool aBehind = a.maxX <= b.minX || a.maxY <= b.minY;
                const bool bBehind = b.maxX <= a.minX || b.maxY <= a.minY;
                if (aBehind == bBehind)
                {
                    // Overlapping footprints or opposite diagonal separation have
                    // no unambiguous dependency. Retain the stable depth order.
                    continue;
                }
                const size_t behind = aBehind ? aIndex : bIndex;
                const size_t front = aBehind ? bIndex : aIndex;
                after[behind].push_back(front);
                ++incoming[front];
            }
        }

        // A footprint relation is a partial order, not a valid std::sort comparator.
        // Resolve it explicitly, choosing the original depth/sequence order for ties.
        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> ready;
        for (size_t i = 0; i < count; ++i)
        {
            if (incoming[i] == 0)
            {
                ready.push(i);
            }
        }
        std::vector<bool> emitted(count, false);
        std::vector<DrawEntry> ordered;
        ordered.reserve(count);
        while (ordered.size() < count)
        {
            if (ready.empty())
            {
                // Intersecting authored scenery can form a cycle. Break it in
                // deterministic depth order rather than omitting or flickering nodes.
                size_t next = 0;
                while (emitted[next])
                {
                    ++next;
                }
                ready.push(next);
            }
            const size_t next = ready.top();
            ready.pop();
            emitted[next] = true;
            ordered.push_back(entries[first + next]);
            for (size_t front : after[next])
            {
                if (!emitted[front] && --incoming[front] == 0)
                {
                    ready.push(front);
                }
            }
        }
        std::copy(ordered.begin(), ordered.end(), entries.begin() + first);
    }

    const std::vector<Actor*>& Ordered()
    {
        if (!orderDirty)
            return drawOrder;

        std::vector<DrawEntry> entries;
        entries.reserve(actors.size());
        for (const auto& entry : actors)
        {
            Actor* actor = entry.second.get();
            const auto p = actor->WorldPosition();
            const auto& foot = actor->footprint;
            const double x = p.x + foot.x, y = p.y + foot.y;
            // Allow the body to overlap the edge even when its foot point is outside.
            const double padding = actor->layer == ActorLayer::Character ? 32 : 0;
            entries.push_back({actor,
                               RenderLayer(actor->layer),
                               x + y + (foot.width + foot.depth) * .5 + actor->depthOffset,
                               x,
                               y,
                               x + foot.width,
                               y + foot.depth,
                               x - y - foot.depth - padding,
                               x + foot.width - y + padding});
        }
        std::sort(entries.begin(),
                  entries.end(),
                  [](const DrawEntry& a, const DrawEntry& b)
                  {
                      if (a.layer != b.layer)
                          return a.layer < b.layer;
                      return a.depth == b.depth ? a.actor->sequence < b.actor->sequence
                                                : a.depth < b.depth;
                  });
        OrderWorld(entries);
        drawOrder.clear();
        drawOrder.reserve(entries.size());
        for (const auto& entry : entries)
            drawOrder.push_back(entry.actor);
        orderDirty = false;
        return drawOrder;
    }
};
