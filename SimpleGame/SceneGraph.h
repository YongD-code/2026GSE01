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
    }

    void BeginSync()
    {
        CheckMutation();
        ++generation;
    }

    void EndSync()
    {
        CheckMutation();
        std::vector<std::string> stale;
        for (const auto& entry : actors)
        {
            if (entry.second->generation != generation)
                stale.push_back(entry.first);
        }
        for (const auto& id : stale)
            Remove(id);
    }

    void Update(float dt)
    {
        Traversal guard(*this);
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
    }

    void Draw(bool interfacePass)
    {
        Traversal guard(*this);
        for (Actor* actor : Ordered())
        {
            if ((actor->layer == ActorLayer::Interface) == interfacePass && actor->IsVisible())
                actor->Draw();
        }
    }

    bool PointerDown(int x, int y)
    {
        Traversal guard(*this);
        const auto ordered = Ordered();
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

    std::vector<Actor*> Ordered() const
    {
        std::vector<Actor*> result;
        for (const auto& entry : actors)
            result.push_back(entry.second.get());
        std::sort(result.begin(),
                  result.end(),
                  [](Actor* a, Actor* b)
                  {
                      if (a->layer != b->layer)
                          return a->layer < b->layer;
                      const auto ap = a->WorldPosition(), bp = b->WorldPosition();
                      const double ad = ap.x + ap.y + a->depthOffset;
                      const double bd = bp.x + bp.y + b->depthOffset;
                      return ad == bd ? a->sequence < b->sequence : ad < bd;
                  });
        return result;
    }
};
