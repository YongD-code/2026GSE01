#pragma once
#include "Renderer.h"
#include "ModelAsset.h"
#include <array>
#include <map>
#include <vector>

namespace ModelAssets
{
class Catalog;
}

// A skinned mesh asset, drawn by the existing character Actor. No independent scene ownership.
class CharacterModel
{
  public:
    enum class Motion
    {
        Idle,
        Walk,
        Run,
        Cast,
        Jump,
        TurnLeft,
        TurnRight,
        Attack,
        Count
    };

    using Matrix = std::array<float, 16>;
    using Pose = std::array<float, 10>;
    using Parts = std::array<int, 4>;

    // Only pose data belongs to an Actor. Geometry, shaders and targets are shared.
    struct State
    {
        const CharacterModel* owner = nullptr;
        std::vector<Pose> pose, transitionFrom;
        std::vector<Matrix> skin;
        Motion motion = Motion::Idle;
        float yaw = 2.35619449f, transition = 1, clock = -1;
        float turnAge = 1;
        Motion turn = Motion::TurnLeft;
        ModelAssets::State imported;
        bool namedClip = false;
        // Actor-local distance clock for authored quadruped stance/swing cycles.
        double gaitTravel = 0;
        float gaitPhase = 0;
        bool gaitInitialized = false, gaitRunning = false;
    };

    CharacterModel() = default;
    ~CharacterModel();
    CharacterModel(const CharacterModel&) = delete;
    CharacterModel& operator=(const CharacterModel&) = delete;

    bool Load(const wchar_t* path);
    bool LoadGltf(const wchar_t* path,
                  const ModelAssets::Catalog* parts = nullptr,
                  const std::map<std::string, std::string>* bindings = nullptr,
                  const std::vector<std::string>* ambientClips = nullptr);
    bool Play(State& state,
              const std::string& clip,
              bool loop = true,
              float speed = 1,
              float fadeSeconds = .12f);
    void ResumeLocomotion(State& state);
    bool Ready() const;
    void Reset(float facing = 2.35619449f);
    void Animate(float dt, Motion motion, float phase, float facing);
    bool Draw(Renderer& renderer, Point feet, Color tint);
    void Reset(State& state, float facing);
    void Animate(State& state, float dt, Motion motion, float phase, float facing);
    bool Draw(Renderer& renderer,
              Point feet,
              Color tint,
              const State& state,
              float height,
              const Parts* parts = nullptr);

  private:
    struct Joint
    {
        int parent = -1;
        Matrix inverseBind;
    };

    struct Clip
    {
        unsigned frames = 0;
        std::vector<Pose> poses;
    };

    std::vector<Joint> joints;

    struct MeshPart
    {
        int slot = -1, index = 0;
        unsigned first = 0, count = 0;
    };

    std::array<Clip, static_cast<size_t>(Motion::Count)> clips;
    std::vector<MeshPart> parts;
    State defaultState;
    float framing = 1;
    GLsizei vertexCount = 0;
    GLuint vao = 0, buffer = 0, meshProgram = 0, compositeProgram = 0;
    GLuint framebuffer = 0, colorBuffer = 0, depthBuffer = 0;
    GLuint resolveFramebuffer = 0, image = 0;
    GLint bonesUniform = -1, yawUniform = -1, viewportUniform = -1;
    GLint feetUniform = -1, tintUniform = -1, linearUniform = -1;
    GLint framingUniform = -1, drawScaleUniform = -1;
    GLint styleUniform = -1;
    std::shared_ptr<ModelAssets::Asset> imported;
    std::vector<ModelAssets::Primitive> importedDraws;
    std::vector<int> importedAmbient;
    std::vector<GLuint> textures;
    std::array<int, static_cast<size_t>(Motion::Count)> importedClips;
    GLuint paletteBuffer = 0;
    GLint objectUniform = -1, normalizationUniform = -1, skinnedUniform = -1;
    GLint materialUniform = -1, emissionUniform = -1, textureUniform = -1;
    GLint alphaUniform = -1, cutoffUniform = -1, unlitUniform = -1;
    GLint morphUniform = -1;
    GLint anchorUniform = -1;

    bool CreateTargets();
    bool CreatePrograms(bool gltf);
    void AnimateGltf(State& state, float dt, Motion kind, float phase);
    void DrawGltf(const State& state, const Parts* selection);
    void Release();
    Pose Sample(Motion motion, size_t bone, float phase) const;
    static Pose Blend(const Pose& a, const Pose& b, float weight);
    static Matrix Transform(const Pose& pose);
    static Matrix Multiply(const Matrix& a, const Matrix& b);
    void UpdateSkin(State& state) const;
};
