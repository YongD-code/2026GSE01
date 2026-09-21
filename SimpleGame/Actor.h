#pragma once
#include <functional>
#include <string>
#include <utility>

struct ActorPosition
{
    double x = 0, y = 0, z = 0;
};

enum class ActorLayer
{
    Terrain,
    Warning,
    World,
    Projectile,
    Atmosphere,
    Interface
};

// A scene node owns its local transform and common lifecycle state.
// Gameplay actors can override the hooks without depending on Prototype.
class Actor
{
    friend class SceneGraph;

  public:
    virtual ~Actor() = default;

    ActorPosition local;
    ActorLayer layer = ActorLayer::World;
    double depthOffset = 0;
    bool visible = true;
    bool enabled = true;

    const std::string& Id() const
    {
        return id;
    }

    const Actor* Parent() const
    {
        return parent;
    }

    ActorPosition WorldPosition() const
    {
        ActorPosition p = local;
        for (const Actor* node = parent; node; node = node->parent)
        {
            p.x += node->local.x;
            p.y += node->local.y;
            p.z += node->local.z;
        }
        return p;
    }

    void SetWorldPosition(ActorPosition p)
    {
        const ActorPosition origin = parent ? parent->WorldPosition() : ActorPosition{};
        local = {p.x - origin.x, p.y - origin.y, p.z - origin.z};
    }

    bool IsEnabled() const
    {
        for (const Actor* node = this; node; node = node->parent)
        {
            if (!node->enabled)
                return false;
        }
        return true;
    }

    bool IsVisible() const
    {
        for (const Actor* node = this; node; node = node->parent)
        {
            if (!node->visible || !node->enabled)
                return false;
        }
        return true;
    }

    virtual void Update(float)
    {
    }

    virtual void Draw()
    {
    }

    virtual bool PointerDown(int, int)
    {
        return false;
    }

  private:
    std::string id;
    Actor* parent = nullptr;
    unsigned long long generation = 0;
    unsigned long long sequence = 0;
};

// Adapter for existing gameplay/render systems. New concrete actors can derive
// directly from Actor; the graph does not need to know their type.
class FunctionActor : public Actor
{
  public:
    std::function<void(Actor&, float)> update;
    std::function<void(const Actor&)> draw;
    std::function<bool(int, int)> pointer;

    void Update(float dt) override
    {
        if (update)
            update(*this, dt);
    }

    void Draw() override
    {
        if (draw)
            draw(*this);
    }

    bool PointerDown(int x, int y) override
    {
        return pointer && pointer(x, y);
    }
};
