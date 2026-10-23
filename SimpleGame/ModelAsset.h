#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace ModelAssets
{
constexpr size_t MaximumBones = 128;
using Matrix = std::array<float, 16>;
using Pose = std::array<float, 10>;

Matrix Identity();
Matrix Multiply(const Matrix& a, const Matrix& b);
Matrix Transform(const Pose& pose);
Pose Blend(const Pose& a, const Pose& b, float weight);
std::array<float, 3> Point(const Matrix& matrix, const float* point);

struct Vertex
{
    float position[3] = {}, normal[3] = {}, color[4] = {1, 1, 1, 1};
    float joints[4] = {}, weights[4] = {1, 0, 0, 0}, uv[2] = {};
    float morphPosition[4][3] = {}, morphNormal[4][3] = {};
};

struct Node
{
    std::string name;
    int parent = -1;
    Pose bind = {{0, 0, 0, 0, 0, 0, 1, 1, 1, 1}};
    Matrix matrix = Identity();
    bool matrixOnly = false;
    unsigned morphCount = 0;
    std::array<float, 4> morph = {};
};

struct Skin
{
    std::vector<int> nodes;
    std::vector<Matrix> inverseBind;
};

struct Material
{
    std::array<float, 4> color = {{1, 1, 1, 1}};
    std::array<float, 3> emission = {{0, 0, 0}};
    int texture = -1;
    bool unlit = false, doubleSided = false;
    int alphaMode = 0;
    float alphaCutoff = .5f;
};

struct Image
{
    std::vector<unsigned char> encoded;
    int wrapS = 10497, wrapT = 10497, minFilter = 9987, magFilter = 9729;
};

struct Primitive
{
    std::string name;
    int node = -1, skin = -1, material = 0;
    int slot = -1, index = 0;
    unsigned first = 0, count = 0;
};

struct Track
{
    enum class Path
    {
        Translation,
        Rotation,
        Scale,
        Morph
    };
    enum class Interpolation
    {
        Step,
        Linear,
        Cubic
    };
    int node = 0;
    Path path = Path::Translation;
    Interpolation interpolation = Interpolation::Linear;
    std::vector<float> times;
    std::vector<std::array<float, 4>> values;
};

struct Clip
{
    std::string name;
    float duration = 0;
    std::vector<Track> tracks;
};

struct State
{
    const void* owner = nullptr;
    int clip = -1;
    float seconds = 0, speed = 1, fade = 1, fadeSeconds = .12f;
    bool loop = true, finished = false;
    std::vector<Pose> pose, from;
    std::vector<std::array<float, 4>> morph, fromMorph;
    std::vector<Matrix> global;
    std::vector<int> layers;
    float ambientSeconds = 0;
};

// CPU-only, immutable after loading. Actor states never own or duplicate GPU geometry.
class Asset
{
  public:
    std::vector<Vertex> vertices;
    std::vector<Node> nodes;
    std::vector<size_t> order;
    std::vector<Skin> skins;
    std::vector<Material> materials;
    std::vector<Image> images;
    std::vector<Primitive> primitives;
    std::vector<Clip> clips;
    Matrix normalization = Identity();
    float framing = .7f;

    static std::shared_ptr<Asset> Load(const std::wstring& path, std::string& error);
    int FindClip(const std::string& name) const;
    void Reset(State& state) const;
    bool Play(State& state,
              const std::string& name,
              bool loop = true,
              float speed = 1,
              float fadeSeconds = .12f) const;
    void Update(State& state, float dt) const;
    void Sample(State& state, int clip, float seconds, float fade, bool inPlace = true) const;
    void Globals(State& state) const;
    void Palette(const State& state, int skin, std::vector<Matrix>& matrices) const;
    void Bounds(const State& state,
                const std::string& mesh,
                std::array<float, 3>& low,
                std::array<float, 3>& high) const;
};
} // namespace ModelAssets
