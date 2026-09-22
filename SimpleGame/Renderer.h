#pragma once
#include "Dependencies/glew.h"
#include "PostProcessing.h"
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>

struct Color
{
    float r, g, b, a;

    Color(float red, float green, float blue, float alpha = 1.f)
        : r(red), g(green), b(blue), a(alpha)
    {
    }
};

struct Point
{
    float x, y;
};

// Submission order is preserved, including transparent instances.
class Renderer
{
  public:
    Renderer(int width, int height);
    ~Renderer();

    bool IsInitialized() const
    {
        return m_Initialized;
    }

    void Resize(int width, int height);
    void Begin(Color background);
    void Flush();
    void FinishScene();
    bool BeginCachedMesh(const std::string& key, Point origin);
    void EndCachedMesh();
    void DrawCachedMesh(const std::string& key, Point origin);

    bool IsHDRScene() const
    {
        return m_InHDRScene;
    }

    PostProcessSettings postProcess;
    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Ellipse(float x, float y, float rx, float ry, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Text(float x, float y, const std::string& text, Color color, bool large = false);
    void Sprite(GLuint texture,
                float x,
                float y,
                float w,
                float h,
                float u0,
                float v0,
                float u1,
                float v1,
                Color tint,
                float emission = 0);
    void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);

  private:
    struct Vertex
    {
        float x, y, r, g, b, a;
    };

    struct Instance
    {
        float transform[4];
        float placement[4];
        float color[4];
        float uv[4];
        float material[4];
    };

    struct CachedMesh
    {
        std::vector<Vertex> vertices;
        int first = 0;
        std::uint64_t lastUsed = 0;
    };

    struct TextBitmap
    {
        int width = 0, height = 0, descent = 0;
        std::vector<unsigned char> pixels;
        GLuint texture = 0;
    };

    GLuint m_Program = 0, m_Buffer = 0, m_Array = 0, m_PoolBuffer = 0, m_PoolTexture = 0;
    GLint m_Viewport = -1, m_LinearScene = -1;
    int m_Width, m_Height, m_BatchVertices = 0;
    size_t m_PoolVertexLimit = 32768;
    bool m_Initialized = false, m_InHDRScene = false, m_Capturing = false;
    bool m_Batching = true;
    std::unique_ptr<PostProcessing> m_Post;
    std::vector<Vertex> m_Vertices;
    std::vector<Instance> m_Instances;
    std::vector<GLuint> m_Textures;
    std::map<std::string, CachedMesh> m_MeshCache;
    std::map<std::string, TextBitmap> m_TextCache;
    std::uint64_t m_Frame = 0;
    Point m_CaptureOrigin = {0, 0};
    std::string m_CaptureKey;
    std::chrono::steady_clock::time_point m_CaptureStart;
    void UploadPool();
    void InstallMesh(const std::string& key, std::vector<Vertex> vertices);
    void Submit(const std::string& key, Instance instance, GLuint texture = 0);
    void Templates();
};
