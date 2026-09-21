#include "stdafx.h"
#include "Renderer.h"
#include "ShaderFiles.h"
#include "Dependencies/freeglut.h"
#include <cmath>
#include <cstddef>
#include <iostream>
#include <utility>
#include <windows.h>

#pragma comment(lib, "gdi32.lib")

Renderer::Renderer(int width, int height) : m_Width(width), m_Height(height)
{
    m_Program = ShaderFiles::Program(L"Shaders/Geometry.vs", L"Shaders/Geometry.fs");
    if (!m_Program)
        return;
    m_MeshOffset = glGetUniformLocation(m_Program, "meshOffset");
    m_Viewport = glGetUniformLocation(m_Program, "viewport");
    glGenVertexArrays(1, &m_Array);
    glBindVertexArray(m_Array);
    glGenBuffers(1, &m_Buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindVertexArray(0);
    m_LinearScene = glGetUniformLocation(m_Program, "linearScene");
    m_Post.reset(new PostProcessing());
    m_Vertices.reserve(40000);
    m_Initialized = true;
    Resize(width, height);
}

Renderer::~Renderer()
{
    for (const auto& entry : m_MeshCache)
    {
        glDeleteBuffers(1, &entry.second.buffer);
        glDeleteVertexArrays(1, &entry.second.array);
    }
    m_Post.reset();
    if (m_Buffer)
        glDeleteBuffers(1, &m_Buffer);
    if (m_Array)
        glDeleteVertexArrays(1, &m_Array);
    if (m_Program)
        glDeleteProgram(m_Program);
}

void Renderer::Resize(int width, int height)
{
    m_Width = width > 0 ? width : 1;
    m_Height = height > 0 ? height : 1;
    glViewport(0, 0, m_Width, m_Height);
    if (m_Post)
        m_Post->Resize(m_Width, m_Height);
}

void Renderer::Begin(Color c)
{
    ++m_Frame;
    m_Vertices.clear();
    m_InHDRScene = m_Post && m_Post->Begin(postProcess.enabled);
    if (m_InHDRScene)
    {
        c.r = std::pow(c.r, 2.2f);
        c.g = std::pow(c.g, 2.2f);
        c.b = std::pow(c.b, 2.2f);
    }
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::Flush()
{
    if (m_Capturing || m_Vertices.empty() || !m_Initialized)
        return;
    glUseProgram(m_Program);
    glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glUniform1i(m_LinearScene, m_InHDRScene ? 1 : 0);
    glUniform2f(m_MeshOffset, 0, 0);
    glBindVertexArray(m_Array);
    glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    glBufferData(
        GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_Vertices.size()));
    glBindVertexArray(0);
    glUseProgram(0);
    m_Vertices.clear();
}

void Renderer::FinishScene()
{
    Flush();
    if (m_InHDRScene)
        m_Post->Finish(postProcess);
    m_InHDRScene = false;
}

bool Renderer::BeginCachedMesh(const std::string& key, Point origin)
{
    if (!m_Initialized || m_Capturing || m_MeshCache.find(key) != m_MeshCache.end())
        return false;
    Flush();
    m_Capturing = true;
    m_CaptureKey = key;
    m_CaptureOrigin = origin;
    return true;
}

void Renderer::EndCachedMesh()
{
    if (!m_Capturing)
        return;
    m_Capturing = false;
    // Bound GPU memory during infinite-world exploration. Evicted meshes rebuild on demand.
    if (m_MeshCache.size() >= 256)
    {
        auto oldest = m_MeshCache.begin();
        for (auto it = m_MeshCache.begin(); it != m_MeshCache.end(); ++it)
            if (it->second.lastUsed < oldest->second.lastUsed)
                oldest = it;
        glDeleteBuffers(1, &oldest->second.buffer);
        glDeleteVertexArrays(1, &oldest->second.array);
        m_MeshCache.erase(oldest);
    }
    for (Vertex& vertex : m_Vertices)
    {
        vertex.x -= m_CaptureOrigin.x;
        vertex.y -= m_CaptureOrigin.y;
    }
    CachedMesh mesh;
    mesh.count = static_cast<GLsizei>(m_Vertices.size());
    mesh.lastUsed = m_Frame;
    glGenVertexArrays(1, &mesh.array);
    glGenBuffers(1, &mesh.buffer);
    glBindVertexArray(mesh.array);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.buffer);
    glBufferData(
        GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindVertexArray(0);
    m_MeshCache.emplace(m_CaptureKey, mesh);
    m_Vertices.clear();
}

void Renderer::DrawCachedMesh(const std::string& key, Point origin)
{
    auto found = m_MeshCache.find(key);
    if (found == m_MeshCache.end() || !m_Initialized)
        return;
    Flush();
    CachedMesh& mesh = found->second;
    mesh.lastUsed = m_Frame;
    glUseProgram(m_Program);
    glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glUniform2f(m_MeshOffset, origin.x, origin.y);
    glUniform1i(m_LinearScene, m_InHDRScene ? 1 : 0);
    glBindVertexArray(mesh.array);
    glDrawArrays(GL_TRIANGLES, 0, mesh.count);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::Triangle(Point a, Point b, Point c, Color col)
{
    m_Vertices.push_back({a.x, a.y, col.r, col.g, col.b, col.a});
    m_Vertices.push_back({b.x, b.y, col.r, col.g, col.b, col.a});
    m_Vertices.push_back({c.x, c.y, col.r, col.g, col.b, col.a});
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color col)
{
    Triangle(a, b, c, col);
    Triangle(a, c, d, col);
}

void Renderer::Rect(float x, float y, float w, float h, Color c)
{
    Quad({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, c);
}

void Renderer::Ellipse(float x, float y, float rx, float ry, Color c)
{
    // Reuse the unit circle; only instance transforms vary per frame.
    static const std::vector<Point> circle = []()
    {
        std::vector<Point> points;
        for (int i = 0; i <= 24; ++i)
        {
            const float angle = i * 6.2831853f / 24;
            points.push_back({std::cos(angle), std::sin(angle)});
        }
        return points;
    }();
    for (int i = 0; i < 24; ++i)
    {
        Triangle({x, y},
                 {x + circle[i].x * rx, y + circle[i].y * ry},
                 {x + circle[i + 1].x * rx, y + circle[i + 1].y * ry},
                 c);
    }
}

void Renderer::Line(Point a, Point b, float w, Color c)
{
    float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy);
    if (length < .001f)
        return;
    float nx = -dy / length * w * .5f, ny = dx / length * w * .5f;
    Quad({a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}, c);
}

void Renderer::Text(float x, float y, const std::string& text, Color c, bool large)
{
    if (text.empty())
        return;
    const std::string key = (large ? "large:" : "normal:") + text;
    auto found = m_TextCache.find(key);
    if (found == m_TextCache.end())
    {
        // Decode UTF-8 explicitly, independent of the Windows system code page.
        int count = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0)
            return;
        std::wstring wide(count, L'\0');
        MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            text.c_str(),
                            static_cast<int>(text.size()),
                            &wide[0],
                            count);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc)
            return;
        HFONT font = CreateFontW(large ? -20 : -15,
                                 0,
                                 0,
                                 0,
                                 large ? FW_BOLD : FW_NORMAL,
                                 FALSE,
                                 FALSE,
                                 FALSE,
                                 DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH,
                                 L"Malgun Gothic");
        if (!font)
        {
            DeleteDC(dc);
            return;
        }
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE size = {};
        TEXTMETRICW metrics = {};
        if (!GetTextExtentPoint32W(dc, wide.c_str(), count, &size) ||
            !GetTextMetricsW(dc, &metrics))
        {
            SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            return;
        }
        TextBitmap bitmap;
        bitmap.width = size.cx + 2;
        bitmap.height = metrics.tmHeight;
        bitmap.descent = metrics.tmDescent;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = bitmap.width;
        // Positive height gives bottom-up pixels, matching OpenGL's raster order.
        info.bmiHeader.biHeight = bitmap.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib)
        {
            SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            return;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, dib);
        PatBlt(dc, 0, 0, bitmap.width, bitmap.height, BLACKNESS);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        BOOL drawn = TextOutW(dc, 0, 0, wide.c_str(), count);
        GdiFlush();
        const unsigned char* source = static_cast<const unsigned char*>(bits);
        bitmap.pixels.resize(static_cast<size_t>(bitmap.width) * bitmap.height * 4);
        for (size_t i = 0; i < bitmap.pixels.size(); i += 4)
        {
            bitmap.pixels[i] = bitmap.pixels[i + 1] = bitmap.pixels[i + 2] = 255;
            bitmap.pixels[i + 3] = source[i];
        }
        SelectObject(dc, oldBitmap);
        SelectObject(dc, oldFont);
        DeleteObject(dib);
        DeleteObject(font);
        DeleteDC(dc);
        if (!drawn)
            return;
        // Bound memory use when future dialogue introduces many unique lines.
        if (m_TextCache.size() >= 128)
            m_TextCache.clear();
        found = m_TextCache.emplace(key, std::move(bitmap)).first;
    }
    Flush();
    const TextBitmap& bitmap = found->second;
    glUseProgram(0);
    glWindowPos2i(static_cast<int>(x), m_Height - static_cast<int>(y) - bitmap.descent);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelTransferf(GL_RED_SCALE, c.r);
    glPixelTransferf(GL_GREEN_SCALE, c.g);
    glPixelTransferf(GL_BLUE_SCALE, c.b);
    glPixelTransferf(GL_ALPHA_SCALE, c.a);
    glDrawPixels(bitmap.width, bitmap.height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.pixels.data());
    glPixelTransferf(GL_RED_SCALE, 1);
    glPixelTransferf(GL_GREEN_SCALE, 1);
    glPixelTransferf(GL_BLUE_SCALE, 1);
    glPixelTransferf(GL_ALPHA_SCALE, 1);
}

void Renderer::DrawSolidRect(
    float x, float y, float, float size, float r, float g, float b, float a)
{
    Rect(m_Width * .5f + x - size * .5f,
         m_Height * .5f - y - size * .5f,
         size,
         size,
         Color(r, g, b, a));
}
