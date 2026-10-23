#pragma once
#include "ModelCatalog.h"
#include "Renderer.h"

class ModelLibrary
{
  public:
    ModelAssets::Catalog catalog;
    bool npcCatalogReady = false, spiritCatalogReady = false;
    bool npcDirect = false;

    void Load();
    bool DrawScenery(Renderer& renderer,
                     int kind,
                     int variant,
                     Point origin,
                     float width,
                     float depth,
                     float height,
                     float scale = 1);

  private:
    std::shared_ptr<ModelAssets::Asset> scenery;
    ModelAssets::State sceneryBind;
    std::string house, hall, rock;
    std::vector<std::string> trees;
    std::map<std::string, bool> scenerySupport;
};
