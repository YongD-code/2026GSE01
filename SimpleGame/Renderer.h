#pragma once
#include "Dependencies/glew.h"
#include <vector>
#include <string>
#include <map>
#include "PostProcessing.h"
#include <memory>

struct Color {
    float r, g, b, a;
    Color(float red, float green, float blue, float alpha = 1.f)
        : r(red), g(green), b(blue), a(alpha) {}
};
struct Point { float x, y; };

// Screen coordinates: top-left origin, pixels, positive Y points down.
// Shapes are batched in submission order, allowing painter's-order 2.5D scenes.
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    bool IsInitialized() const { return m_Initialized; }
    void Resize(int width, int height);
    void Begin(Color background);
    void Flush();
    void FinishScene();
    PostProcessSettings postProcess;
    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Ellipse(float x, float y, float rx, float ry, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Text(float x, float y, const std::string& text, Color color, bool large = false);
    void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);
private:
    struct Vertex { float x, y, r, g, b, a; };
    GLuint m_Program = 0, m_Buffer = 0, m_Array = 0;
    GLint m_Viewport = -1;
    int m_Width, m_Height;
    bool m_Initialized = false;
    bool m_InHDRScene = false;
    GLint m_LinearScene = -1;
    std::unique_ptr<PostProcessing> m_Post;
    std::vector<Vertex> m_Vertices;
    struct TextBitmap {
        int width = 0, height = 0, descent = 0;
        std::vector<unsigned char> pixels;
    };
    std::map<std::string, TextBitmap> m_TextCache;
};

