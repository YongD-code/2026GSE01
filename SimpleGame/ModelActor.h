#pragma once
#include "Actor.h"
#include "CharacterModel.h"

// SceneGraph retains one animation state for each visible gameplay identity.
class ModelActor : public FunctionActor
{
  public:
    mutable CharacterModel::State modelState;
};

// A service NPC remembers one customer visit across greeting and idle clips.
class StationaryNpcActor : public ModelActor
{
  public:
    bool attending = false;
    int workCycle = 0;
};
