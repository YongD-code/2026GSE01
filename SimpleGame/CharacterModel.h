#pragma once
#include "Renderer.h"
#include <array>
#include <vector>

// A skinned mesh asset, drawn by the existing character Actor. No independent scene ownership.
class CharacterModel
{
  public:
    enum class Motion
    {
        Idle,
        Walk,
        Run,
        Cast
    };

    CharacterModel() = default;
    ~CharacterModel();
    CharacterModel(const CharacterModel&) = delete;
    CharacterModel& operator=(const CharacterModel&) = delete;

    bool Load(const wchar_t* path);
    bool Ready() const;
    void Reset(float facing = 2.35619449f);
    void Animate(float dt, Motion motion, float phase, float facing);
    bool Draw(Renderer& renderer, Point feet, Color tint);

  private:
    using Matrix = std::array<float, 16>;
    using Pose = std::array<float, 10>;

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
    std::array<Clip, 4> clips;
    std::vector<Pose> pose, transitionFrom;
    std::vector<Matrix> skin;
    Motion motion = Motion::Idle;
    float yaw = 2.35619449f, transition = 1;
    GLsizei vertexCount = 0;
    GLuint vao = 0, buffer = 0, meshProgram = 0, compositeProgram = 0;
    GLuint framebuffer = 0, colorBuffer = 0, depthBuffer = 0;
    GLuint resolveFramebuffer = 0, image = 0;
    GLint bonesUniform = -1, yawUniform = -1, viewportUniform = -1;
    GLint feetUniform = -1, tintUniform = -1, linearUniform = -1;

    bool CreateTargets();
    void Release();
    Pose Sample(Motion motion, size_t bone, float phase) const;
    static Pose Blend(const Pose& a, const Pose& b, float weight);
    static Matrix Transform(const Pose& pose);
    static Matrix Multiply(const Matrix& a, const Matrix& b);
    void UpdateSkin();
};
