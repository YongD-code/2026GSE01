#pragma once
#include "Actor.h"
#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

// Owns all actors; parent links never own memory. Structural changes happen
// between traversal calls, never from an Actor callback.
class SceneGraph
{
  public:
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
        auto ordered = Ordered();
        // Ancestors update before descendants regardless of drawing layer.
        std::stable_sort(ordered.begin(),
                         ordered.end(),
                         [](Actor* a, Actor* b)
                         {
                             return HierarchyDepth(a) < HierarchyDepth(b);
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

    const std::vector<Actor*>& Ordered()
    {
        if (!orderDirty)
            return drawOrder;

        struct Entry
        {
            Actor* actor;
            double depth;
        };

        std::vector<Entry> entries;
        entries.reserve(actors.size());
        for (const auto& entry : actors)
        {
            Actor* actor = entry.second.get();
            const auto p = actor->WorldPosition();
            entries.push_back({actor, p.x + p.y + actor->depthOffset});
        }
        std::sort(entries.begin(),
                  entries.end(),
                  [](const Entry& a, const Entry& b)
                  {
                      if (a.actor->layer != b.actor->layer)
                          return a.actor->layer < b.actor->layer;
                      return a.depth == b.depth ? a.actor->sequence < b.actor->sequence
                                                : a.depth < b.depth;
                  });
        drawOrder.clear();
        drawOrder.reserve(entries.size());
        for (const auto& entry : entries)
            drawOrder.push_back(entry.actor);
        orderDirty = false;
        return drawOrder;
    }
};
