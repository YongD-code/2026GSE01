#pragma once
#include "Actor.h"
#include "CharacterModel.h"

// SceneGraph retains one animation state for each visible gameplay identity.
class ModelActor : public FunctionActor
{
  public:
    mutable CharacterModel::State modelState;
};
